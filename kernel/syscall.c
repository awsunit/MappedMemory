#include <kernel/proc.h>
#include <kernel/thread.h>
#include <kernel/console.h>
#include <kernel/kmalloc.h>
#include <kernel/fs.h>
#include <kernel/pipe.h>
#include <kernel/vpmap.h>
#include <kernel/sfs.h>
#include <lib/syscall-num.h>
#include <lib/errcode.h>
#include <lib/stddef.h>
#include <lib/string.h>
#include <arch/asm.h>
#include <kernel/pipe.h>
#include <kernel/shmms.h>
// syscall handlers
static sysret_t sys_fork(void* arg);
static sysret_t sys_spawn(void* arg);
static sysret_t sys_wait(void* arg);
static sysret_t sys_exit(void* arg);
static sysret_t sys_getpid(void* arg);
static sysret_t sys_sleep(void* arg);
static sysret_t sys_open(void* arg);
static sysret_t sys_close(void* arg);
static sysret_t sys_read(void* arg);
static sysret_t sys_write(void* arg);
static sysret_t sys_link(void* arg);
static sysret_t sys_unlink(void* arg);
static sysret_t sys_mkdir(void* arg);
static sysret_t sys_chdir(void* arg);
static sysret_t sys_readdir(void* arg);
static sysret_t sys_rmdir(void* arg);
static sysret_t sys_fstat(void* arg);
static sysret_t sys_sbrk(void* arg);
static sysret_t sys_meminfo(void* arg);
static sysret_t sys_dup(void* arg);
static sysret_t sys_pipe(void* arg);
static sysret_t sys_info(void* arg);
static sysret_t sys_halt(void* arg);
static sysret_t sys_createSharedRegion(void* arg);
static sysret_t sys_destroySharedRegion(void* arg);
static sysret_t sys_createMapping(void* arg);
static sysret_t sys_destroyMapping(void* arg);
static sysret_t sys_lockSharedRegion(void* arg);
static sysret_t sys_unlockSharedRegion(void* arg);

extern size_t user_pgfault;
struct sys_info {
    size_t num_pgfault;
};

/*
 * Machine dependent syscall implementation: fetches the nth syscall argument.
 */
extern bool fetch_arg(void *arg, int n, sysarg_t *ret);

/*
 * Validate string passed by user.
 */
static bool validate_str(char *s);
/*
 * Validate buffer passed by user.
 */
static bool validate_bufptr(void* buf, size_t size);
static int alloc_fd();

static sysret_t (*syscalls[])(void*) = {
    [SYS_fork] = sys_fork,
    [SYS_spawn] = sys_spawn,
    [SYS_wait] = sys_wait,
    [SYS_exit] = sys_exit,
    [SYS_getpid] = sys_getpid,
    [SYS_sleep] = sys_sleep,
    [SYS_open] = sys_open,
    [SYS_close] = sys_close,
    [SYS_read] = sys_read,
    [SYS_write] = sys_write,
    [SYS_link] = sys_link,
    [SYS_unlink] = sys_unlink,
    [SYS_mkdir] = sys_mkdir,
    [SYS_chdir] = sys_chdir,
    [SYS_readdir] = sys_readdir,
    [SYS_rmdir] = sys_rmdir,
    [SYS_fstat] = sys_fstat,
    [SYS_sbrk] = sys_sbrk,
    [SYS_meminfo] = sys_meminfo,
    [SYS_dup] = sys_dup,
    [SYS_pipe] = sys_pipe,
    [SYS_info] = sys_info,
    [SYS_halt] = sys_halt,
    [SYS_createSharedRegion] = sys_createSharedRegion,
    [SYS_destroySharedRegion] = sys_destroySharedRegion,
    [SYS_map] = sys_createMapping,
    [SYS_unmap] = sys_destroyMapping,
    [SYS_lockSharedRegion] = sys_lockSharedRegion,
    [SYS_unlockSharedRegion] = sys_unlockSharedRegion,
};
/*
 *
  Helper function which evaulates a processes open files for passed fd
  
  args:
        fd - represents an open file
        p - current process
  returns:
           1 iff process has open file matching fd, 0 otherwise
 *
*/
static bool
validate_fd(int fd, struct proc *p)
{ 
  if (fd < 0 || fd >= PROC_MAX_FILE || p->files[fd] == NULL) {
    return False;
  }
  return True;
}

