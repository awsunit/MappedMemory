# Lab 1: Interrupts and System Calls
## 

## Introduction
All of our labs are based on OSV. OSV is a new experimental
operating system kernel for teaching the principles and practice of operating systems.
OSV's baseline code is a complete, bootable operating system. It provides some simple system
calls that it is capable of running a minimal shell program. Your task in each lab is to make
OSV complete and also add a few functionalities.
**For this lab you do not need to worry about synchronization. There will only be one process.**

## Installing OSV
The files you will need for this and subsequent lab assignments in
this course are distributed using the Git version control system.
To learn more about Git, take a look at the
[Git user's manual](http://www.kernel.org/pub/software/scm/git/docs/user-manual.html),
or, if you are already familiar with other version control systems,
you may find this
[CS-oriented overview of Git](http://eagain.net/articles/git-for-computer-scientists/)
useful.

The course Git repository is on the [CSE GitLab](//www.cs.washington.edu/lab/gitlab).
Follow the instructions there to set up your ssh keys if you haven't.

You need to clone the course repository, by running the commands below. You need a x86_64 linux machine for the lab. 
We recommend using attu by logging into it remotely (`ssh attu.cs.washington.edu`), this is what we will grade all assignments on.
```
$ git clone git@gitlab.cs.washington.edu:osv-public/20wi.git osv
Cloning into osv...
$ cd osv
```

You want to create your own repository on gitlab instead of working directly on our git repository. To do so, create a new project on gitlab with `blank` template. Set visibility to private (we don't want students in other groups to see your code).

Let's say your project is called `<proj_name>`, in the osv directory.
```
$ git remote rename origin upstream
$ git remote add origin git@gitlab.cs.washington.edu:<uwid>/<proj_name>.git
$ git push -u origin --all
```
This generates a copy of our repository in your project. You need to add all the course staffs
as `Developer` in your repository, and add your team members as a Master. You can find our cs email on the course website.

If you are part of a team, then only one person should create the repo and perform the steps above. The other
team member should directly pull the newly created repo.
```
$ git clone git@gitlab.cs.washington.edu:<uwid>/<proj_name>.git
$ git remote add upstream git@gitlab.cs.washington.edu:osv-public/20wi.git
```
Before you start coding, add a git tag
```
$ git tag start_lab1
$ git push origin master --tags
```

Git allows you to keep track of the changes you make to the code. For example, if you are
finished with one of the exercises, and want to checkpoint your progress, you can git add 
changed files and then commit your changes by running:
```
$ git commit -m 'my solution for lab1'
Created commit 60d2135: my solution for lab1
 1 files changed, 1 insertions(+), 0 deletions(-)
```

You can keep track of your changes by using the `git diff` command. Running `git diff` will display
the changes to your code since your last commit, and `git diff upstream/master` will display the
changes relative to the initial code supplied. Here, `upstream/master` is the name of the git
branch with the initial code you downloaded from the course server for this assignment.

We have set up the appropriate compilers and simulators for you on attu. Run the following command each time you log into the attu server:
```
export PATH=/cse/courses/cse451/17au/bin/x86_64-softmmu:$PATH
```
or add it to your shell startup file (`~/.bashrc` for BASH). If you are working on your own machine, you’ll need to install the toolchains. Follow the instructions from the [software setup page](tools.md).

After you finish lab1, add another git tag
```
$ git tag end_lab1
$ git push origin master --tags
```

This will allow us to pull a version of your lab1 solution even when you start working on the next lab.

## Organization of source code

```
osv
├── arch(x86_64)      // all architecture dependent code for different architecture
│   └── boot          // bootloader code
│   └── include(arch) // architecture dependent header files (contain architecture specific macros)
│   └── kernel        // architecture dependent kernel code
│   └── user          // architecture dependent user code (user space syscall invocation code)
│   └── Rules.mk      // architecture dependent makefile macros
├── include           // all architecture independent header files
│   └── kernel        // header files for kernel code
│   └── lib           // header files for library code, used by both kernel and user code
├── kernel            // the kernel source code
│   └── drivers       // driver code
│   └── mm            // memory management related code, both physical and virtual memory
│   └── fs            // file system code, generic filesys interface (VFS)
│       └── sfs       // a simple file system implementation implementing VFS
│   └── Rules.mk      // kernel specific makefile macros
├── user              // all the source code for user applications
│   └── lab*          // tests for lab*
│   └── Rules.mk      // user applications makefile macros
├── lib               // all the source code for library code
│   └── Rules.mk      // user applications makefile macros
├── tools             // utility program for building osv
├── labs              // handouts for labs and osv overview
└── Makefile          // Makefile for building osv kernel
```

After compilation ('make'), a new folder `build` will appear, which contains the kernel and fs image and all the intermediate binaries (.d, .o, .asm).
 
## Part 1: Starting osv

The purpose of the first exercise is to introduce you to x86_64 assembly
language and the PC bootstrap process, and to get you started with
QEMU and QEMU/GDB debugging. You will not have to write any code
for this part of the lab, but you should go through it anyway for
your own understanding.

### Getting started with x86_64 assembly

The definitive reference for x86_64 assembly language programming is Intel’s
instruction set architecture reference is
[Intel 64 and IA-32 Architectures Software Developer’s Manuals](https://software.intel.com/en-us/articles/intel-sdm).
It covers all the features of the most recent processors that we won’t need in class but
you may be interested in learning about. An equivalent (and often friendlier) set of
manuals is [AMD64 Architecture Programmer’s Manual](http://developer.amd.com/resources/developer-guides-manuals/).
Save the Intel/AMD architecture manuals for later or use them for reference when you want
to look up the definitive explanation of a particular processor feature or instruction.

You don't have to read them now, but you'll almost certainly want
to refer to some of this material when reading and writing x86_64 assembly.

### Simulating the x86_64

Instead of developing the operating system on a real, physical
personal computer (PC), we use a program that faithfully emulates
a complete PC: the code you write for the emulator will boot on a
real PC too. Using an emulator simplifies debugging; you can, for
example, set break points inside of the emulated x86_64, which is
difficult to do with the silicon version of an x86_64.

In osv, we will use the [QEMU Emulator](http://www.qemu.org/), a
modern and fast emulator. While QEMU's built-in monitor
provides only limited debugging support, QEMU can act as a remote
debugging target for the GNU debugger (GDB), which we'll use in
this lab to step through the early boot process.

To get started, extract osv source code into your own directory, then type
`make` in the root directory to build osv you will start with.

Now you're ready to run QEMU, supplying the file `build/fs.img` and `build/osv.img`,
created above, as the contents of the emulated PC's "virtual hard
disk." Those hard disk images contain our boot loader `build/arch/x86_64/boot`
, our kernel `build/kernel/kernel.elf` and a list of user applications in `build/user`.

Type `make qemu` to run QEMU with the options required to
set the hard disk and direct serial port output to the terminal.
Some text should appear in the QEMU window:

```
E820: physical memory map [mem 0x126000-0x1FFE0000]
 [0x0 - 0x9FC00] usable
 [0x9FC00 - 0xA0000] reserved
 [0xF0000 - 0x100000] reserved
 [0x100000 - 0x1FFE0000] usable
 [0x1FFE0000 - 0x20000000] reserved
 [0xFFFC0000 - 0x100000000] reserved

cpu 0 is up and scheduling 
cpu 1 is up and scheduling 
OSV initialization...Done

$
```

Press Ctrl-a x to exit the QEMU virtual instance.

### GDB

GDB can be used as a remote debugger for osv. We have provided a gdbinit file for you to use as `~/.gdbinit`.
It connects to a port that qemu will connect to when running `make qemu-gdb` and loads in kernel symbols.
You can generate your `~/.gdbinit` using the following command.
```
cp arch/x86_64/gdbinit ~/.gdbinit
```

To attach GDB to osv, you need to open two separate terminals. Both of them should be in the osv root directory. In one terminal, type `make qemu-gdb`. This starts the qemu process and wait for GDB to attach. In another terminal, type `gdb`. Now the GDB process is attached to qemu. If you are using `attu`, note that there are actually several attus. Make sure both of your terminals are connected to the same physical machine (e.g., explicitly using `attu2.cs.washington.edu`).

In osv, when bootloader loads the kernel from disk to memory, the CPU operates in 32-bit mode. The starting point of the 32-bit kernel is in `arch/x86_64/kernel/entry.S`. `entry.S` setups 64-bit virtual memory and enables 64-bit mode. You don't need to understand `entry.S`. `entry.S` jumps to `main` function in `kernel/main.c` which is the starting point of 64-bit OS.


### Question #1:
After attaching the qemu instance to gdb, set a breakpoint at the entrance of osv by typing in "b main". You should see a breakpoint set at `main` in `kernel/main.c`.
Then type "c" to continue execution. OSV will go through booting and stop at `main`. Which line of code in `main` prints the physical memory table? (Hint: use the `n` comand)

### Question #2:
We can examine memory using GDB’s `x` command. The GDB manual has full details, but for now, it is enough to know that the command `x/nx ADDR` prints `n` words of memory at `ADDR`. (Note that both ‘x’s in the command are lowercase, the second x tells gdb to display memory content in hex)

To examine instructions in memory (besides the immediate next one to be executed, which GDB prints automatically), use the `x/i` command. This command has the syntax `x/ni ADDR`, where `n` is the number of consecutive instructions to disassemble and `ADDR` is the memory address at which to start disassembling.

Repeat the previous process to break at main, what's the memory address of `main`? Does GDB work with real physical addresses?


## Part 2: Memory Management
Please take a look at the [memory management in osv](memory.md). After answer the following questions:

### Question #3
Why does osv map kernel and user-application into the same address space? (instead of using the kernel address space when entering into kernel)

### Question #4
Why is the osv user malloc (`lib/malloc.c:malloc`) different from the osv kernel malloc (`kernel/mm/kmalloc.c:kmalloc`)? 
Why is the osv user printf (`lib/stdio.c:printf`) different from the osv kernel printf (`kernel/console.c:kprintf`)?

## Part 3: Kernel

### Starting the first user process

osv spawns the first user space program through the `proc_spawn`(`kernel/proc.c`) call in `kernel_init`. 
`proc_spawn` initializes a process struct and its address space, loads in its executable, sets up its stack and heap memory regions.
It then creates a kernel thread through `thread_create`(`kernel/thread.c`) to execute instructions for this program. `tf_proc` sets up
the thread's trapframe, which tells the thread what instruction to execute and what stack to use when it goes into user mode.

Every new process goes through either `proc_spawn` or `proc_fork`(which you will implement later).
The first user space program is `user/sh.c`, which outputs a shell prompt "$" and waits for user input.

```c
int main(int argc, char **argv)
{
    char cmdline[MAXLINE];
    while (1) {
        puts(prompt, 3);
        gets(cmdline, MAXLINE);
        eval(cmdline);
    }
    exit(0);
    return 0;
}

```
Later on, you can type in the name of different test cases to test your kernel.
To shutdown the kernel, just type `quit` in the shell.

## Part 4: System calls
osv has to support a list of system calls. Here is a list of system calls that are already implemented.

Process system calls:
- `int spawn(const char *args)`
  + creates a new process
- `int getpid()`
  + returns pid of process

File system system calls:
- `int link(const char *oldpath, const char *newpath)`
  + create a hard link for a file
- `int unlink(const char *pathname)`
  + remove a hard link.
- `int mkdir(const char *pathname)`
  + create a directory
- `int chdir(const char *path)`
  + change the current workig directory
- `int rmdir(const char *pathname)`
  + remove a directory

Utility system calls:
- `void meminfo()`
  + print information about the current process's address space
- `void info(struct sys_info *info)`
  + report system info


### Trap
osv uses software interrupts to implement system calls. When a user application needs to invoke a system call,
it issues an interrupt with `int 0x40`. System call numbers are defined in `include/lib/syscall-num.h`. 
When the `int` instruction is being issued, the user program is responsible to set the register `%rax` to be the chosen system call number.

The software interrupt is captured by the registered trap vector (`arch/x86_64/kernel/vectors.S`) and the handler in `arch/x86_64/kernel/vectors.S`
will run. The handler will reach the `trap` function in `arch/x86_64/kernel/trap.c` and the `trap` function to demux the interrupt to
`syscall` function implemented in `kernel/syscall.c`. `syscall` then demuxes the call to the respective handler in `kernel/syscall.c`.

### Implement system calls
Your task is to implement the listed system calls (listed below). You will need a simple understanding of the file system used in osv.
Unimplemented system calls will panic if they are called, as you implement each system call, remove the panic. 

#### File, file descriptor and inode
The kernel needs to keep track of the open files so it can read, write, and eventually close the
files. A file descriptor is an integer that represents this open file. Somewhere in the kernel you
will need to keep track of these open files. Remember that file descriptors must be reusable between
processes. File descriptor 4 in one process should be able to be different than file descriptor 4 in another
(although they could reference the same open file).

Traditionally the file descriptor is an index into an array of open files.

The console is simply a file (file descriptor) from the user application's point of view. Reading
from keyboard and writing to screen is done through the kernel file system call interface.
Currently read/write to console is implemented as hard coded numbers, but as you implement file descriptors, 
you should use stdin and stdout file structs as backing files for console reserved file descriptors (0 and 1).

### Question #5
What is the first line of c code executed in the kernel when there is an interrupt? To force an interrupt, perform a system call. 
Add a `getpid` call within `sh.c` and use gdb to trace through it with the `si` command. You can add `build/user/sh` as a symbol file in your gdbinit.
(Hint: cs register selects code segment, this changes upon mode change).

### Question #6
How large (in bytes) is a trap frame?

### Question #7
Set a breakpoint in the kernel implementation of a system call (e.g., `sys_getpid`) and continue
executing until the breakpoint is hit (be sure to call `getpid` within `sh.c`. Do a backtrace, `bt` in gdb. 
What kernel functions are reported by the backtrace when it reaches `sys_getpid`?

### Exercise

#### Hints:
- File descriptors are just integers.
- Look at already implemented sys calls to see how to parse the arguments. (`kernel/syscall.c:sys_read`)
- If a new file descriptor is allocated, it must be saved in the process's file descriptor tables. Similarly, if a file descriptor is released, this must be reflected in the file descriptor table.
- A full file descriptor table is a user error (return an error value instead of calling `panic`).
- A file system is already implemented. You can use fs_read_file/fs_write_file to read/write from a file. 
  You can use fs_open_file to open a file. If you decide to have multiple file descriptors referring to a single file struct, make sure to call `fs_reopen_file()` on the file each time.
  You can find information of a file in the file struct and the inode struct inside the file struct.
- For this lab, the TA solution makes changes to `kernel/syscall.c`, `kernel/proc.c` and `include/kernel/proc.h`.

#### What To Implement
1) File Descriptor Opening
```c
/*
 * Corresponds to int open(const char *pathname, int flags, int mode); 
 * 
 * pathname: path to the file
 * flags: access mode of the file
 * mode: file permission mode if flags contains FS_CREAT
 * 
 * Open the file specified by pathname. Argument flags must include exactly one
 * of the following access modes:
 *   FS_RDONLY - Read-only mode
 *   FS_WRONLY - Write-only mode
 *   FS_RDWR - Read-write mode
 * flags can additionally include FS_CREAT. If FS_CREAT is included, a new file
 * is created with the specified permission (mode) if it does not exist yet.
 * 
 * Each open file maintains a current position, initially zero.
 *
 * Return:
 * Non-negative file descriptor on success.
 * The file descriptor returned by a successful call will be the lowest-numbered
 * file descriptor not currently open for the process.
 * 
 * ERR_FAULT - Address of pathname is invalid.
 * ERR_INVAL - flags has invalid value.
 * ERR_NOTEXIST - File specified by pathname does not exist, and FS_CREAT is not
 *                specified in flags.
 * ERR_NOTEXIST - A directory component in pathname does not exist.
 * ERR_NORES - Failed to allocate inode in directory (FS_CREAT is specified)
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_NOMEM - Failed to allocate memory.
 */
sysret_t
sys_open(void *arg);
```

2) File Descriptor Reading
```c
/*
 * Corresponds to ssize_t read(int fd, void *buf, size_t count);
 * 
 * fd: file descriptor of a file
 * buf: buffer to write read bytes to
 * count: number of bytes to read
 * 
 * Read from a file descriptor. Reads up to count bytes from the current position of the file descriptor 
 * fd and places those bytes into buf. The current position of the file descriptor is updated by number of bytes read.
 * 
 * If there are insufficient available bytes to complete the request,
 * reads as many as possible before returning with that number of bytes. 
 * Fewer than count bytes can be read in various conditions:
 * If the current position + count is beyond the end of the file.
 * If this is a pipe or console device and fewer than count bytes are available 
 * If this is a pipe and the other end of the pipe has been closed.
 *
 * Return:
 * On success, the number of bytes read (non-negative). The file position is
 * advanced by this number.
 * ERR_FAULT - Address of buf is invalid.
 * ERR_INVAL - fd isn't a valid open file descriptor.
 */
sysret_t
sys_read(void *arg);
```

3) File Descriptor Writing
```c
/*
 * Corresponds to ssize_t write(int fd, const void *buf, size_t count);
 * 
 * fd: file descriptor of a file
 * buf: buffer of bytes to write to the given fd
 * count: number of bytes to write
 * 
 * Write to a file descriptor. Writes up to count bytes from buf to the current position of 
 * the file descriptor. The current position of the file descriptor is updated by that number of bytes.
 * 
 * If the full write cannot be completed, writes as many as possible before returning with 
 * that number of bytes. For example, if the disk runs out of space.
 *
 * Return:
 * On success, the number of bytes (non-negative) written. The file position is
 * advanced by this number.
 * ERR_FAULT - Address of buf is invalid;
 * ERR_INVAL - fd isn't a valid open file descriptor.
 * ERR_END - if fd refers to a pipe with no open read
 */
sysret_t
sys_write(void *arg);
```

4) Close a File
```c
/*
 * Corresponds to int close(int fd);
 * 
 * fd: file descriptor of a file
 * 
 * Close the given file descriptor.
 *
 * Return:
 * ERR_OK - File successfully closed.
 * ERR_INVAL - fd isn't a valid open file descriptor.
 */
sysret_t
sys_close(void *arg);
```

5) Duplicate a File Descriptor
```c
/*
 * Corresponds to int dup(int fd);
 * 
 * fd: file descriptor of a file
 * 
 * Duplicate the file descriptor fd, must use the smallest unused file descriptor.
 * Reading/writing from a dupped fd should advance the file position of the original fd
 * and vice versa. 
 *
 * Return:
 * Non-negative file descriptor on success
 * ERR_INVAL if fd is invalid
 * ERR_NOMEM if no available new file descriptor
 */
sysret_t
sys_dup(void *arg);
```

6) File Stat
```c
/*
 * Corresponds to int fstat(int fd, struct stat *stat);
 * 
 * fd: file descriptor of a file
 * stat: struct stat * pointer
 *
 * Populates the struct stat pointer passed in to the function.
 * Console (stdin, stdout) and all console dupped fds are not valid fds for fstat. 
 * Only real files in the file system are valid for fstat.
 *
 * Return:
 * ERR_OK - File status is written in stat.
 * ERR_FAULT - Address of stat is invalid.
 * ERR_INVAL - fd isn't a valid open file descriptor or refers to non file. 
 */
sysret_t
sys_fstat(void *arg);
```

## Testing
After you implement the system calls described above. You can go through `user/lab1/*` files and run individual test
in the shell program by typing `close-test` or `open-bad-args` and so on. To run all tests in lab1, run `python3 test.py 1` in the osv directory. The script relys on python 3.6.8 (the version on attu).

For each test passed, you should see a `passed <testname>` message. At the end of the test it will assign a score of the test run.
Note that when grade the lab, we will run tests multiple times.

### Question #8
For each member of the project team, how many hours did you spend on this lab?

## Handin
Create a file `lab1.txt` in the top-level osv directory with
your answers to the questions listed above.

When you're finished, create a `end_lab1` git tag as described above so we know the point at which you
submitted your code.

