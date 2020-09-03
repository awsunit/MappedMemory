# Lab 3 Design Doc: Address Space Management

## Overview
The goal of this lab is to implement address space management in osv

**Topics Covered:**
1. Growing user stack on-demand
    - Objective:
    Allocate physical pages for additional stack pages on demand and correctly configure the page table to reflect the change
    - Cases to consider:
        1. On regular stack operations
        2. On direct memory reference to stack region
2. Create user-level heap
    - Objective: Properly set up the memory region range for dynamic heap growth using sbrk(), and correctly handles page fault from heap addresses to support memregion extend
    - Cases to consider:
        1. Increment
        2. Decrement
        3. 0 case
3. Copy-on-Write Fork
    - Objective: Improve the performance of fork by using a copy-on-write mechanism
    - Cases to consider (after fork):
        1. No one modifies
        2. last process modifies
        3. Modifies(not last process)
## In-depth Analysis and Implementation

### Growing Stack:
- Implement handle_page_fault() at `kernel/pgfault.c:handle_page_fault`
- On fault, verify: write is set (disallow user to read previously unaccessed data), fault_addr is valid for the stack region [USTACK_UPPERBOUND - 10*PAGE_SZ, USTACK_UPPERBOUND], and that present is not set
- Use pmem_alloc() to allocate a physical memory page, zero it by first converting the physical address to kernel virtual via kmap_p2v(paddr), then memset(). If any steps failed, free the pmem allocated and terminate the process.
- Use vpmap_map() with permission MEMPERM_URW to map the new page to pg_round_down(fault_addr). If failed, terminate the process.

##### Changes to other files
- Manually manipulate stack's memregion.start at the end of stack_setup() at `kernel/proc.c` so any address within [10 pages below USTACK_UPPERBOUND, USTACK_UPPERBOUND] is valid. Note: calling memregion_extend would require modifying that function's intended logic. As this is the start of a process, the region will be free (still assert).

#### Cases
    1. Regular stack operations
        Occurs when current_thread->tf->rsp == fault_addr, handle as described
    2. Direct Memory Access to stack region
        Terminate to prevent random memory access

### User-level heap:
1. sbrk
    - Implement sys_sbrk in `kernel/syscall.c`
    - Call memregion_extend() with the *struct memregion *heap* from *struct addrspace* of *struct proc*
    - Return old end if memregion_extend() returns ERR_OK, else return ERR_NOMEM
2. memregion extend
    - Implement memregion_extend in `kernel/mm/vm.c`
    - Verify region is not NULL
    - call spinlock_acquire on region's lock
    - Set old_bound to region->end
    - If size is 0
        - Return ERR_OK
    - If size is positive
        - Verify region.end + size does not overlap other regions using memregion_allocated(), if overlap found return *ERR_VM_BOUND*
        - Set region->end += size
        - Return ERR_OK
    - If size is negative
        - If region->end - region->start < |size|, return ERR_OK
        - Otherwise, set region->end -= size and return ERR_OK
    - Release region's lock at appropriate locations

##### Changes to other files
1. Modify handle_page_fault() in `kernel/pgfault.c:handle_page_fault` to support heap grow-on-demand
    - On fault, check if fault_addr is within heap region via as_find_memregion, comparing pointers
    - If yes, use pmem_alloc() to allocate a physical page, memset() to zero it, vpmap_map() map it to the page of the fault_addr, then vpmap_set_perm() to MEMPERM_URW
    - If any of the steps failed, free the allocated memory and return ERR_VM_BOUND 

#### Cases (sufficiently discussed above)

### Copy-on-write fork:
- Implement vpmap_cow_copy() at `arch/x86_64/kernel/mm/vpmap.c`

- For n pages starting from srcaddr, copy parent's page table to child usinf find_pte(...,0) on parent's entries, and find_pte(...,1) on child
    - Use vpmap_set_perm() to set page perm to MEMPERM_UR if current region is writable
    - Set the content of the newly allocated pte of child to that of parent's pte content
    - Use pmem_inc_refcnt() to increment ref count on the just copied page
- Use memregion_invalidate() to flush the TLB

##### Changes to other files
1. Modify handle_page_fault() at `kernel/pgfault.c:handle_page_fault` to support copy-on-write
    - On fault, if it is due to a user write to a present page that belongs to a memregion with write permission, do copy, else exit the process
    - Before copy, check current ref on page by calling paddr_to_page(fault_addr's physical address) to get *struct page*, if refcnt is 1, no need to copy, use vpmap_set_perm() to set perm to MEMPERM_URW, flush the TLB with vpmap_flush_tlb(), then continue execution
    - Copying
        - Do pmem_alloc() to allocate a page of physical memory, if failed, exit process
        - Copy the entire page where fault_addr is to the new page, remember to use KMAP_P2V()
        - Use vpmap_map() to map fault_addr to the new page with perm MEMPERM_URW
        - Decrement page ref count using pmem_dec_refcnt() on the old PPN
        - If any steps failed, free the allocated page and terminate the process
        - Flush the TLB with vpmap_flush_tlb()
        - Resume process execution
2. Change vpmap_copy() to vpmap_cow_copy() in memregion_copy_internal() at `kernel/mm/vm.c`

#### Cases
    1. No one modifies
        Since all pte of writable regions have been set to MEMPERM_UR, if nobody modifies, nothing extra needs to be done
    2. last process modifies
        Since current process is the last using the faulting page, setting the perm of the page to MEMPERM_URW is enough because no other process knows about the page
    3. Modifies(not last process)
        One process' modification should be invisible to the other. Making a copy of the faulting page and setting its perm to MEMPERM_URW allows the process that is trying to modify the shared page to modify its own copy of the page. This way, the other process will still have a reference to the readonly page, while current process get to make changes, and they are no longer sharing the same page.

#### Files/methods changed
   `kernel/proc.c: stack_setup`\
   `kernel/pgfault.c: handle_page_fault`\ 
   `kernel/syscall.c: sys_sbrk`\
   `kernel/mm/vm.c: memregion_extend, memregion_copy_internal`\
   `arch/x86_64/kernel/mm/vpmap.c: vmap_cow_copy`       

# Questions
How to determine a fault address is the result of a stack operation and not a random memory access. SOLVED