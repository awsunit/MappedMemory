#include <kernel/vm.h>
#include <kernel/pmem.h>
#include <kernel/vpmap.h>
#include <kernel/shmms.h>
#include <kernel/pgcache.h>
#include <kernel/console.h>
#include <kernel/proc.h>
#include <lib/errcode.h>
#include <lib/stddef.h>
#include <lib/string.h>
#include <kernel/thread.h>
#include <arch/mmu.h>
// Get context from ctable by given name
// Pre: hold ctable_lock
static err_t getContextByHandle(char* name, int mapped, struct smemcontext** context, struct addrspace *as);

/*  Initialize context based on given name and size
    Return ERR_OK when success
*/
static err_t contextInit(struct smemcontext* context, char* name, int size, rs_flag flag);

/*
    Helper method to destroy mapping by context
*/
static err_t destroyMapping_Internal(struct smemcontext* ctx, struct memregion *mr, int forceDestroy);

/*
    Destroys context itself iff refcnt == 0
    requires: caller holds ctable_lock
*/
static err_t destroySharedRegion_Internal(struct smemcontext *context, int forceDestroy);

/*
  Searches context->store->rmap for a region associated with this as
*/
static struct pid2mem* find_remove_rmapping(struct smemcontext *context, struct addrspace *as, vaddr_t vaddr);

/*
   Sets up mappings, handles errors
 */
static err_t createMapping_Internal(char *name, vaddr_t* start, struct addrspace* as, struct smemcontext *context, struct memregion* mr); 

// Print useful info about current regions
static void printSharedRegions();

/*
    Allocates nodes for reverse mapping's
*/
static struct pid2mem* pid2mem_alloc(void);

/*
   Free node's for reverse mapping's
*/
static void pid2mem_free(struct pid2mem *node); 

// used for nodes within rmap
static struct kmem_cache *rmap_node_allocator = NULL;

List ctable;    // Context table for active shared regions
struct spinlock ctable_lock;
struct condvar wait_cv;
struct kmem_cache *ctx_allocator;

static err_t getContextByHandle(char* name, int mapped, struct smemcontext** context, struct addrspace *as) {
    struct smemcontext *c = NULL;
    for (Node *n = list_begin(&ctable); n != list_end(&ctable); n = list_next(n)) {
        c = list_entry(n, struct smemcontext, ctable_node);
        kassert(c);
        if (strcmp((char*)name, (char*)c->name) == 0) {
            break;
        }
        c = NULL;
    }
    if (c != NULL && mapped) {
        int found = 0;
        kassert(c->store);
        for (Node *n = list_begin(&c->store->rmap.regions); n != list_end(&c->store->rmap.regions); n = list_next(n)) {
            struct pid2mem* p = list_entry(n, struct pid2mem, node);
            
            if (p->as == as) {
                found = 1;
                break;
            }
        }
        if (!found) {
            c = NULL;
        }
    }
    if (c != NULL) {
        if (context != NULL) {
            *context = c;
        }
        return ERR_OK;
    }
    return ERR_NOTEXIST;
}

static err_t contextInit(struct smemcontext* context, char* name, int size, rs_flag flag) {
    kassert(context);
    memset(context, 0, sizeof(struct smemcontext));
    strcpy((char*)context->name, (char*)name);
    context->store = shmms_alloc(context);
    if (context->store == NULL) {
        return ERR_NOMEM;
    }
    spinlock_init(&context->m_lock, False);
    context->size = size;
    context->refcnt = 0;
    context->lock_by = -1;
    context->flag = flag;
    return ERR_OK;
}

#pragma GCC diagnostic ignored "-Wunused-function"
static void printSharedRegions() {
    for (Node *n = list_begin(&ctable); n != list_end(&ctable); n = list_next(n)) {
        struct smemcontext *c = list_entry(n, struct smemcontext, ctable_node);
        kassert(c);
        kprintf("name = %s | sz = %d | refcnt = %d\n", c->name, c->size, c->refcnt);
    }
}

void smem_sys_init(void) {
    list_init(&ctable);
    spinlock_init(&ctable_lock, False);
    condvar_init(&wait_cv);
    ctx_allocator = kmem_cache_create(sizeof(struct smemcontext));
    kassert(ctx_allocator);
    kprintf("shared memory initialized\n");
}

