# Lab 4 Design Doc: Named-Shared Memory

## Overview
The goal of this lab is to implement name-shared memory in osv, modeled after Windows Memory-Mapped Files

**Topics Covered:**
1. Create/Destroy Shared Region
    - Objective: Creating a shared memory mapping object(uniquely identified by name, fixed size), that allows user program to map/unmap a set of memory pages into their address space; Destroying a shared memory mapping object will decrease the refcnt on that object, and free the object when refcnt is 0
    - Cases to consider:
        1. Duplicate names
        2. Race condition
2. Map/Unmap Shared Memory
    - Objective: Allow user process to map an established shared region into its address space by id. Each shared region will be represented as a new `struct memregion` in the process' a.s..
    - Cases to consider:
        1. Name is valid
        2. Name is invalid
3. Synchronization Primitive
    - Objective: Add syscalls for using the locks in user program

## In-depth Analysis and Implementation

### Setup syscalls
- Add syscalls necessary for the implementation of name shared memory inside `syscall-num.h` and `usyscall.S`
- Implement the following syscalls so they call into the appropriate kernel functions:
    1. sys_createSharedRegion()
    2. sys_destroySharedRegion()
    3. sys_createMapping()
    4. sys_destroyMapping()

### Create/Destroy Shared Region
- Implement createSharedRegion() and destroySharedRegion() in `kernel/fs/shmms.c`
- The kernel maintains a list of active regions, ctable, each node containing a *struct smemcontext* that describes the information about that region and contains backing *struct memstore*.
- Create Region
    - Acquire lock on ctable
    - Check (using getContextByHandle()) if the supplied name identifies a region that is active; if so, return **ERR_EXIST**
    - Allocate a new *struct smemcontext* in kernel memory
    - Initialize struct fields 
    - return **ERR_NOMEM** iff either of previous two steps failed
    - Append context to ctable
    - Release the lock on ctable
    - Return **ERR_OK**
- Destroy Region
    - Check if the region is currently mapped to the process' address space; if so, return **ERR_INVAL**
    - Acquire lock on ctable
    - Check if the supplied name identifies a region that is active; if not, return **ERR_NOTEXIST**
    - If refcnt is equal to 0
        - Free the allocated pages for that region
        - Remove the corresponding *struct smemcontext* from ctable
        - Free the *struct smemcontext*
    - Otherwise: wait until it is 0
    - Release lock on ctable
    - Return **ERR_OK**
#### Cases
    1. Duplicate Names
        - The first thing createSharedRegion does is to check for duplicates
    2. Race Condition
        - spinLock for ctable will be used to coordinate synchronization

### Map/Unmap Shared Memory
- Implement createMapping() and destroyMapping() in `kernel/fs/shmms.c`

- createMapping(char *name, void** region_beg)`
    - Check if region name is valid, if not, return ERR_NOTEXIST
    - Create a new memregion in the current process' address space, use the memstore of the region
        - Use as_map_memregion() with ADDR_ANYWHERE, memstore should use that of the particular region
        - Region size is given in *struct smemcontext*
        - Region should be shared
        - page faulting will handle virtual mapping of pages
        - If as_map_memregion() fails, return ERR_NOMEM
    - acquire ctable_lock
    - increase refcount
    - release lock
    - Output the starting address of the region to region_beg and return ERR_OK
    - Otherwise return ERR_NOMEM
    - Note: It is possible to map the same shared region multiple times
- Unmap
    - Check if region has been mapped, if not, return ERR_NOTEXIST
    - Use memregion_unmap() to unmap the particular region
    - Decrease the refcnt on the region's context struct
    - Signal wait_cv for the dec of refcnt
#### Cases
    1. Name is valid
        - Both map/unmap will not error if name is valid in the context of the functions

### Read - Do we need to add method to `struct memstore`?

### Write
- write(struct memstore*, paddr_t, offset_t)

### Synchronization

### Files to change
1. pgfault.h
    - if fault address within shared region:\
        a. lock mr->store->pgcache_lock\
        b. call pgcache_get_page(mr->store, address). Method handles page allocation upon a miss. Calls store->fillpage on the page, then inserts into cache, returns a `struct *page`  NOTE - method calls for offset, translation needed?\
        c. convert page to paddr, map into user space, return ERR_OK

2. syscall.c (enumerated sufficiently above.

### Questions
1. memstore->info just points to context, are we using store simply as a reverse mapping, i.e., not worrying about its methods so much?\
Answer: memstore is the structure ensuring that each accessor is touching the same physical pages.
2. How is a user going to read/write to region? I thought it would be something similar to pointer dereferencing to read, and assignment to write.
3. Why should a process wait to close the storage, why not the last user?