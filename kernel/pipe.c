/*
/
  Copyright 2020
  Shangjin Li
  Todd Gilbert

  This file contains definitions for a pipe implementation.

/
*/
/**/
#include <kernel/pipe.h>
#include <kernel/kmalloc.h>
#include <kernel/console.h>
#include <kernel/keyboard.h>
#include <lib/errcode.h>
#include <lib/string.h>
/**/
/*
  ops for the pipe, used with sys_calls
*/
static struct file_operations kpipe_ops = {
  .read = &kpipe_read,
  .write = &kpipe_write,
  .close = &kpipe_close,
};
/**/
err_t kpipe_init(struct file *f, int fd_reader, int fd_writer) {
  struct kpipe *p;

  // alloc space for struct
  p = kpipe_allocate();

  if (p == NULL) {
    return ERR_NOMEM;
  }

  // locks
  spinlock_init(&p->pipe_lock, 0);
  // cv's
  condvar_init(&p->pipe_notempty_cv);
  condvar_init(&p->pipe_full_cv);

  // meta
  p->readers = 1;
  p->writers = 1;
  p->fd_reader = fd_reader;
  p->fd_writer = fd_writer;
  p->read_ofs = 0;
  p->write_ofs = 0;

  // set file ops
  f->f_ops = &kpipe_ops;
  f->kpipe = p;
  
  return ERR_OK;
}


/**/
struct kpipe* kpipe_allocate() {
  struct kpipe *p;
  // kmalloc returns kernel virtual address -> directly accessible 
  if ((p = kmalloc(sizeof(struct kpipe))) != NULL) {
      memset(p, 0, sizeof(struct kpipe));
  }
  return p;
}
/**/
void kpipe_close(struct file *f) {
  struct kpipe *p = f->kpipe;
  // this is called at instances when pipe has open ends
  spinlock_acquire(&p->pipe_lock);
  if (p->writers == 0 && p->readers == 0) {
    // deallocate pipe
    kfree((void*)p);
    f->kpipe = NULL;
  }
  spinlock_release(&p->pipe_lock);
}
/**/
ssize_t kpipe_read(struct file *f, void *buf, size_t count, offset_t *ofs) {
  // lock buffer
  spinlock_acquire(&f->kpipe->pipe_lock);

  // buffer empty?
  while ((f->kpipe->write_ofs - f->kpipe->read_ofs) == 0) {
    if (f->kpipe->writers == 0) {
      // and no more writers 
      spinlock_release(&f->kpipe->pipe_lock);
      return 0;  // EOF
    }
    condvar_wait(&f->kpipe->pipe_notempty_cv, &f->kpipe->pipe_lock);
  } 

  size_t to_read = count;
  // Loop inv:
  //   to_read > 0
  //   buffer !empty
  while (to_read > 0 && (f->kpipe->write_ofs - f->kpipe->read_ofs) != 0) {
    char c = f->kpipe->buffer[f->kpipe->read_ofs++ % PIPE_SZ];
    *((char*)buf++) = c;
    to_read--;
  }  // end-while

  // let any writers know space available
  if (count - to_read > 0) {
    condvar_signal(&f->kpipe->pipe_full_cv);
  }

  spinlock_release(&f->kpipe->pipe_lock);

  return count - to_read;
}
/**/
ssize_t kpipe_write(struct file *f, const void *buf, size_t count, offset_t *ofs) {
  // lock buffer
  spinlock_acquire(&f->kpipe->pipe_lock);

  // any listeners?
  if (f->kpipe->readers == 0) {
    spinlock_release(&f->kpipe->pipe_lock);
    return ERR_END;
  }

  // buffer full?
  while ((f->kpipe->write_ofs - f->kpipe->read_ofs) == PIPE_SZ) {
    condvar_wait(&f->kpipe->pipe_full_cv, &f->kpipe->pipe_lock);
  }

  // write bytes
  size_t written = 0;
  // Loop inv:
  //  written < count
  //  buffer !full
  while (written  < count && (f->kpipe->write_ofs - f->kpipe->read_ofs) != PIPE_SZ) {
    char c = ((char*)buf)[written];
    f->kpipe->buffer[f->kpipe->write_ofs++ % PIPE_SZ] = c;
    written++;
  }  // end-while

  // alert readers
  if (written > 0) {
    condvar_signal(&f->kpipe->pipe_notempty_cv);
  }

  spinlock_release(&f->kpipe->pipe_lock);

  return written;
}
/**/
void kpipe_ref_decrease(struct file *f, int fd) {
  struct kpipe *p = f->kpipe;
  if (p != NULL) {
    spinlock_acquire(&p->pipe_lock);
    if (fd == p->fd_reader) {
      int i = p->readers;
      p->readers = i -1;
    } else {
      int i = p->writers;
      p->writers = i - 1;
    }
    spinlock_release(&p->pipe_lock);
  }
}
/**/
void kpipe_ref_increase(struct file *f, int fd) {
  struct kpipe *p = f->kpipe;
  if (p != NULL) {
    spinlock_acquire(&p->pipe_lock);
    if (fd == p->fd_reader) {
      int i = p->readers;
      p->readers = i + 1;
    } else {
      int i = p->writers;
      p->writers = i + 1;
    }
    spinlock_release(&p->pipe_lock);
  }
}
/**/
/*EOF*/
