# Lab 2 Design Doc: Multiprocessing

## Overview
The goal of this lab is to implement the features for multiprocessing

**Topics Covered:**
1. Synchronization issues<br/>
    - Objective:    
         Correctly coordinate threads to avoid any corruption of state or any kind of unexpected behavior    
    - Cases:
        1. parent calls before child exits
        2. parent waits after child exits
        3. parent exits without waiting for child
        4. concurrent attempts at reading|writing to pipe
        5. writing to pipe with no open reading FD's
        6. reading from pipe with no open writing FD's
2. Fork 
    - Create new process by duplicating the calling process
    - The new process is referred to as the child process
    - The calling process is referred to as the parent process
    - The child process is exactly the same as the parent process except
        1. The child has its own unique pid
    - The child inherits the following from the parent
        1. Opened file descriptors at time of fork
        2. Address space at time of fork
    - The child's pid is returned to the parent, 0 is returned to the child
3. Wait
    - A state change is considered to be: the child terminated
    - Wait for state changes in a child of the calling process
    - Save information about the child's state change
4. Exit
    - Terminate execution
    - Release kernel resources used by process
    - Return status value for the parent of the calling process
5. Pipe
    - open a file twice for reading and writing
    - update file's ops to ones which maintain atomic use of the file
    - allocate memory to represent a bounded buffer
    - populate output parameters with FD's   
6. Spawn with args
    - Setup the stack for the spawning process so that argc and argv can be used by the new process to retrieval start-up parameters

## In-depth Analysis and Implementation

