#include <arch/cpu.h>
#include <kernel/trap.h>
#include <kernel/sched.h>
#include <kernel/console.h>
#include <kernel/list.h>
#include <lib/errcode.h>
#include <lib/stddef.h>

List _ready_queue;
List* ready_queue = &_ready_queue;
/* 
 * Schedules a new thread, if no thread on the ready queue
 * current cpu's idle thread is scheduled. Returns a thread
 * if the descheduled thread needs to be reclaimed.
 * */
static struct thread* sched(void);

void
sched_sys_init(void)
{
    list_init(ready_queue);
    spinlock_init(&sched_lock, True);
}

err_t
sched_start()
{
    kassert(intr_get_level() == INTR_OFF);
    cpu_set_idle_thread(mycpu(), thread_current());
    kprintf("cpu %d is up and scheduling \n", mycpu()->lapic_id);
    // turn on interrupt
    intr_set_level(INTR_ON);
    return ERR_OK;
}

err_t
sched_start_ap()
{
    kassert(intr_get_level() == INTR_OFF);
    // initialize idle thread and starts interrupt
    struct thread *t = thread_create("idle thread", NULL, DEFAULT_PRI);
    if (t == NULL) {
       return !ERR_OK;
    }
    cpu_set_idle_thread(mycpu(), t);
    kprintf("cpu %d is up and scheduling \n", mycpu()->lapic_id);
    // turn on interrupt
    intr_set_level(INTR_ON);
    return ERR_OK;
}

void
sched_ready(struct thread *t)
{
    kassert(t);
    spinlock_acquire(&sched_lock);
    t->state = READY;
    list_append(ready_queue, &t->node);
    spinlock_release(&sched_lock);
}

void
sched_sched(threadstate_t next_state, struct spinlock* lock)
{
    struct thread *curr = thread_current();
    spinlock_acquire(&sched_lock);
    if (next_state == READY) {
        // checking ready queue empty to avoid adding ourself to the list
        // and then immediately remove ourself
        if (list_empty(ready_queue)) {
            spinlock_release(&sched_lock);
            return;
        }
        if (curr != cpu_idle_thread(mycpu())) {
            list_append(ready_queue, &curr->node);
        }
    }
    if (lock) {
        spinlock_release(lock);
    }
    curr->state = next_state;
    // schedule a new thread and see if any thread needs to be cleaned up
    struct thread *dying = sched();
    spinlock_release(&sched_lock);
    if (dying) {
        thread_cleanup(dying);
    }
}

// function to schedule thread, sched_lock must be held before calling this function
static struct thread*
sched(void)
{
    void *cpu = mycpu();
    struct thread *curr = thread_current();
    struct thread *prev, *t;

    if (!list_empty(ready_queue)) {
        Node *n = list_begin(ready_queue);
        t = (struct thread*) list_entry(n, struct thread, node);
        kassert(t->state == READY);
        list_remove(n);
    } else {
        // if current thread is not the idle thread, schedules to idle thread of the cpu
        struct thread *idle = cpu_idle_thread(cpu);
        kassert(idle != curr);
        t = idle;
    }
    t->state = RUNNING;
    prev = cpu_switch_thread(cpu, t);
    kassert(prev);
    return prev->state == ZOMBIE ? prev : NULL;
}