static bool
validate_str(char *s)
{
    struct memregion *mr;
    // find given string's memory region
    if((mr = as_find_memregion(&proc_current()->as, (vaddr_t) s, 1)) == NULL) {
        return False;
    }
    // check in case the string keeps growing past user specified amount
    for(; s < (char*) mr->end; s++){
        if(*s == 0) {
            return True;
        }
    }
    return False;
}

static bool
validate_bufptr(void* buf, size_t size)
{
    vaddr_t bufaddr = (vaddr_t) buf;
    if (bufaddr + size < bufaddr) {
        return False;
    }
    // verify argument buffer is valid and within specified size
    if(as_find_memregion(&proc_current()->as, bufaddr, size) == NULL) {
        return False;
    }
    return True;
}

static int
alloc_fd() {
    for (int i = 0; i < PROC_MAX_FILE; i++) {
        if ( proc_current()->files[i] == NULL) {
            return i;
        }
    }
    return -1;
}

// int fork(void);
static sysret_t
sys_fork(void *arg)
{
    struct proc *p;
    if ((p = proc_fork()) == NULL) {
        return ERR_NOMEM;
    }
    return p->pid;
}

// int spawn(const char *args);
static sysret_t
sys_spawn(void *arg)
{
    int argc = 0;
    sysarg_t args;
    size_t len;
    char *token, *buf, **argv;
    struct proc *p;
    err_t err;

    // argument fetching and validating
    kassert(fetch_arg(arg, 1, &args));
    if (!validate_str((char*)args)) {
        return ERR_FAULT;
    }

    len = strlen((char*)args) + 1;
    if ((buf = kmalloc(len)) == NULL) {
        return ERR_NOMEM;
    }
    // make a copy of the string to not modify user data
    memcpy(buf, (void*)args, len);
    // figure out max number of arguments possible
    len = len / 2 < PROC_MAX_ARG ? len/2 : PROC_MAX_ARG;
    if ((argv = kmalloc((len+1)*sizeof(char*))) == NULL) {
        kfree(buf);
        return ERR_NOMEM;
    }
    // parse arguments  
    while ((token = strtok_r(NULL, " ", &buf)) != NULL) {
        argv[argc] = token;
        argc++;
    }
    argv[argc] = NULL;

    if ((err = proc_spawn(argv[0], argv, &p)) != ERR_OK) {
        return err;
    }
    return p->pid;
}

// int wait(int pid, int *wstatus);
static sysret_t
sys_wait(void* arg)
{
    /* remove when writing your own solution */
    sysarg_t pid, statusPtr;
    kassert(fetch_arg(arg, 1, &pid));
    kassert(fetch_arg(arg, 2, &statusPtr));

    if (!validate_bufptr((int*)statusPtr, sizeof(int))) {
        return ERR_FAULT;
    }
    return proc_wait((int)pid, (int*)statusPtr);
}

// void exit(int status);
static sysret_t
sys_exit(void* arg)
{
    sysarg_t status;
    kassert(fetch_arg(arg, 1, &status));
    if (proc_current()->pid == 1) {
        shutdown();
    }
    //kprintf("shutting down with exit code: %d\n", (int)status);
    proc_exit((int)status);
    panic("syscall exit not implemented");
}

// int getpid(void);
static sysret_t
sys_getpid(void* arg)
{
    return proc_current()->pid;
}

// void sleep(unsigned int, seconds);
static sysret_t
sys_sleep(void* arg)
{
    panic("syscall sleep not implemented");
}

// int open(const char *pathname, int flags, fmode_t mode);
static sysret_t
sys_open(void *arg)
{
    sysarg_t path, flags, mode;
    kassert(fetch_arg(arg, 1, &path));
    kassert(fetch_arg(arg, 2, &flags));
    kassert(fetch_arg(arg, 3, &mode));

    if (!validate_str((char*)path)) {
        return ERR_FAULT;
    }

    int fd = alloc_fd();
    if (fd == -1) {
        return ERR_NOMEM;
    }

    err_t err;
    if ((err = fs_open_file((char*)path, (int)flags, (int)mode, (struct file**)&proc_current()->files[fd])) == ERR_OK) {
        return fd;
    }

    return err;
}

