#include <kernel/trap.h>
#include <kernel/synch.h>
#include <kernel/console.h>
#include <kernel/thread.h>
#include <kernel/sched.h>
#include <lib/errcode.h>
#include <lib/stddef.h>

static bool synch_enabled = False;

void
synch_init(void)
{
    synch_enabled = True;
}

void
spinlock_init(struct spinlock* lock, uint8_t intrlock)
{
    kassert(lock);
    lock->lock_status = 0;
    lock->holder = NULL;
    lock->intrlock = intrlock;
}

void
spinlock_acquire(struct spinlock* lock)
{
    if (!synch_enabled) {
        return;
    }
    kassert(lock);
    intr_set_level(INTR_OFF);
    // can't grab the same lock again
    struct thread *curr = thread_current();
    if (lock->holder != NULL && lock->holder == curr) {
        panic("MEH");
    }
    kassert(lock->holder == NULL || lock->holder != curr);
    while (lock->lock_status || __sync_lock_test_and_set(&lock->lock_status, 1) != 0);
    __sync_synchronize();
    lock->holder = curr;
}

err_t
spinlock_try_acquire(struct spinlock* lock)
{
    if (!synch_enabled) {
        return ERR_OK;
    }
    kassert(lock);
    intr_set_level(INTR_OFF);
    // can't grab the same lock again
    struct thread *curr = thread_current();
    kassert(lock->holder == NULL || lock->holder != curr);
    if (lock->lock_status == 0 && __sync_lock_test_and_set(&lock->lock_status, 1) == 0) {
        __sync_synchronize();
        lock->holder = curr;
        return ERR_OK;
    }
    intr_set_level(INTR_ON);
    return ERR_LOCK_BUSY;
}


void
spinlock_release(struct spinlock* lock)
{
    if (!synch_enabled) {
        return;
    }
    kassert(lock);
    lock->holder = NULL;
    __sync_lock_release(&lock->lock_status);
    __sync_synchronize();
   intr_set_level(INTR_ON);
}

void
sleeplock_init(struct sleeplock* lock)
{
    kassert(lock);
    spinlock_init(&lock->lk, False);
    condvar_init(&lock->waiters);
    lock->holder = NULL;
}

void
sleeplock_acquire(struct sleeplock* lock)
{
    if (!synch_enabled) {
        return;
    }
    kassert(lock);
    spinlock_acquire(&lock->lk);
    while (lock->holder != NULL) {
        condvar_wait(&lock->waiters, &lock->lk);
    }
    lock->holder = thread_current();
    spinlock_release(&lock->lk);
}

void
sleeplock_release(struct sleeplock* lock)
{
    if (!synch_enabled) {
        return;
    }
    kassert(lock && lock->holder == thread_current());
    spinlock_acquire(&lock->lk);
    lock->holder = NULL;
    condvar_signal(&lock->waiters);
    spinlock_release(&lock->lk);
}

void
condvar_init(struct condvar* cv)
{
    kassert(cv);
    list_init(&cv->waiters);
}

void
condvar_wait(struct condvar* cv, struct spinlock* lock)
{
    if (!synch_enabled) {
        return;
    }
    // assuming we are holding lock already
    kassert(cv && lock);
    struct thread *t = thread_current();
    kassert(lock->holder == t);

    // add to cv's waiter list, cv is protected by the lock
    list_append(&cv->waiters, &t->node);
    // put ourselves to sleep and wake up next lock holder
    sched_sched(SLEEPING, lock); // lock is released in the sched call
    spinlock_acquire(lock);
}

void
condvar_signal(struct condvar* cv)
{
    if (!synch_enabled) {
        return;
    }
    kassert(cv);
    if (list_empty(&cv->waiters)) {
        return;
    }
    Node* n = list_begin(&cv->waiters);
    list_remove(n);
    struct thread *wakeup_thread = list_entry(n, struct thread, node);
    sched_ready(wakeup_thread);
}

void
condvar_broadcast(struct condvar* cv)
{
    if (!synch_enabled) {
        return;
    }
    kassert(cv);
    for (Node* n = list_begin(&cv->waiters); n != list_end(&cv->waiters);) {
        struct thread *wakeup_thread = list_entry(n, struct thread, node);
        n = list_remove(n);
        sched_ready(wakeup_thread);
    }
}
