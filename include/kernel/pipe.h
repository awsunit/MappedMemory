/*
/
  Copyright 2020
  Shangjin Li
  Todd Gilbert

  This file contains declarations for a struct which represents a pipe.

  A pipe allows interprocess communication, providing functions for reading and
  writing to the pipe, both by a parent process and its child processes. Note
  that forking will allow for all ancestors of parent to access pipe.

/
*/
/**/
#ifndef _PIPE_H_
#define _PIPE_H_
/**/
#include <stdint.h>
#include <kernel/types.h>
#include <kernel/synch.h>
#include <kernel/fs.h>
#include <lib/errcode.h>
/**/
#define PIPE_SZ 512
/**/
/* pipe definition */
struct kpipe {
  char buffer[PIPE_SZ];
  struct spinlock pipe_lock;
  struct condvar pipe_notempty_cv;
  struct condvar pipe_full_cv;
  // maintained un-modulo
  size_t read_ofs, write_ofs;
  uint64_t readers, writers;
  int fd_reader, fd_writer;
  struct file_operations *og_file_ops;
};
/**/
/*
  Initializes a pipe
  args:
    f - file with kpipe
    fd_reader - fd for read end of pipe
    fd_writer - fd for write end of pipe  
  requires: f->kpipe != NULL
  modifies: f
  returns:
    ERR_OK on success
    ERR_NOMEM on failure

*/
//err_t kpipe_init(struct file *reader, struct file *writer, int fd_reader, int fd_writer);
err_t kpipe_init(struct file *f, int fd_reader, int fd_writer);
/*
  Allocates space in kernel for pipe struct

  Returns: pointer to region allocated
*/
struct kpipe* kpipe_allocate();
/*
  Closes pipe, deallocates memory when appropriate
  args:
    f - file with kpipe
  requires: f->kpipe != NULL    
*/
void kpipe_close(struct file *f);
/*
  Attempts to read count bytes from p.buffer to buf.
  If buffer is empty, waits on writers.
  args:
    f - file with kpipe
    buf - location to read to
    count - number bytes to read
    ofs - dummy variable
  requires: f->kpipe != NULL    

  Returns number of bytes read, or EOF iff p.buffer is empty and p.writers == 0.
*/
ssize_t kpipe_read(struct file *f, void *buf, size_t count, offset_t *ofs);
/*
  Attempts to write count bytes from buf to p.buffer. If buffer full, waits on readers.
  args:
    f - file with kpipe
    buf - location to read to
    count - number bytes to read
    ofs - dummy variable
  requires:
    f->kpipe != NULL    
  modifies:
    f->kpipe

  Returns number of bytes written, or ERR_PIPEDEAF iff p.readers == 0
*/
ssize_t kpipe_write(struct file *f, const void *buf, size_t count, offset_t *ofs);
/*
  Decrements the appropriate reader/writer count for pipe

  args:
    f - file with kpipe
    fd - file descriptor associated with pipe
  requires:
    f->kpipe != NULL    
  modifies:
    f->kpipe
*/
void kpipe_ref_decrease(struct file *f, int fd);
/*
  Increases the appropriate reader/write count for pipe

  args:
    f - file with kpipe
    fd - file descriptor associated with pipe
  requires:
    f->kpipe != NULL    
  modifies:
    f->kpipe
*/
void kpipe_ref_increase(struct file *f, int fd);
/**/
#endif  // _PIPE_H_
/*EOF*/