// int close(int fd);
static sysret_t
sys_close(void *arg)
{
    sysarg_t fd;
    kassert(fetch_arg(arg, 1, &fd));
    struct proc *process = proc_current();
    if (!validate_fd((int)fd, process)) {
        return ERR_INVAL;
    }
    // Lab 2 addition
    // This check would be better suited in fs_close_file but need access to fd
    kpipe_ref_decrease(process->files[(int)fd], (int)fd);
    fs_close_file(process->files[(int)fd]);

    //
    //process->files[(int)fd]->f_ops = &file_og_operations;

    process->files[(int)fd] = NULL;
    return ERR_OK;
}

// int read(int fd, void *buf, size_t count);
static sysret_t
sys_read(void* arg)
{
    sysarg_t fd, buf, count;

    kassert(fetch_arg(arg, 1, &fd));
    kassert(fetch_arg(arg, 3, &count));
    kassert(fetch_arg(arg, 2, &buf));

    if (!validate_bufptr((void*)buf, (size_t)count)) {
        return ERR_FAULT;
    }

    struct proc *p = proc_current();
    // does this process have a file for fd?
    if (!validate_fd((int)fd, p)) {
      return ERR_INVAL;
    }

    // read
    ssize_t bytesRead = fs_read_file(p->files[(int)fd], (void *)buf, (size_t)count,
                                     &(p->files[fd]->f_pos));
    return (sysret_t)bytesRead;

}

// int write(int fd, const void *buf, size_t count)
static sysret_t
sys_write(void* arg)
{
    sysarg_t fd, count, buf;

    kassert(fetch_arg(arg, 1, &fd));
    kassert(fetch_arg(arg, 3, &count));
    kassert(fetch_arg(arg, 2, &buf));

    if (!validate_bufptr((void*)buf, (size_t)count)) {
        return ERR_FAULT;
    }

    struct proc *p = proc_current();
    // does proc have a file for fd?
    if (!validate_fd((int)fd, p)) {
      return ERR_INVAL;
    }

    // write
    ssize_t bytesWrote = fs_write_file(p->files[(int)fd], (void *)buf, (size_t)count,
                                       &(p->files[(int)fd]->f_pos));
    return (sysret_t)bytesWrote;
}

// int link(const char *oldpath, const char *newpath)
static sysret_t
sys_link(void *arg)
{
    sysarg_t oldpath, newpath;

    kassert(fetch_arg(arg, 1, &oldpath));
    kassert(fetch_arg(arg, 2, &newpath));

    if (!validate_str((char*)oldpath) || !validate_str((char*)newpath)) {
        return ERR_FAULT;
    }

    return fs_link((char*)oldpath, (char*)newpath);
}

// int unlink(const char *pathname)
static sysret_t
sys_unlink(void *arg)
{
    sysarg_t pathname;

    kassert(fetch_arg(arg, 1, &pathname));

    if (!validate_str((char*)pathname)) {
        return ERR_FAULT;
    }

    return fs_unlink((char*)pathname);
}

// int mkdir(const char *pathname)
static sysret_t
sys_mkdir(void *arg)
{
    sysarg_t pathname;

    kassert(fetch_arg(arg, 1, &pathname));

    if (!validate_str((char*)pathname)) {
        return ERR_FAULT;
    }

    return fs_mkdir((char*)pathname);
}

// int chdir(const char *path)
static sysret_t
sys_chdir(void *arg)
{
    sysarg_t path;
    struct inode *inode;
    struct proc *p;
    err_t err;

    kassert(fetch_arg(arg, 1, &path));

    if (!validate_str((char*)path)) {
        return ERR_FAULT;
    }

    if ((err = fs_find_inode((char*)path, &inode)) != ERR_OK) {
        return err;
    }

    p = proc_current();
    kassert(p);
    kassert(p->cwd);
    fs_release_inode(p->cwd);
    p->cwd = inode;
    return ERR_OK;
}

// int readdir(int fd, struct dirent *dirent);
static sysret_t
sys_readdir(void *arg)
{
    panic("syscall readir not implemented");
}

