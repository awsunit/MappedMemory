#include <kernel/console.h>
#include <kernel/proc.h>
#include <kernel/kmalloc.h>
#include <kernel/thread.h>
#include <kernel/list.h>
#include <kernel/fs.h>
#include <kernel/vpmap.h>
#include <kernel/pipe.h>
#include <kernel/shmms.h>
#include <arch/elf.h>
#include <arch/trap.h>
#include <arch/mmu.h>
#include <lib/errcode.h>
#include <lib/stddef.h>
#include <lib/string.h>

List ptable; // process table
struct spinlock ptable_lock;
struct spinlock pid_lock;
static int pid_allocator;
struct kmem_cache *proc_allocator;

/* go through process table */
static void ptable_dump(void);
/* helper function for loading process's binary into its address space */ 
static err_t proc_load(struct proc *p, char *path, vaddr_t *entry_point);
/* helper function to set up the stack */
static err_t stack_setup(struct proc *p, char **argv, vaddr_t* ret_stackptr);
/* helper function to get a process's proc struct by pid from ptable, must be called holding the ptable lock*/
static err_t getProcByPID(int pid, struct proc** p);

/* tranlsates a kernel vaddr to a user stack address, assumes stack is a single page */
#define USTACK_ADDR(addr) (pg_ofs(addr) + USTACK_UPPERBOUND - pg_size);

static struct proc*
proc_alloc()
{
    struct proc* p = (struct proc*) kmem_cache_alloc(proc_allocator);
    if (p != NULL) {
        spinlock_acquire(&pid_lock);
        p->pid = pid_allocator++;
        spinlock_release(&pid_lock);
    }
    return p;
}

#pragma GCC diagnostic ignored "-Wunused-function"
static void
ptable_dump(void)
{
    kprintf("ptable dump:\n");
    spinlock_acquire(&ptable_lock);
    for (Node *n = list_begin(&ptable); n != list_end(&ptable); n = list_next(n)) {
        struct proc *p = list_entry(n, struct proc, proc_node);
        kprintf("Process %s: pid %d\n", p->name, p->pid);
    }
    spinlock_release(&ptable_lock);
    kprintf("\n");
}

void
proc_free(struct proc* p)
{
    kmem_cache_free(proc_allocator, p);
}

void
proc_sys_init(void)
{
    list_init(&ptable);
    spinlock_init(&ptable_lock, False);
    spinlock_init(&pid_lock, False);
    proc_allocator = kmem_cache_create(sizeof(struct proc));
    kassert(proc_allocator);
}

/*
 * Allocate and initialize basic proc structure
*/
static struct proc*
proc_init(char* name)
{
    struct super_block *sb;
    inum_t inum;
    err_t err;

    struct proc *p = proc_alloc();
    if (p == NULL) {
        return NULL;
    }

    if (as_init(&p->as) != ERR_OK) {
        proc_free(p);
        return NULL;
    }

    size_t slen = strlen(name);
    slen = slen < PROC_NAME_LEN-1 ? slen : PROC_NAME_LEN-1;
    memcpy(p->name, name, slen);
    p->name[slen] = 0;

    list_init(&p->threads);

	// cwd for all processes are root for now
    sb = root_sb;
	inum = root_sb->s_root_inum;
    if ((err = fs_get_inode(sb, inum, &p->cwd)) != ERR_OK) {
        as_destroy(&p->as);
        proc_free(p);
        return NULL;
    }
    // standard I/O
    for (int i = 0; i < PROC_MAX_FILE; i++) {
        p->files[i] = NULL;
    }
    p->files[0] = &stdin;
    p->files[1] = &stdout;
    fs_reopen_file(&stdin);
    fs_reopen_file(&stdout);

    list_init(&p->children);
    // setup waiter cv
    condvar_init(&p->wait_cv);

    // Setup exit_status
    p->exit_status = STATUS_ALIVE;
    return p;
}

