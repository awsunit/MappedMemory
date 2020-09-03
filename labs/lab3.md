# Lab 3: Address Space Management
**Design Doc Due: 2/14/20 at 11:59pm. \
Complete Lab Due: 2/21/20 at 11:59pm.**

## Introduction
In this lab, we are going to cover address space management. 
We will also ask you to implement some common techniques to save memory.

osv uses address space and memory region to track information about a virtual address space. A virtual address space ranges 
from virtual address [0, 0xffffffffffffffff] on a 64-bit machine. A memory region tracks a contiguous region of memory within 
an address space. A memory region belongs to only one address space, an address space can have many memory regions.
The kernel sets up memory regions for each process and use them to track valid memory ranges for a process to access.
Each user process has a memory region for code/SDS, stack, and heap on start up. More details can be found [here](memory.md).

In addition to managing each process's virtual address space, the kernel is also responsible for setting up the address translation 
table (page table) which maps a virtual address to a physical address. Each virual address space has its own page table, which is why
when two processes access the same virtual address, they access different data. osv uses `struct vpmap` to represent the page table.
`include/kernel/vpmap.h` defines a list of operatiions on the page table. For example `vpmap_map` maps a virtual page to a physical page.

You will be interacting with all these abstractions in lab3.

### Submission details
Create a lab3design.md in labs/ and follow the guidelines on [how to write a design document](designdoc.md). 
When finished with your design doc, tag your repo with:
```
git tag lab3_design
```

When finished with your lab3, tag your repo with:
```
git tag end_lab3
```

Don't forget to push the tags!
```
git push origin master --tags
```

For reference, the staff solution for this lab has made changes to
- `kernel/syscall.c`
- `kernel/pgfault.c`
- `kernel/mm/vm.c`
- `kernel/proc.c`
- `arch/x86_64/kernel/mm/vpmap.c` 

### Configuration
To pull new changes, second lab tests and description, run the command
```
git pull upstream master
```
and merge.

### Exercise
After the merge, double check that your code still passes lab1 and lab2 tests.


## Part 1: Grow user stack on-demand

In lab2, you have set up the user stack with an initial page to store application arguments. Great! But what if a process wants to use more stack?
One option is to allocate more physical pages for stack on start up and map them into the process's address space.
But if a process doesn't actually use all of its stack, the allocated physical pages are wasted while the process executes.

To reduce this waste, a common technique is to allocate physical pages for additional stack pages on demand.
For this part of the lab, you will extend osv to do on-demand stack growth. 
This means that the physical memory of the *additional stack page* is allocated at run-time.
Whenever a user application issues an instruction that reads or writes to the user stack (e.g., creating a stack frame, 
accessing local variables), we grow the stack as needed. For this lab, you should limit your overall stack size to 10 pages total.

To implement on-demand stack growth, you will need to understand how to handle page faults.
A page fault is a hardware exception that occurs when a process accesses a virtual memory page without a valid page table mapping, 
or with a valid mapping, but where the process does not have permission to perform the operation.

On a page fault, the process will trap into the kernel and trigger the page fault handler.
If the fault address is within a valid memory region and has the proper permission, the page fault handler
should allocate a physical page, map it to the process's page table, and and resume process execution.
Note that a write on a read only memory permission is not a valid access and the calling process should terminate. 

