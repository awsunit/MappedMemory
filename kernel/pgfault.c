#include <kernel/proc.h>
#include <kernel/console.h>
#include <kernel/trap.h>
#include <kernel/vm.h>
#include <kernel/vpmap.h>
#include <arch/mmu.h>
#include <lib/errcode.h>
#include <lib/string.h>
#include <kernel/memstore.h>
#include <kernel/pgcache.h>

size_t user_pgfault = 0;

err_t handleCOW(struct proc* proc, struct vpmap* vpmap, vaddr_t fault_addr);

/*
  Use memstore of the shared region to map physical page into address space
  Return:
    ERR_OK on success
*/
err_t handleSharedRegion(struct proc* proc, struct memregion* mr, struct vpmap* vpmap, vaddr_t fault_addr);

/*
  allocates and maps a page of memory
  args:
    proc - current process
    fault_addr - address to begin mapping from
  returns:
    ERR_OK on success, ERR_NOMEM otherwise
*/
err_t setup_page(struct proc *proc, vaddr_t fault_addr);

void
handle_page_fault(vaddr_t fault_addr, int present, int write, int user) {
    if (user) {
        __sync_add_and_fetch(&user_pgfault, 1);
    }
    // turn on interrupt now that we have the fault address 
    intr_set_level(INTR_ON);

    /* Your Code Here. */
    // COW
    struct memregion *mr;
    struct proc *proc;
    proc = proc_current();
    if ((mr = as_find_memregion(&proc->as, fault_addr, 1)) == NULL) {
        // no associated region -> fail
        proc_exit(-1);
    }
    if (mr->shared) {
      if (handleSharedRegion(proc, mr, mr->as->vpmap, fault_addr) == ERR_OK) {
        return;
      }
    }
    if (present && write) {
        if (mr->perm == MEMPERM_URW) {
            if (handleCOW(proc, mr->as->vpmap, fault_addr) == ERR_OK) {
                return;
            }
        }
    }
    if (user) {
        err_t err;

      if (mr->end == USTACK_UPPERBOUND) {

	// stack
	if ((err = setup_page(proc, fault_addr)) != ERR_OK) {
	  // no memory or error mapping -> fail?
	  proc_exit(-1);
	}

      } else if (mr == proc->as.heap) {

	// heap
	if ((err = setup_page(proc, fault_addr)) != ERR_OK) {
	  // no memory or error mapping -> fail?
	  proc_exit(-1);
	}

      } else {
	// bad memory access
	proc_exit(-1);
      }

    // end-user
    } else {
        kprintf("fault addr %p %d %d %d\n", fault_addr, present, write, user);
        panic("Kernel error in page fault handler\n");
    }
}

err_t
setup_page(struct proc *proc, vaddr_t fault_addr) {

  err_t err;
  paddr_t paddr;

  // get page
  if ((err = pmem_alloc(&paddr)) != ERR_OK) {
    return ERR_NOMEM;
  }
  memset((void*) kmap_p2v(paddr), 0, pg_size);

  // map the page
  if ((err = vpmap_map(proc->as.vpmap, pg_round_down(fault_addr),
    paddr, 1, MEMPERM_URW)) != ERR_OK) {
    // error mapping -> fail
    pmem_free(paddr);
    return ERR_NOMEM;
  }

  return ERR_OK;
}

err_t
handleCOW(struct proc* proc, struct vpmap* vpmap, vaddr_t fault_addr) {
    vaddr_t pageAddr = pg_round_down(fault_addr);
    paddr_t *paddr;
    if (vpmap_lookup_vaddr(vpmap, pageAddr, (paddr_t*)&paddr, NULL) != ERR_OK) {
        return ERR_FAULT;
    }
    struct page* pg = paddr_to_page((paddr_t)paddr);
    if (pg == NULL) {
        return ERR_FAULT;
    }
    sleeplock_acquire(&pg->lock);
    if (pg->refcnt == 1) {
        //kprintf("last ref\n");
        vpmap_set_perm(vpmap, pageAddr, 1, MEMPERM_URW);
    } else {
        paddr_t cpPage;
        //kprintf("mod: %p\n", fault_addr);
        // Allocate a new page
        if (pmem_alloc(&cpPage) != ERR_OK) {
            sleeplock_release(&pg->lock);
            return ERR_FAULT;
        }
        memcpy((void*)kmap_p2v((paddr_t)cpPage), (void*)pageAddr, pg_size);
        if (vpmap_map(vpmap, pageAddr, cpPage, 1, MEMPERM_URW) != ERR_OK) {
            pmem_free((paddr_t)cpPage);
            sleeplock_release(&pg->lock);
            return ERR_FAULT;
        }
        pmem_dec_refcnt((paddr_t)paddr);
        //kprintf("old page: %p -> new page: %p\n", paddr, cpPage);
    }
    sleeplock_release(&pg->lock);
    vpmap_flush_tlb();
    return ERR_OK;
}

err_t handleSharedRegion(struct proc* proc, struct memregion* mr, struct vpmap* vpmap, vaddr_t fault_addr) {
  //kprintf("shared page fault\n");
  sleeplock_acquire(&mr->store->pgcache_lock);
  struct page* pg = pgcache_get_page(mr->store, (offset_t)(fault_addr - mr->start));
  if (pg == NULL) {
    sleeplock_release(&mr->store->pgcache_lock);
    return ERR_FAULT;
  }

  paddr_t paddr = page_to_paddr(pg);
  if (paddr == NULL) {
    sleeplock_release(&mr->store->pgcache_lock);
    return ERR_FAULT;
  }

  sleeplock_acquire(&pg->lock);
  if (vpmap_map(vpmap, pg_round_down(fault_addr), paddr, 1, MEMPERM_URW) != ERR_OK) {
    sleeplock_release(&mr->store->pgcache_lock);
    sleeplock_release(&pg->lock);
    return ERR_FAULT;
  }
  pmem_inc_refcnt(paddr, 1);
  sleeplock_release(&pg->lock);
  sleeplock_release(&mr->store->pgcache_lock);
  //kprintf("done\n");
  return ERR_OK;
}