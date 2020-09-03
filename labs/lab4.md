# Lab 4: Open-ended project
**Proposal Due: 2/28/20 at 11:59pm. <br>
Complete Lab Due: 3/13/20 at 11:59pm.**

No late days can be used for lab4. <br>
Lab grading will be done through in person interviews on March 12th and 13th. <br>
We will provide a sign up sheet for interview times and a list of questions you should be prepared to answer on your project.

## Overview
For this lab, you will pick a project of your choice to extend osv. We encourage you to take this time to
explore particular OS features that you find interesting. We have provided a list of project ideas for reference. 
You can pick one from the list or do anything else you wish. <br>

### Submission details

For project proposal, follow the [template](lab4proposal.md). Here is a sample proposal from [19au](https://courses.cs.washington.edu/courses/cse451/19au/sample_proposal.pdf).

When finished with your proposal, tag your repo with:
```
git tag lab4_proposal
```

When finished with your lab4, tag your repo with:
```
git tag end_lab4
```

Don't forget to push the tags!
```
git push origin master --tags
```

### Configuration
To pull new changes, run the command
```
git pull upstream master
```
and merge.

## Potential projects 

**User threading library**
- Allow applications to run many user threads on top of its kernel thread.
- To add user library files, create them under `lib/` and start your file with prefix `u`, so that the Makefile can link them only to user processes. For example, you can create `lib/uthreads.c`, `lib/usched.c` and so on.

**Kernel threading library**
- Expose kernel thread interface to user applications, allows an application to create multiple kernel threads.
- Requires adding system calls and letting a process to create kernel threads, let a kernel thread exit, and allow a kernel thread in the process to join another thread (wait for another thread to terminate).
- Once a process can have multiple kernel threads, it is necessary to re-examine `kernel/syscall.c` and `kernel/proc.c` to make sure they are safe in the new multithreaded process setting.

**Named shared memory**
- Add system calls to allow processes to create, map, unmap a shared memory region.
- Processes should be able to create a shared memory region with a given name and size. 
- Once a named shared memory is created, other processes can address the shared memory region by name, and map it into its own address space with a particular permission and unmap it from its address space.
- This is functionally similar to pipe, but communication through shared memory does not go through the kernel once the memory is mapped into processes' address spaces.

**Signals**
- A way for processes to send software interrupt to another process through the kernel.
- Define a set of signals to support, provide a default set of signal handlers, and add system calls
for a process to register their own signal handler with a specific signal.
- A process can send a signal to another process (`signal(signal number, pid)`), which should cause the signaled process to run the corresponding signal handler.

**Priority Scheduling**
- Implement a priority based scheduler.
- Detailed priority scheduling description from a different educational OS can be found [here](https://web.stanford.edu/class/cs140/projects/pintos/pintos_2.html) section 2.2.3 Priority Scheduling.

**Swap**
- When kernel runs out of physical pages while handling a user page fault, currently we just exit the process. This is not always desirable. Instead, you can make a physical page available by evict an in-used physical page (clear its page table mapping, and write it to disk), and give it to the faulting process. 
- When the evicted page is accessed later, you need to find its data, write it back to a physical page (if there is no available physical page, you need to evict again), and map it, so that user can still access its memory.
- [OSTEP textbook Chapter 21](http://pages.cs.wisc.edu/~remzi/OSTEP/vm-beyondphys.pdf)

**Additional useful system calls**
- Implement any useful system calls you like in osv, a minimum of 5.
- Newly added system calls must be nontrivial: providing a non-cow fork is trivial.
- If you are unsure about whether a system call is too trivial, send us an email. 

## Testing and hand-in
After implementing your proposed project, you need to create 10 tests under `user/lab4` folder
to test your project. Each test should print out `pass(test-name)` on success. 

When you're finished, create an `end_lab4` git tag as described above,
so we know the point at which you submitted your code.