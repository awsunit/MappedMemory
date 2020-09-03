#ifndef _SHMMS_H_
#define _SHMMS_H_

#include <kernel/memstore.h>
#include <kernel/list.h>

#define MAX_NAME_LEN 128

// Flags
#define REG_ANY 0           // Any region(mapped or unmapped)
#define REG_MAPPED 1        // Mapped regions only

// Region-specific flags
#define RS_DEFAULT 0x0
#define RS_PERSIST 0x100

// Errors
#define ERR_MAPPED -1
#define ERR_PERSIST -2

typedef int rs_flag;

// Kernel keep track of a list of this
// Each represents an active shared region
//  uniquely identified by name
struct smemcontext {
    char name[MAX_NAME_LEN];    // Unique identifier of the region
    struct memstore* store;     // Memory backing store
    struct spinlock m_lock;     // Lock for smemcontext
    Node ctable_node;           // List node for ctable
    tid_t lock_by;              // ThreadID of the locking thread
    int size;                   // Size of region in # pages
    int refcnt;                 // Current reference count
    rs_flag flag;               // flag to keep s.m. open
};

// used for store->rmap
struct pid2mem {
  Node node;
  struct addrspace *as;
  struct memregion *mr;
};

void smem_sys_init();

// Create a shared memory region that spans n pages
err_t createSharedRegion(char* name, int n, rs_flag flag);

/*
    Destroy a shared region by name
    If region has mapping, return ERR_MAPPED; ERR_OK o.t.w
*/
err_t destroySharedRegion(char* name);

// Map the given shared memory region into current process address space
//  region_beg will be filled with the virtual address of beginning of region
err_t createMapping(char* name, vaddr_t* region_beg, struct addrspace *as);

// Unmap the given shared memory region from current address space
err_t destroyMapping(char* name, vaddr_t vaddr, struct addrspace *as);

/*
    Unmap all shared region that has been mapped to this address space
    req: caller holds as->lock
*/
void unmapAll(struct memregion *region);

/*
    Lock the given region, will block until return
    Pre:
        Region must be mapped
    Return:
        ERR_OK if lock is successful
        ERR_NOTEXIST if region does not exist/unmapped
*/
err_t lockRegion(char* name, struct addrspace *as);

/*
    Unlock the given region
    Pre:
        Region must be mapped
    Returns:
        ERR_OK if unlock success
        ERR_NOTEXIST if region does not exist/unmapped
        ERR_INVAL if region has been locked by a different thread
*/
err_t unlockRegion(char* name, struct addrspace *as);

/*
    Map the context to the address space
*/
void addContextRef(struct smemcontext* ctx, vaddr_t start, struct addrspace* as, struct memregion* mr);

/*
 * Memstore backed by shared memory.
 */

/*
 * Allocate a shared memory memstore. TODO: parameters
 * Return NULL if failed to allocate.
 */
struct memstore *shmms_alloc(struct smemcontext* context);

/*
 * Free a shared memory memstore.
 */
void shmms_free(struct memstore *store);

#endif /* _SHMMS_H_ */