### Fork: 
- Implement proc_fork() in `kernel/proc.c`
- A new process needs to be created through `kernel/proc.c:proc_init`, if failed, return **ERR_NOMEM** 
- Parent must copy its memory to the child via `kernel/mm/vm.c:as_copy_as`, if failed, return **ERR_NOMEM**  
- All the opened files must be duplicated in the new process. While copying, use `fs_reopen_file` to increment the count of the copied file descriptor that are not NULL
- Use `thread_create` to create a thread in the new process with priority = parent's priority, if failed, return **ERR_NOMEM**
- Use `list_append` to add the new thread to the child process's list of threads in *struct proc* of proc_init()
- Duplicate current thread (parent process)'s trapframe in the new thread (child process) by copying trapframe in *struct thread* of thread_current()
- Set rax of parent's trapframe to child's pid (get from proc_init()'s return struct), set rax of child's trapframe to 0
- Acquire ptable_lock with spinlock_acqure, then add the new process to the process table and release the lock
- Put the new thread in the scheduler queue using `sched_ready` on the new thread
- Save child's PID - options: *linkedlist* field in *struct proc* or utilizing *ptable* in `kernel/proc.c` 
- Important data structures: *struct proc*, *struct thread*, *struct addrspace*, *TCB*

### Wait
- Add an int field in *struct proc* in `include/proc.h` called exit_status with default value **STATUS_ALIVE**
- Add a *struct condvar* field in *struct proc* in `include/proc.h` called wait_cv. Initialize it in proc_init()
- Add a *struct spinlock* field in *struct proc* in `include/proc.h` called wait_lock. Initialize it in proc_init()
- Implement proc_wait() in `kernel/proc.c`
- Important data structure: *struct proc*
##### Synchronization Solutions
##### Parent wait before/after child exits
- If pid is not ANY_CHILD and current process has child, then check the proc_node list to find target pid. If not exists or doesn't have any child, return **ERR_CHILD**
- If pid is ANY_CHILD, select the first child process on the list
- Acquire wait_lock of the target process using spinlock_acquire
- In a while loop, check if target process' exit_status = **STATUS_ALIVE**, if so, call condvar_wait on the target's wait_cv to wait until change
- Upon exiting the while loop, fetch child exit status by accessing child *struct proc*'s exit_status. Store in int* status if not null
- Release the wait_lock with spinlock_release()
- Remove child's *struct proc* from parent's proc list, remove it from the process table (with lock), then free the child's *struct proc* with proc_free()
##### Parent exits without waiting for child
- In `user/init.c:main`, check current process' children for zombie and release their resources (*struct proc*)
- Recall that in the case of a parent exits before its child, the child will be adopted by init

### Exit
- Implement proc_exit() in `kernel/proc.c`
- Acquire wait_lock of the current process
- Set exit_status in *struct proc* of current_process()
- Release kernel resources: address space, kernel thread, and file table
  - osv handles address space via proc_exit() and thread via thread_exit() & thread_cleanup()
  - *struct proc* memory handled in `kernel/proc_wait` or `user/init.c:main`
  - call fs_close_file for all files in the file table
- Broadcast wait_cv
- Release wait_lock
- Important data structure: *struct proc*
##### Synchronization Solutions
##### Parent exits before child
- If current process has running children:\
        1. Make a running child become the child of init, refered now as new_child\
        2. For every remaining non-zombie child of parent, make new_child the parent of child\
        3. Free resources (*struct proc*) for all of parent's zombie children
- Else:\
        1. Free resources (*struct proc*) for all of parent's zombie children
##### Parent exits after child
- Free resources (*struct proc*) for all of parent's zombie children

### Pipe
- update *struct file* in `include/kernel/fs.h` to include a pointer to *struct kpipe*
- expecting arg is array, arg[0] is read, arg[1] is write
- return **ERR_INVAL** iff arg's address not valid
- Use `fs_open_file()` to open file (no name needed)
- call alloc_fd() twice, if file table full, return **ERR_NOMEM**
- put *struct file* ptr into empty file table slots, call slots x and y for read and write FDs, respectively
- modify *struct file_operations* to point to the correct functions (to support `fs_read_file()`, `fs_write_file()`), functions are `kpipe_write()`,`kpipe_close()`, and `kpipe_read()`, all located in `include/kernel/pipe.h` and corresponding c file, see section below for details
- initialize *struct kpipe* via `kpipe_init()`, this sets up memory used by pipe as buffer, amongst other variable initializations. This *struct kpipe* will exist in kernel memory. Iff call fails, return **ERR_NOMEM**
- write x and y into arg at appropriate locations
- return **ERR_OK**

#### Synchronization Solutions
##### Interprocess communication
- after `fork()` call, processes will not share address space
- normal file will not work in this case, as the processes files will be distinct after `fork()`
- Solved with a "buffer" allocated by `kmem_alloc()`which is accessed and maintained by the OS
##### Must allow for creation of multiple pipes
- create *struct pipe* on each call to sys_pipe
- struct maintains metadata about pipe, creates locks for handling, etc.
##### file descriptors are re-opened
- currently, `fs_reopen_file()` only increments f_ref field of file -> doesn't communicate if this is a reader or writer
- updating `sys_dup` would allow us to access pipe's metadata fields, updating our reader/writer lists
##### Attempt to read from empty pipe
- _buffer will be empty (read_ofs == write_ofs)
- call `condvar_wait()` on pipe_notempty_cv, thread paused
##### Attempt to read while pipe is being written
- `spinlock_acquire()` will have failed, thread paused
##### Attempt to read from a pipe with no open writing FD's
- pipe metadata will indicate there are no open writers
- iff _buffer contains data, reads are successful
- return **EOF** when _buffer empty
##### Attempt to write to full pipe
- _buffer full -> read_ofs - write_ofs == 1 || write_ofs - read_ofs == BUFSIZE - 1
- call `condvar_wait()` on pipe_full_cv, thread paused
##### Attempt to write while pipe is being read
- `spinlock_acquire()` will have failed, thread paused
##### Attempt to write to a pipe with no open reading FD's
- pipe's metadata will indicate there are no open readers
- return **ERR_PIPEDEAF**
#### Handling open readers/writers
- any method which calls `fs_reopen_file()` needs to check pipe
- if the fd == (fd_reader || fd_writer), the accompanying count in pipe needs to be incremented
- methods to make change in: `proc_exit()`, `proc_fork()`, `sys_close()`, `sys_dup()`
- add another *spinlock* kpipe_countlock to *struct kpipe*, for access to counter variables


### `include/kernel/pipe.h`
  - *struct kpipe* which contains the following fields:
    1. buffer, 512B's of `kmalloc()` memory, used as a bounded buffer
    2. pipe_lock, a *spinlock* for access to _buffer 
    3. pipe_notempty_cv *condvar* for signaling data is available to read, used with pipe_lock
    4. pipe_full_cv *condvar* for signaling that buffer has room to write, used with pipe_lock
    6. variables read_ofs and write_ofs, to track progress of readers/writers
    7. variables open_readers, open_writers, keeps track of users
    8. int fd_reader & fd_writer so that `close()` operation will adjust counts accordingly
  - method `kpipe_init()`:
    1. allocate a *struct pipe*, return ERR_NOMEM iff failed
    2. allocate _buffer, a 512B's region of memory, return ERR_NOMEM iff failed
    2. call `spinlock_init()` on pipe_lock
    3. call `condvar_init()` on pipe_notempty_cv and pipe_full_cv, *cond_var*'s used by x and y
    4. set write_ofs = read_ofs = 0
    5. set open_readers = open_writers = 1 
    6. sets fd_reader & fd_writer
  - `pipe_close()` method to be called by `f_ops->close()`, note that freeing of a *struct file* occurs in `fs_close_file()`\
    if this is the last reference to file method needs to deallocate:
    1. _buffer
    2. *struct pipe*  
    else this method:
    1. decrements count of this file (either fd_reader or fd_writer fields)
  - kpipe_read()
    1. call `spinlock_acquire()` with pipe_lock
    2. iff _buffer empty, call `condvar_wait()` on pipe_notempty_cv with pipe_lock
    3. read from _buffer into the *buf supplied by user in initial `read()` call
    4. increment read_ofs, modulo as needed
    5. call `condvar_signal()` on pipe_full_cv
    6. call `spinlock_release()` with pipe_lock
    7. return number of bytes read
  -  kpipe_write() (implementation very similiar, seems redundant to include, see files for more information

### Spawn with args

## Risk Analysis

## Unsures
#### Unanswered
    - what do i_nodes really do 
#### Answer's Found
    - handling open_file_table, assuming it will be just for loop, if file != NULL, calling close on the file. Table was probably malloc'd, so will need freeing afterwards
    - how to return back to the caller on fork, I remember it being mentioned in lecture, will just have to revisit that portion
    - how to create a file whose memory is actually located within the kernel
    - is there really no need to call a lock_destroy method?
    - Any reason not to have pipe's _buffer be a file opened/created from fs_open?
    - Would that take care of concurrency issues?
## Time Estimate (total between both parties, updated 1/26)
    - best: 30 hours
    - average: 40 hours
    - worst: 50 hours
## Plan
    1. Complete Design Document
    2. Implement logic of sys_fork() and proc_fork() 
    3. Implement logic of sys_wait() and proc_wait()
    4. Implement logic of sys_exit() and proc_exit()
    5. Add error checking for previous functions
    6. Implement logic of sys_pipe() and proc_pipe()
    7. Implement logiv of sys_spawn() and sys_spawn()
    8. Add error checking for remaining functions
    9. Test