osv's page fault handler is `kernel/pgfault.c:handle_page_fault`.
```c
    void
    handle_page_fault(vaddr_t fault_addr, int present, int write, int user) {
        if (user) {
            __sync_add_and_fetch(&user_pgfault, 1);
        }
        // turn on interrupt now that we have the fault address 
        intr_set_level(INTR_ON);
```
`fault_addr` is the address that was attempted to be accessed <br />
`present` is set if it was a page protection issue (fault address has a correpsonding physical page mapped, but permission is off). This is not set if the page is not present.<br />
More can be found [here](https://wiki.osdev.org/Exceptions#Page_Fault)
`write` is set if the fault occurred on a write.<br />
`user` is set if the fault occurred in user mode.<br />

To support stack growth, you should first expand the range of stack memory region so any address within [10 pages below USTACK_UPPERBOUND, USTACK_UPPERBOUND] is valid , and then implement the page fault handler. To avoid information leaking, you need to memset the allocated physical page to 0s. Note that you cannot directly
access physical memory, so you need to translate the physical address to a kernel virtual address using `kmap_p2v(paddr)` before you do the memset.

### Question #1:
When a syscall completes, user-level execution resumes with the instruction
immediately after the syscall.  When a page fault exception completes, where does
user-level execution resume?

### Question #2:
How should the kernel decide whether an unmapped reference is a normal stack
operation versus a stray pointer dereference that should cause the application to halt? 
What should happen, for example, if an application calls a procedure with a local variable that is an array
of a million integers?  

### Question #3:
Is it possible to reduce the user stack size at
run-time (i.e., to deallocate the user stack when a procedure with a
large number of local variables goes out of scope)?  If so, sketch how that
might work. (This is not part of the lab implementation, just a thought question)

### Exercise
Implement growing the user stack on-demand. Note that our test code
uses the system call `sysinfo` to figure out how many page faults have happened.


## Part 2: Create a user-level heap
After you have set up page fault handler to handle on-demand stack growth, you will now
support dynamic heap growth. Heap growth differs in that a process has to explicitly request
for more virtual address to be allocated to its heap. A process that needs more memory at runtime 
can call `sbrk` (set program break) to grow its heap size. The common use case is the situation where 
a user library routine, `malloc` in C or `new` in C++, calls `sbrk` whenever the application asks to allocate 
a data region that cannot fit on the current heap (e.g., if the heap is completely allocated due to prior calls to `malloc`).

If a user application wants to increase the heap size by `n` bytes, it calls `sbrk(n)`. `sbrk(n)` returns the OLD limit.
The user application can also decrease the amount of the heap by passing negative values to `sbrk`.
Generally, the user library asks `sbrk` to provide more space than immediately needed, to reduce the number of system calls.

When a user process is first spawned, its heap memory region is initialized to size 0, so the first call to `malloc`
always calls `sbrk`. osv needs to track how much memory has been allocated to each process's heap, and also extend/decrease
size of heap base on the process's request. To do so, you need  to implement (`kernel/mm/vm.c:memregion_extend`).

Once you have properly set up the memory region range for dynamic heap growth, you can handle page fault from heap address
similar to how you handle on-demand stack growth. osv internally allocates and frees user memory at page granularity, 
but a process can call `sbrk` to allocate/deallocate memory at byte granularity. 
The OS does this to be portable, since an application cannot depend on the machine adopting a specific page size. 

In user space, we have provided an implementation of `malloc` and `free` (in `lib/malloc.c`) that is going to use `sbrk`. 
After the implementation of `sbrk` is done, user-level applications should be able to call `malloc` and `free`.

### Question #4
Why might an application prefer using `malloc` and `free` instead of using `sbrk` directly?


### Exercise
Implement `sys_sbrk` and `memregion_extend`. Need to support increment, decrement, and 0 case.

#### What to Implement
```c
 /*
  * Corresponds to void* sbrk(int increment) 
  * Increase/decrement current process' heap size.
  * If user requests to decrement more than the amount of heap allocated, treat it as sbrk(0).
  *
  * Return:
  * On success, address of the previous program break/heap limit.
  * ERR_NOMEM - Failed to extend the heap segment.
  */
sysret_t
sys_sbrk(void *arg);
```

```c
/*
 * Extend a region of allocated memory by size bytes.
 * End is extended size and old_bound is returned.
 * Return ERR_VM_BOUND if the extended region overlaps with other regions in the
 * address space.
 */
err_t
memregion_extend(struct memregion *region, int size, vaddr_t *old_bound);
```


## Warning
At this point we will be editing fork. It is possible to get in a state where
no process will be able to start. We strongly suggest you checkpoint your code
in some way (using a git tag, or copying the code into a directory you won't touch).


## Part 3: Copy-on-write fork

In the rest of lab3, we study how to reduce osv's memory consumption. The first technique is to grow stack 
and heap on demand. The next optimization improves the performance of fork by using a copy-on-write mechanism.
Currently, `fork` duplicates every page of user memory in the parent process. Depending on the size of the parent process, 
this can consume a lot of memory and can potentially take a long time. Take a look at `as_copy_as`, 
`memregion_copy_internal`, and `vpmap_copy`. 

Here, we can reduce the cost of `fork` by allowing multiple processes to share the same physical memory, 
while at a logical level still behaving as if the memory was copied. As long as neither process modifies the memory, 
it can stay shared; if either process changes a page, a copy of that page will be made at that point (that is, copy-on-write).

When `fork` returns, the child process is given a page table that points to the same memory pages as the parent. 
No additional memory is allocated for the new process, other than to hold the new page table.
However, the page tables of both processes will need to be changed to be read-only.  That way, if either process
tries to alter the content of their memory, a trap to the kernel will occur, and the kernel can make a copy of the 
memory page at that point, before resuming the user code.

To do so, you will want to implement a `vpmap_cow_copy` function that is similar to `vpmap_copy` but instead 
of making a copy of the parent's physical page, you will update the parent's page table entry permission to read only, 
set the child's page table entry to the same content as parent, and increment the physical memory reference through 
`pmem_inc_refcnt` on the physical page. You can get the physical page address by applying `PPN()` macro on the 
content of a page table entry. Once you have the `vpmap_cow_copy` function, you can update `memregion_copy_internal` to
use the new function instead of `vpmap_copy`. Note that since copy-on-write fork changes the memory mapping permission
in the parent, you must call `memregion_invalidate` at the end to invalidate previously cached permission.

On a page fault, if the fault address is present, being written to, and the corresponding memregion has write permission,
then you know it is a copy-on-write page. From there you can allocate a physical page, copy the data from the copy-on-write
page, and let the faulting process start writing to that freshly-allocated page. Note that when you are no longer using 
the read only page, you should decrement its reference count using `pmem_dec_refcnt`. An optional optimizaion is to check the 
physical page's reference count and if you are the last reference, you can simply use that page as your "new" read/write page, 
instead of making a copy of the read only page.

A tricky part of the assignment is that, of course, a child process with a set of copy-on-write pages can fork another child.
Thus, any physical memory page can be shared by many processes. We will only test for functional correctness -- that the
number of allocated pages is small and that the child processes execute as if they received a complete copy of the parent's 
memory.

### Question #5:
The TLB caches the page table entries of recently referenced pages.  
When you modify the page table entry to allow write access, how can osv ensure that the TLB does not have a stale version of the cache? (Note: you need to do this when a memory mapping's permission is updated, check `include/kernel/vpmap.h` for relevant function).


### Exercise
Implement copy-on-write fork.

## Testing and hand-in
After you implement the system calls described above, test lab3 code by either running 
individual tests in osv shell, or run `python3 test.py 3`.

Running the tests from the _previous_ labs is a good way to boost your confidence
in your solution for _this_ lab.

## Tips
- To get the physical address from a page table entry, you can use `PPN(*pte)`.
- `PTE_FLAGS(*pte)`: get page table entry flags
- `paddr_to_page(paddr)`: get information about a physical address (paddr), a page struct contains reference count of the paddr
- `vpmap_lookup_vaddr()`: retrieves the physical address underlying the virtual address, if the virtual address is not mapped to a physical address, then the function returns an error that's not ERR_OK
- `kmap_p2v(paddr)`: gets the kernel virtual address of a physical address
- `kmap_v2p(vaddr)`: given the physical address of a kernel virtual address

### Question #6
For each member of the project team, how many hours did you spend on this lab?

Create a file `lab3.txt` in the top-level osv directory with your answers to the questions listed above.

When you're finished, create an `end_lab3` git tag as described above,
so we know the point at which you submitted your code.