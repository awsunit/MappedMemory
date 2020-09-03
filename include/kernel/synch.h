#ifndef _SYNCH_H_
#define _SYNCH_H_

#include <kernel/list.h>

#define ERR_LOCK_BUSY 1

/* Spinlock */
struct spinlock {
    uint8_t lock_status;
    uint8_t intrlock;  // true if spinlock is used by interrupt handler
    struct thread *holder;
};

/* Condition variable */
struct condvar {
    List waiters;
};

/* Sleeplock */
struct sleeplock {
    struct spinlock lk;     // spinlock that protects access to waiters
    struct condvar waiters;
    struct thread *holder;
};

void synch_init(void);

void spinlock_init(struct spinlock *lock, uint8_t intrlock);

err_t spinlock_try_acquire(struct spinlock* lock);

void spinlock_acquire(struct spinlock *lock);

void spinlock_release(struct spinlock *lock);


void sleeplock_init(struct sleeplock *lock);

void sleeplock_acquire(struct sleeplock *lock);

void sleeplock_release(struct sleeplock *lock);


void condvar_init(struct condvar *cv);

void condvar_wait(struct condvar *cv, struct spinlock* lock);

void condvar_signal(struct condvar *cv);

void condvar_broadcast(struct condvar *cv);

#endif /* _SYNCH_H_ */