err_t
proc_spawn(char* name, char** argv, struct proc **p)
{
    err_t err;
    struct proc *proc;
    struct thread *t;
    vaddr_t entry_point;
    vaddr_t stackptr;

    if ((proc = proc_init(name)) == NULL) {
        return ERR_NOMEM;
    }
    // load binary of the process
    if ((err = proc_load(proc, name, &entry_point)) != ERR_OK) {
        goto error;
    }

    // allocate memregion for heap
    if ((proc->as.heap = as_map_memregion(&proc->as, ADDR_ANYWHERE, 0, MEMPERM_URW, NULL, 0, 0)) == NULL) {
        err = ERR_NOMEM;
        goto error;
    }

    // set up stack and allocate its memregion 
    if ((err = stack_setup(proc, argv, &stackptr)) != ERR_OK) {
        goto error;
    }

    if ((t = thread_create(proc->name, proc, DEFAULT_PRI)) == NULL) {
        err = ERR_NOMEM;
        goto error;
    }

    // Set up parent for the new process
    if (proc_current()) {
        list_append((List*)&proc_current()->children, (Node*)&proc->child_node);
    }

    // add to ptable
    spinlock_acquire(&ptable_lock);
    list_append(&ptable, &proc->proc_node);
    spinlock_release(&ptable_lock);

    // set up trapframe for a new process
    tf_proc(t->tf, t->proc, entry_point, stackptr);
    thread_start_context(t, NULL, NULL);

    // fill in allocated proc
    if (p) {
        *p = proc;
    }
    return ERR_OK;
error:
    as_destroy(&proc->as);
    proc_free(proc);
    return err;
}

struct proc*
proc_fork()
{
    struct proc* parent, *child;
    struct thread* th_parent, *th_child;
    parent = proc_current();
    th_parent = thread_current();
    kassert(parent);
    kassert(th_parent);
    child = proc_init(parent->name);
    
    if (child == NULL) {
        return NULL;
    }
    if (as_copy_as((struct addrspace*)&parent->as, (struct addrspace*)&child->as) != ERR_OK) {
        kprintf("as copy failed\n");
        goto error;
    }
    th_child = thread_create(th_parent->name, child, th_parent->priority);
    if (th_child == NULL) {
        kprintf("cthread failed\n");
        goto error;
    }

    th_child->proc = child;
    // Close standard I/O
    fs_close_file(child->files[0]);
    fs_close_file(child->files[1]);

    // Copy the file table
    
    for (int i = 0; i < PROC_MAX_FILE; i++) {
        if (parent->files[i] != NULL) {
	    // pipe
  	    kpipe_ref_increase(parent->files[i], i);
            child->files[i] = parent->files[i];
            fs_reopen_file((struct file*)parent->files[i]);
        }
    }
    // Mark child as child
    list_append((List*)&parent->children, (Node*)&child->child_node);

    // Copy + modify trapframe
    *(th_child->tf) = *(th_parent->tf);
    th_child->tf->rax = 0;  // return 0 for child
    th_parent->tf->rax = child->pid;    // return child's pid for parent

    // Add child to ptable
    spinlock_acquire(&ptable_lock);
    list_append((List*)&ptable, (Node*)&child->proc_node);
    spinlock_release(&ptable_lock);

    thread_start_context(th_child, NULL, NULL);
    return child;
error:
    as_destroy(&child->as);
    proc_free(child);
    return NULL;

}

struct proc*
proc_current()
{
    return thread_current()->proc;
}

void
proc_attach_thread(struct proc *p, struct thread *t)
{
    kassert(t);
    if (p) {
        list_append(&p->threads, &t->thread_node);
    }
}

bool
proc_detach_thread(struct thread *t)
{
    bool last_thread = False;
    struct proc *p = t->proc;
    if (p) {
        list_remove(&t->thread_node);
        last_thread = list_empty(&p->threads);
    }
    return last_thread;
}

int
proc_wait(pid_t pid, int* status)
{
   // Check if there is a such a child
    struct proc *cur = NULL, *child = NULL;
    Node *n, *next;
    cur = proc_current();
    kassert(cur);
    
    spinlock_acquire(&ptable_lock);
    for (n = list_begin(&cur->children); n != list_end(&cur->children); n = next) {
        struct proc* p = list_entry(n, struct proc, child_node);
        next = list_next(n);
        if (p->pid == pid || (pid == ANY_CHILD && next == list_end(&cur->children))) {
            child = p;
            pid = p->pid;
            break;
        }
    }
    if (child == NULL) {
        // Child not found
        spinlock_release(&ptable_lock);
        return ERR_CHILD;
    }
    
    while (child->exit_status == STATUS_ALIVE) {
        condvar_wait(&child->wait_cv, &ptable_lock);
    }
    if (status != NULL) {
        *status = child->exit_status;
    }
    list_remove((Node*)&child->child_node); // remove from children list
    proc_free(child);
    spinlock_release(&ptable_lock);
    return pid;

}

