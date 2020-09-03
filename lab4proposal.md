# Lab 4 Project Proposal
This document will cover the design for named shared memory in osv

## Overview 
We have decided to implement Named-Shared memory, modeled after Windows' Memory-Mapped Files. These memory regions woud allow for efficient access to large amounts of data, too big for traditional pipes.
We will implement functions to allow user program to create, map, synchronize, and destroy shared memory pages, as detailed below.

## Initial Risk Analysis/Potential Problems
1. Cow fork may be problematic, how do we distinguish between native pages and shared ones?\
    A: Shared pages will belong to a shared memregion. When cow-fork, ignore the pages in such region.
2. Which region should shared pages be mapped to? If it is the heap, what happen when a user attempts to free it?\
    A: The region can be independent of stack/heap. Inside the initial call to CreateRegion(), the user will pass in the requested byte size. The system can choose to allocate more space than neccessary, if desired. Calls to CreateRegion return 'handles' which can then be used to map the memory into user space. The mapping function returns a pointer to the region mapped. 
3. Some sort of synchronization primitive will be necessary to allow multi-process manipulation of data\
    A: Spinlock can be used in Read/Write methods.
4. Should each process have own unique file mapping object or share single instance?\
    A: A single FMO corresponding to a single shared memory region sounds preferable.
5. Is an internal descriptor table necessary?\
    A: Yes, to track each instance of shared memory a process has access to.
6. How to ensure shared pages get cleaned up when no one is using it anymore? Should OSV deal with more than bare minimum of allocating/deallocating pages?\
7. Does a user have the ability to destroy regions that still has other, active users?\
    A: No, at least no valid reason comes to mind.
8. How much space should we grant a user? Should a limit be set?\
    A: A limit of 10 pages sounds appropriate. 
9. How to ensure that malicious/random users do not have access to the region?\
    A: Flags are used with FMO's and views. During fork, when copying regions, permissions will dictate what child inherits. 
10. What is the smallest unit of allocation for shared memory?\
    A: A page will be the smallest unit of allocation.
11. What happen when a user write across the boundary of the shared region?\
    A: The user is likely to trigger a page fault, if the page is valid, write will propagate across the region; if not. user process will terminate. Either way, user can only modify shared memory to an extend.
12. How to ensure memory is free'd?\
    A: The internal descriptor table entries will keep track of regions location/active users. During process_exit() this table will be swept, freeing the physical memory iff active_users == 0. 

## Helpful resources you've found to research topic
https://docs.microsoft.com/en-us/windows/win32/dlls/using-shared-memory-in-a-dynamic-link-library
https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfilemappinga
https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile
https://www.spiria.com/en/blog/desktop-software/sharing-data-between-processes-windows-named-pipe-and-shared-memory/
https://docs.microsoft.com/en-us/previous-versions/ms810613(v=msdn.10)?redirectedfrom=MSDN
https://www.installsetupconfig.com/win32programming/dynamiclinklibrarydll9_8.html

## Minimum Viable Goal

#### Users
- A initial user should be able to create the associated memory region via a function call, currently named CreateRegion(), this function returns a handle for mapping the shared memory.
- The CreateMapping() function should map the shared memory into user address space using the CreateRegion() handle. This returns a pointer to the start of the region. 
- Subsequent users desiring use of the region may gain a handle via a call to CreateMapping().
- Access to manipulate the memory region, tentatively called MMF_Read() and MMF_Write().
- Communication to notify other processes of data manipulation.
- Control to terminate association with the region, via a function, currently named FileMappingDecRef(). This includes the ability destroy shared memory pages. Note that this could be handled via the FileMappingDecRef() call when users == 0. 
- process inheritance approval or denial

#### OSV
- Track allocated memory-mapped pages.

## Stretch Goal
- dynamic allocation which increases size of shared region as needed.
- memory-mapped files

## Definitions
shared memory mapping object: used to create association for shared memory between a user's virtual address space and the physical memory which backs the object. Calls on this will trap to the kernel.\
memory view: portion of the user's address space that the region is mapped to. Note this could be an implicit structure, i.e., pointer. 