// int rmdir(const char *pathname);
static sysret_t
sys_rmdir(void *arg)
{
    sysarg_t pathname;

    kassert(fetch_arg(arg, 1, &pathname));

    if (!validate_str((char*)pathname)) {
        return ERR_FAULT;
    }

    return fs_rmdir((char*)pathname);
}

// int fstat(int fd, struct stat *stat);
static sysret_t
sys_fstat(void *arg)
{
    sysarg_t fd, stat;

    // check args
    kassert(fetch_arg(arg, 1, &fd));
    kassert(fetch_arg(arg, 2, &stat));

    // check address of struct
    if (!validate_bufptr((void *)stat, sizeof(struct stat))) {
      return ERR_FAULT;
    }

    struct proc *p = proc_current();
    // does this proccess have a file corresponding to fd?
    // (0, 1) = console
    if (!validate_fd((int)fd, p) || (int)fd < 2) {
      return ERR_INVAL;
    }
    // make sure this isnt a dupped console file
    if ((p->files[(int)fd] == &stdin) || (p->files[(int)fd] == &stdout)) {
      return ERR_INVAL;
    }

    // file has inode
    struct inode *f_inode = p->files[fd]->f_inode;

    // check for lab2
    if (f_inode == NULL) {
      return ERR_INVAL;
    }

    // fill struct
    struct stat *s = (struct stat*)stat;
    s->ftype = f_inode->i_ftype;
    s->inode_num = f_inode->i_inum;
    s->size = f_inode->i_size;

    return ERR_OK;
}

// void *sbrk(size_t increment);
static sysret_t
sys_sbrk(void *arg)
{
  err_t err;
  sysarg_t size;
  vaddr_t vaddr;

  // get arg
  kassert(fetch_arg(arg, 1, &size));

  if ((err = memregion_extend(proc_current()->as.heap, (int)size, &vaddr)) != ERR_OK) {
    return err;
  }

  return vaddr;
}

// void memifo();
static sysret_t
sys_meminfo(void *arg)
{
    as_meminfo(&proc_current()->as);
    return ERR_OK;
}

// int dup(int fd);
static sysret_t
sys_dup(void *arg)
{
    sysarg_t fd;
    kassert(fetch_arg(arg, 1, &fd));
    struct proc *process = proc_current();
    if (!validate_fd((int)fd, process)) {
        return ERR_INVAL;
    }
    int i = alloc_fd();
    if (i == -1) {
        return ERR_NOMEM;
    }
    // Lab 2 addition
    kpipe_ref_increase(process->files[(int)fd], (int)fd);

    process->files[i] = process->files[(int)fd];
    fs_reopen_file(process->files[(int)fd]);
    return i;
}

// int pipe(int* fds);
static sysret_t
sys_pipe(void* arg)
{
  sysarg_t fd_reader, fd_writer;
  kassert(fetch_arg(arg, 1, &fd_reader));
  // do single buf assert with sizeof(int) * 2?
  if (!validate_bufptr((void *)fd_reader, sizeof(int))) {
    return ERR_FAULT;
  }
  kassert(fetch_arg(arg, 2, &fd_writer));
  if (!validate_bufptr((void *)fd_writer, sizeof(int))) {
    return ERR_FAULT;
  }
  // get fds
  int temp_rfd = alloc_fd();
  if (temp_rfd == -1) {
    return ERR_NOMEM;
  }

  // reader
  // allocate a file
  struct proc *process = proc_current();
  err_t err;
  process->files[temp_rfd] = fs_alloc_file();
  if (process->files[temp_rfd] == NULL) {
    return ERR_NOMEM;
  }
  // set up files goodies
  process->files[temp_rfd]->f_ref = 1;
  process->files[temp_rfd]->oflag = FS_RDONLY;
  process->files[temp_rfd]->f_inode = NULL;
  process->files[temp_rfd]->f_pos = 0;
  spinlock_init(&process->files[temp_rfd]->f_lock, 0);  
  // ops handled by pipe

  // writer
  int temp_wfd = alloc_fd();
  if (temp_wfd == -1) {
    return ERR_NOMEM;
  }
  process->files[temp_wfd] = fs_alloc_file();
  if (process->files[temp_wfd] == NULL) {
    // need to deallocate previous file
    fs_free_file(process->files[temp_rfd]);
    return ERR_NOMEM;
  }
  // set up goodies
  process->files[temp_wfd]->f_ref = 1;
  process->files[temp_wfd]->oflag = FS_WRONLY;
  process->files[temp_wfd]->f_inode = NULL;
  process->files[temp_wfd]->f_pos = 0;
  spinlock_init(&process->files[temp_wfd]->f_lock, 0);
 
  // set up pipe
  if ((err = kpipe_init(process->files[temp_wfd], temp_rfd, temp_wfd)) != ERR_OK) {
    // free files
    fs_free_file(process->files[temp_rfd]);
    fs_free_file(process->files[temp_wfd]);
    return ERR_NOMEM;
  }
  
  process->files[temp_rfd]->kpipe = process->files[temp_wfd]->kpipe;
  process->files[temp_rfd]->f_ops = process->files[temp_wfd]->f_ops;

  // output params
  *((int *)fd_reader) = temp_rfd;
  fd_reader += sizeof(int);
  *((int *)fd_reader) = temp_wfd;
  
  return ERR_OK;

}