void
proc_exit(int status)
{
    struct thread *t = thread_current();
    struct proc *p = proc_current();
    struct proc *init;
    Node* next;

    // detach current thread, switch to kernel page table
    // free current address space if proc has no more threads
    // order matters here
    proc_detach_thread(t);
    t->proc = NULL;
    vpmap_load(kas->vpmap);

    as_destroy(&p->as);

    // release process's cwd
    fs_release_inode(p->cwd);
    /* your code here */
    
    for (int i = 0; i < PROC_MAX_FILE; i++) {
        if (p->files[i] != NULL) {
	    // pipe
            kpipe_ref_decrease(p->files[i], i);
            fs_close_file(p->files[i]);
        }
    }
    
    spinlock_acquire(&ptable_lock);
    list_remove((Node*)&p->proc_node);  // remove self from ptable
    
    kassert(getProcByPID(0, (struct proc**)&init) == ERR_OK);

    // Make any running children the child of init
    for (Node *n = list_begin(&p->children); n != list_end(&p->children); n = next) {
        struct proc* p = list_entry(n, struct proc, child_node);
        next = list_remove((Node*)&p->child_node);
        if (p->exit_status == STATUS_ALIVE) {
            list_append((List*)&init->children, (Node*)&p->child_node);
        } else {
            proc_free(p);
        }
    }
    p->exit_status = status;
    condvar_signal(&p->wait_cv);
    spinlock_release(&ptable_lock);

    thread_exit(status);
}

/* helper function for loading process's binary into its address space */ 
static err_t
proc_load(struct proc *p, char *path, vaddr_t *entry_point)
{
    int i;
    err_t err;
    offset_t ofs = 0;
    struct elfhdr elf;
    struct proghdr ph;
    struct file *f;
    paddr_t paddr;
    vaddr_t vaddr;

    if ((err = fs_open_file(path, FS_RDONLY, 0, &f)) != ERR_OK) {
        return err;
    }

    // check if the file is actually an executable file
    if (fs_read_file(f, (void*) &elf, sizeof(elf), &ofs) != sizeof(elf) || elf.magic != ELF_MAGIC) {
        return ERR_INVAL;
    }

    // read elf and load binary
    for (i = 0, ofs = elf.phoff; i < elf.phnum; i++) {
        if (fs_read_file(f, (void*) &ph, sizeof(ph), &ofs) != sizeof(ph)) {
            return ERR_INVAL;
        }
        if(ph.type != PT_LOAD)
            continue;

        if(ph.memsz < ph.filesz || ph.vaddr + ph.memsz < ph.vaddr) {
            return ERR_INVAL;
        }

        memperm_t perm = MEMPERM_UR;
        if (ph.flags & PF_W) {
            perm = MEMPERM_URW;
        }

        // found loadable section, add as a memregion
        if (as_map_memregion(&p->as, pg_round_down(ph.vaddr), pg_round_up(ph.memsz + pg_ofs(ph.vaddr)),
            perm, NULL, ph.off, False) == NULL) {
            return ERR_NOMEM;
        }

        // pre-page in code and data, may span over multiple pages
        int count = 0;
        size_t avail_bytes;
        size_t read_bytes = ph.filesz;
        size_t pages = pg_round_up(ph.memsz + pg_ofs(ph.vaddr)) / pg_size;
        // vaddr may start at a nonaligned address
        vaddr = pg_ofs(ph.vaddr);
        while (count < pages) {
            // allocate a physical page and zero it first
            if ((err = pmem_alloc(&paddr)) != ERR_OK) {
                return err;
            }
            vaddr += kmap_p2v(paddr);
            memset((void*)pg_round_down(vaddr), 0, pg_size);
            // calculate how many bytes to read from file
            avail_bytes = read_bytes < (pg_size - pg_ofs(vaddr)) ? read_bytes : (pg_size - pg_ofs(vaddr));
            if (avail_bytes && fs_read_file(f, (void*)vaddr, avail_bytes, &ph.off) != avail_bytes) {
                return ERR_INVAL;
            }
            // map physical page with code/data content to expected virtual address in the page table
            if ((err = vpmap_map(p->as.vpmap, ph.vaddr+count*pg_size, paddr, 1, perm)) != ERR_OK) {
                return err;
            }
            read_bytes -= avail_bytes;
            count++;
            vaddr = 0;
        }
    }
    *entry_point = elf.entry;
    return ERR_OK;
}