err_t createSharedRegion(char* name, int n, rs_flag flag) {
    // Does there exist a region with this name?
    spinlock_acquire(&ctable_lock);
    
    if (getContextByHandle(name, REG_ANY, NULL, NULL) == ERR_OK) {
      // can we include return context here?
        spinlock_release(&ctable_lock);
        return ERR_EXIST;
    }
    
    // Nope, create the region context then
    struct smemcontext* context = (struct smemcontext*) kmem_cache_alloc(ctx_allocator);
    if (context == NULL) {
        spinlock_release(&ctable_lock);
        return ERR_NOMEM;
    }

    // Initialize the context
    if (contextInit(context, name, n, flag) != ERR_OK) {
        kmem_cache_free(ctx_allocator, context);
        spinlock_release(&ctable_lock);
        return ERR_NOMEM;
    }
    list_append(&ctable, &context->ctable_node);
    //printSharedRegions();
    spinlock_release(&ctable_lock);
    return ERR_OK;
}

err_t destroySharedRegion(char* name) {
    spinlock_acquire(&ctable_lock);
    struct smemcontext* ctx;
    err_t err;
    //printSharedRegions();
    if (getContextByHandle(name, REG_ANY, &ctx, NULL) != ERR_OK) {
        spinlock_release(&ctable_lock);
        return ERR_NOTEXIST;
    }
    err = destroySharedRegion_Internal(ctx, ctx->flag & RS_PERSIST);
    spinlock_release(&ctable_lock);
    return err;
}

static err_t destroySharedRegion_Internal(struct smemcontext *ctx, int forceDestroy) {

    // check if other regions are still mapped or persist
    if (ctx->refcnt != 0) {
        return ERR_MAPPED;
    } else if (((ctx->flag & RS_PERSIST) & ~forceDestroy)) {
        return ERR_PERSIST;
    }
    
    // remove smemcontext, free its memstore and context
    list_remove((Node*)&ctx->ctable_node);
    shmms_free(ctx->store);
    kmem_cache_free(ctx_allocator, ctx);

    return ERR_OK;
}

static void pid2mem_free(struct pid2mem *node) {

  kassert(node);
  kmem_cache_free(rmap_node_allocator, node);

}

static struct pid2mem* pid2mem_alloc(void) {

  struct pid2mem *n = NULL;

  if (rmap_node_allocator == NULL) {
    if ((rmap_node_allocator = kmem_cache_create(sizeof(struct pid2mem))) == NULL) {
      return NULL;
    }
  }

  n = kmem_cache_alloc(rmap_node_allocator);

  return n;

}

void addContextRef(struct smemcontext* ctx, vaddr_t start, struct addrspace* as, struct memregion* mr) {
    kassert(ctx);
    kassert(as);
    spinlock_acquire(&ctable_lock);

    err_t err = createMapping_Internal(ctx->name, &start, as, ctx, mr); 

    if (err != ERR_OK) {
      // cancel entire copy process?
      // probably should
      kprintf("error copying shared region: %s", ctx->name);
    }
  
    spinlock_release(&ctable_lock);
}

static err_t createMapping_Internal(char *name, vaddr_t* start, struct addrspace* as, struct smemcontext *context, struct memregion* mr) {  // 3/6 remove pid arg at end

  //struct memregion* mr;
  err_t err = ERR_OK;

  if (mr == NULL) {
      if (*start == NULL) {
        // initial mapping, any virtual address fine
        mr = as_map_memregion(as, ADDR_ANYWHERE, context->size * PG_SIZE, MEMPERM_URW,
                            context->store, 0, 1);
    } else {
        // called during forking, making a copy for user -> use start
        mr = as_map_memregion(as, *start, context->size * PG_SIZE, MEMPERM_URW,
                            context->store, 0, 1);
    }
  }

  //kprintf("inside internal\n");
  //as_meminfo(as);


  if (mr == NULL) {
    err = ERR_NOMEM;
  } else {
    // reverse mapping from pid->mr
    struct pid2mem *node = pid2mem_alloc();
    if (node == NULL){
      err = ERR_NOMEM;
      // mr freed in this call
      memregion_unmap(mr);
    } else {
     // set up context node
     node->as = as;
     node->mr = mr;
     list_append(&context->store->rmap.regions, (Node*)node);
     // memory region fields already assigned
     context->refcnt++;
     // return vaddr to user
     *start = mr->start;
 
     //kprintf("%p\n", mr->start);
    }
  }  // end mapping
    
    return err;   
}

err_t createMapping(char* name, vaddr_t* region_beg, struct addrspace *as) {

    struct smemcontext *context;
    err_t err = ERR_OK;

    spinlock_acquire(&ctable_lock);

    // context exists?
    if (getContextByHandle(name, REG_ANY, &context, NULL) != ERR_OK) {
      spinlock_release(&ctable_lock);
      return ERR_NOTEXIST;
    }

    err = createMapping_Internal(name, region_beg, as, context, NULL);

    spinlock_release(&ctable_lock);

    return err;
}