// void sys_info(struct sys_info *info);
static sysret_t
sys_info(void* arg)
{
    sysarg_t info;

    kassert(fetch_arg(arg, 1, &info));

    if (!validate_bufptr((void*)info, sizeof(struct sys_info))) {
        return ERR_FAULT;
    }
    // fill in using user_pgfault 
    ((struct sys_info*)info)->num_pgfault = user_pgfault;
    return ERR_OK;
}

// void halt();
static sysret_t 
sys_halt(void* arg)
{
    shutdown();
    panic("shutdown failed");
}

static sysret_t 
sys_createSharedRegion(void* arg)
{
    sysarg_t name, size, flag;
    kassert(fetch_arg(arg, 1, &name));
    if (!validate_str((char*)name)) {
        return ERR_FAULT;
    }
    if (strlen((char*)name) >= MAX_NAME_LEN) {
        return ERR_INVAL;
    }
    kassert(fetch_arg(arg, 2, &size));
    kassert(fetch_arg(arg, 3, &flag));
    size = pg_round_up(size);
    return createSharedRegion((char*)name, size, flag);
}

static sysret_t 
sys_destroySharedRegion(void* arg)
{
    sysarg_t name;
    kassert(fetch_arg(arg, 1, &name));
    if (!validate_str((char*)name)) {
        return ERR_FAULT;
    }
    return destroySharedRegion((char*)name);
}

static sysret_t 
sys_createMapping(void* arg)
{
    sysarg_t name, reg_beg;
    kassert(fetch_arg(arg, 1, &name));
    if (!validate_str((char*)name)) {
        return ERR_FAULT;
    }
    kassert(fetch_arg(arg, 2, &reg_beg));
    if (!validate_bufptr((void*)reg_beg, sizeof(void*))) {
        return ERR_FAULT;
    }
    // make sure user isn't sneaking in an address
    *((vaddr_t*)reg_beg) = NULL;
    return createMapping((char*)name, (vaddr_t*)reg_beg, &proc_current()->as);
}

static sysret_t 
sys_destroyMapping(void* arg)
{
  sysarg_t name, vaddr;
    kassert(fetch_arg(arg, 1, &name));
    if (!validate_str((char*)name)) {
      return ERR_FAULT;
    }
    kassert(fetch_arg(arg, 2, &vaddr));

    // 3/6
    // vaddr checked in method
    return destroyMapping((char*)name, (vaddr_t)vaddr, &proc_current()->as);
}

static sysret_t 
sys_lockSharedRegion(void* arg)
{
    sysarg_t name;
    kassert(fetch_arg(arg, 1, &name));
    if (!validate_str((char*)name)) {
      return ERR_FAULT;
    }
    //    return lockRegion((char*)name); 3/6
    return lockRegion((char*)name, &proc_current()->as);
}

static sysret_t 
sys_unlockSharedRegion(void* arg)
{
    sysarg_t name;
    kassert(fetch_arg(arg, 1, &name));
    if (!validate_str((char*)name)) {
      return ERR_FAULT;
    }
    //    return unlockRegion((char*)name); 3/6
    return unlockRegion((char*)name, &proc_current()->as);
}

sysret_t
syscall(int num, void *arg)
{
    kassert(proc_current());
    if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        return syscalls[num](arg);
    } else {
        panic("Unknown system call");
    }
}