err_t
stack_setup(struct proc *p, char **argv, vaddr_t* ret_stackptr)
{
    err_t err;
    paddr_t paddr;
    vaddr_t stackptr;
    vaddr_t stacktop = USTACK_UPPERBOUND-pg_size;

    // allocate a page of physical memory for stack
    if ((err = pmem_alloc(&paddr)) != ERR_OK) {
        return err;
    }
    memset((void*) kmap_p2v(paddr), 0, pg_size);
    // create memregion for stack
    if (as_map_memregion(&p->as, stacktop, pg_size, MEMPERM_URW, NULL, 0, False) == NULL) {
        err = ERR_NOMEM;
        goto error;
    }
    // map in first stack page
    if ((err = vpmap_map(p->as.vpmap, stacktop, paddr, 1, MEMPERM_URW)) != ERR_OK) {
        goto error;
    }
    // extend memregion of stack
    struct memregion *mr = as_find_memregion(&p->as, stacktop, 0);
    if (mr == NULL) {
      kprintf("bad");
    }
    kassert(as_find_memregion(&p->as, USTACK_UPPERBOUND - (10*pg_size), 9*pg_size) == NULL);
    // lock here?
    mr->start = USTACK_UPPERBOUND - (10*pg_size);
    // kernel virtual address of the user stack, points to top of the stack
    // as you allocate things on stack, move stackptr downward.
    stackptr = kmap_p2v(paddr) + pg_size;
    vaddr_t tableAddr = stackptr;   // fast ptr
    vaddr_t tableEntryAddr;
    /* Your Code Here.*/  
    int argc = 0;
    char* ptr = argv[argc];

    // Get argc + setup tableAddr
    while (ptr != NULL) {
        argc++;
        tableAddr -= strlen(ptr) + 1;
        ptr = argv[argc];
    }

    tableAddr = tableAddr & ~(sizeof(void*) - 1);
    // tableAddr points at the end of argv array (on stack)

    tableAddr -= sizeof(void*);  // Null entry
    tableEntryAddr = tableAddr - argc * sizeof(void*);  // points to the head of argv array
    // tableAddr now points to the end of argv array
    // We do that because the address of array entry is increasing, 
    //  so argv[0]'s address < argv[1]'s

    // stackptr is still on top of stack
    for (int i = 0; i < argc; i++) {
        ptr = argv[i];
        int len = strlen(ptr);
        stackptr -= len + 1;
        memcpy((char*)stackptr, (char*)ptr, len);
        
        // Copy the address into argv array
        //*(uint64_t*)tableAddr = USTACK_ADDR(stackptr);
        *(uint64_t*)tableEntryAddr = USTACK_ADDR(stackptr);
        tableEntryAddr += sizeof(void*);    // Increment address for next entry
    }
    tableAddr -= argc * sizeof(void*);  // Points at argv, the variable
    stackptr = tableAddr - sizeof(void*);   // Allocate space for the pointer to argv[0]
    *(uint64_t*)stackptr = USTACK_ADDR(tableAddr);  // Put in &argv[0]
    stackptr -= sizeof(void*);
    *(uint64_t*)stackptr = argc;    // Put argc on stack
    stackptr -= sizeof(void*);  // fake return

    // stack state for argc == 2

    // paddr: USTACK_UPPERBOUND
    //
    // *pg_size bytes*
    //
    //
    // argv[0]_COPY
    // argv[1]_COPY
    // size(*void)
    // tableEntryAddr: USTACK_ADDR(argv[1]_COPY)
    // tableAddr: USTACK_ADDR(argv[0]_COPY)
    // USTACK_ADDR(tableAddr)
    // argc
    // stackptr: size(*void)

    // allocate space for fake return address, argc, argv
    // remove following line when you actually set up the stack

    // translates stackptr from kernel virtlsual address to user stack address
    *ret_stackptr = USTACK_ADDR(stackptr); 
    return err;
error:
    pmem_free(paddr);
    return err;
}

static err_t getProcByPID(int pid, struct proc** pa) {

    for (Node *n = list_begin(&ptable); n != list_end(&ptable); n = list_next(n)) {
        struct proc* p;
        p = list_entry(n, struct proc, proc_node);
        if (pid == ANY_CHILD || p->pid == pid) {
            *pa = p;
            return ERR_OK;
        }
    }
    return ERR_INVAL;
}