err_t destroyMapping(char* name, vaddr_t vaddr, struct addrspace *as) {

    struct smemcontext *context;

    spinlock_acquire(&ctable_lock);

    if (getContextByHandle(name, REG_MAPPED, &context, as) != ERR_OK) {
      spinlock_release(&ctable_lock);
      return ERR_NOTEXIST;
    }

    struct pid2mem *node;

    if ((node = find_remove_rmapping(context, as, vaddr)) != NULL) {
	struct memregion *mr = node->mr;
	pid2mem_free(node);
	spinlock_release(&ctable_lock);
	// handles biz
	return destroyMapping_Internal(context, mr, 0);
    }
    // no region found
    spinlock_release(&ctable_lock);

    return ERR_NOTEXIST;
}

static struct pid2mem* find_remove_rmapping(struct smemcontext *context, struct addrspace *as, vaddr_t vaddr) {

   // loop inv: for every node, n, seen, n->pid != proc_current.pid
   for (Node *n = list_begin(&context->store->rmap.regions);
               n != list_end(&context->store->rmap.regions);
               n = list_next(n)) {

      struct pid2mem *node = list_entry(n, struct pid2mem, node);

      if (node->as == as) {
        if (vaddr >= node->mr->start && vaddr < node->mr->end) {
          list_remove(n);
	      return node;
        }
      }
   }

   // not found
   return NULL;
}

static err_t destroyMapping_Internal(struct smemcontext* ctx, struct memregion *mr, int forceDestroy) {
    err_t err = ERR_OK;

    kassert(ctx);

    // call frees mr struct 
    memregion_unmap(mr);

    spinlock_acquire(&ctable_lock);
    ctx->refcnt--;

    // if no other references and !persist
    if (ctx->refcnt == 0 && !(ctx->flag & RS_PERSIST) && forceDestroy) {
      err = destroySharedRegion_Internal(ctx, 0);
    }

    spinlock_release(&ctable_lock);
    return err;
}

// caller holds r->as->as_lock
void unmapAll(struct memregion *r) {

    kassert(r->store);
    kassert(r->store->info);
    struct pid2mem *node;

    if ((node = find_remove_rmapping(r->store->info, r->as, r->start)) != NULL) {
      // have ref to region, can destroy node
      pid2mem_free(node);
  
      // calls method which needs lock, silly
      struct addrspace *as = r->as;
      spinlock_release(&as->as_lock);
      destroyMapping_Internal(r->store->info, r, 1);
      spinlock_acquire(&as->as_lock);
    }
}


err_t lockRegion(char* name, struct addrspace *as) { 
    struct smemcontext* ctx;
    spinlock_acquire(&ctable_lock);

    if (getContextByHandle(name, REG_MAPPED, &ctx, as) != ERR_OK) {
        spinlock_release(&ctable_lock);
        return ERR_NOTEXIST;
    }
    tid_t t = thread_current()->tid;
    while (ctx->lock_by != t && ctx->lock_by != -1) {
        condvar_wait(&wait_cv, &ctable_lock);
    }
    ctx->lock_by = t;
    spinlock_release(&ctable_lock);
    return ERR_OK;
}

err_t unlockRegion(char* name, struct addrspace *as) {
    struct smemcontext* ctx;
    spinlock_acquire(&ctable_lock);

    if (getContextByHandle(name, REG_MAPPED, &ctx, as) != ERR_OK) {
        spinlock_release(&ctable_lock);
        return ERR_NOTEXIST;
    }
    tid_t t = thread_current()->tid;
    if (ctx->lock_by != t) {
        spinlock_release(&ctable_lock);
        return ERR_INVAL;
    }
    ctx->lock_by = -1;
    condvar_signal(&wait_cv);
    spinlock_release(&ctable_lock);
    return ERR_OK;
}

/*
 * shared memory memstore fillpage function.
 */
static err_t fillpage(struct memstore *store, offset_t ofs, struct page *page);

/*
 * Shared memory memstore write function.
 */
static err_t write(struct memstore *store, paddr_t paddr, offset_t ofs);


static err_t
fillpage(struct memstore *store, offset_t ofs, struct page *page)
{
    // Fill in 0s
    kassert(store);
    kassert(page);

    memset((void*)kmap_p2v(page_to_paddr(page)), 0, pg_size);
    return ERR_OK;
}

static err_t
write(struct memstore *store, paddr_t paddr, offset_t ofs)
{
    // TODO - do we still want this?
    return ERR_OK;
}

struct memstore*
shmms_alloc(struct smemcontext* context)
{
    struct memstore *store;

    if ((store = memstore_alloc()) != NULL) {
        store->info = context;
        store->fillpage = fillpage;
        store->write = write;
    }
    return store;
}

void
shmms_free(struct memstore *store)
{
    kassert(store);
    memstore_free(store);
}
/*That's all folks*/
/*EOF*/
