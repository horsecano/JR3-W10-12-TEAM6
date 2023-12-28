/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

   /* Copyright (c) 1992-1996 The Regents of the University of California.
	  All rights reserved.

	  Permission to use, copy, modify, and distribute this software
	  and its documentation for any purpose, without fee, and
	  without written agreement is hereby granted, provided that the
	  above copyright notice and the following two paragraphs appear
	  in all copies of this software.

	  IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
	  ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
	  CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
	  AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
	  HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	  THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
	  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
	  PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
	  BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
	  PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
	  MODIFICATIONS.
	  */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <debug.h>

	  /* Priority Scheduling */
bool cmp_sem_priority(const struct list_elem* a, const struct list_elem* b, void* aux UNUSED);
bool d_elem_cmp_priority(const struct list_elem* a, const struct list_elem* b, void* aux UNUSED);

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void sema_init(struct semaphore* sema, unsigned value)
{
	ASSERT(sema != NULL);

	sema->value = value;
	list_init(&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void sema_down(struct semaphore* sema)
{
	enum intr_level old_level;

	ASSERT(sema != NULL);
	ASSERT(!intr_context());

	old_level = intr_disable();
	while (sema->value == 0)
	{
		/* Priority Scheduling-Synchronization */
		// list_push_back (&sema->waiters, &thread_current ()->elem);
		list_insert_ordered(&sema->waiters, &thread_current()->elem, cmp_priority, NULL);
		thread_block();
	}
	sema->value--;
	intr_set_level(old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool sema_try_down(struct semaphore* sema)
{
	enum intr_level old_level;
	bool success;

	ASSERT(sema != NULL);

	old_level = intr_disable();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level(old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void sema_up(struct semaphore* sema)
{
	enum intr_level old_level;
	ASSERT(sema != NULL);
	old_level = intr_disable();

	if (!list_empty(&sema->waiters))
	{
		list_sort(&sema->waiters, cmp_priority, NULL);
		thread_unblock(list_entry(list_pop_front(&sema->waiters),
			struct thread, elem));
	}

	sema->value++;
	test_max_priority();
	intr_set_level(old_level);
}

static void sema_test_helper(void* sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test(void)
{
	struct semaphore sema[2];
	int i;

	printf("Testing semaphores...");
	sema_init(&sema[0], 0);
	sema_init(&sema[1], 0);
	thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up(&sema[0]);
		sema_down(&sema[1]);
	}
	printf("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper(void* sema_)
{
	struct semaphore* sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down(&sema[0]);
		sema_up(&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void lock_init(struct lock* lock)
{
	ASSERT(lock != NULL);

	lock->holder = NULL;
	sema_init(&lock->semaphore, 1);
}

void donate_priority(void)
{
	/*
	 Nested donation을 고려하여 구현
	 현재 쓰레드가 기다리고 있는 lock과 연결된 모든 쓰레드들을 순회하며 현재 쓰레드의 우선순위를 lock을 보유하고 있는 쓰레드에게 기부
	 현재 쓰레드가 기다리고 있는 락의 holder -> holde가 기다리고 있는 lock의 holder
	 nested depth는 8로 제한
	*/

	struct lock* curr_lock = thread_current()->wait_on_lock;
	struct thread* cmp_t = list_entry(list_begin(&curr_lock->holder->donations), struct thread, d_elem);

	while (curr_lock)
	{
		if (curr_lock->holder->priority < cmp_t->priority)
		{
			// curr_lock->holder->origin_priority = curr_lock->holder->priority;
			curr_lock->holder->priority = cmp_t->priority;
		}
		curr_lock = curr_lock->holder->wait_on_lock;
	}
}

/* Priority Donation */
bool d_elem_cmp_priority(const struct list_elem* a, const struct list_elem* b, void* aux UNUSED)
{
	/*
	TODO :
	1. 포인터를 받아서 쓰레드로 전환한다.
	2. 쓰레드의 우선 순위를 받는다.
	3. 우선 순위를 비교하여 블리언 값으로 리턴한다.
	*/

	struct thread* t_a;
	struct thread* t_b;

	t_a = list_entry(a, struct thread, d_elem);
	t_b = list_entry(b, struct thread, d_elem);

	return (t_a->priority) > (t_b->priority);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void lock_acquire(struct lock* lock)
{

	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(!lock_held_by_current_thread(lock));

	if (thread_mlfqs)
	{
		sema_down(&lock->semaphore);
		lock->holder = thread_current();
		return;
	}

	/* Priority Donation */
	if (lock->holder != NULL)
	{
		thread_current()->wait_on_lock = lock;
		list_insert_ordered(&lock->holder->donations, &thread_current()->d_elem, d_elem_cmp_priority, NULL);
		// list_push_back(&lock->holder->donations, &thread_current()->d_elem);

		donate_priority();
	}

	sema_down(&lock->semaphore);
	lock->holder = thread_current();
	thread_current()->wait_on_lock = NULL;
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool lock_try_acquire(struct lock* lock)
{
	bool success;

	ASSERT(lock != NULL);
	ASSERT(!lock_held_by_current_thread(lock));

	success = sema_try_down(&lock->semaphore);
	if (success)
		lock->holder = thread_current();
	return success;
}

void refresh_priority(void)
{
	struct thread* curr = thread_current();

	if (list_empty(&curr->donations))
	{
		curr->priority = curr->origin_priority;
		return;
	}

	struct thread* max_t = list_entry(list_begin(&curr->donations), struct thread, d_elem);
	curr->priority = curr->origin_priority;

	if (curr->priority < max_t->priority)
	{
		curr->priority = max_t->priority;
	}
}

void remove_with_lock(struct lock* lock)
{
	/*
	 lock을 해지 했을 때, waiters 리스트에서 해당 엔트리를 삭제 하기 위한 함수를 구현
	 현재 쓰레드의 waiters 리스트를 확인하여 해지할 lock을 보유하고 있는 엔트리를 삭제
	*/
	struct thread* curr_t = thread_current();
	struct list_elem* e;
	// struct list_elem *e = list_begin(&curr_t->donations);

	// while (e != list_end(&curr_t->donations))
	// {
	// 	struct thread *lock_need_thread = list_entry(e, struct thread, d_elem);
	// 	if (lock == lock_need_thread->wait_on_lock)
	// 	{
	// 		list_remove(e);
	// 	}
	// 	e = list_next(e);
	// }

	for (e = list_begin(&curr_t->donations); e != list_end(&curr_t->donations);)
	{
		struct thread* lock_need_thread = list_entry(e, struct thread, d_elem);
		if (lock == lock_need_thread->wait_on_lock)
		{
			list_remove(e);
		}
		e = list_next(e);
	}
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void lock_release(struct lock* lock)
{
	ASSERT(lock != NULL);
	ASSERT(lock_held_by_current_thread(lock));

	if (thread_mlfqs)
	{
		lock->holder = NULL;
		sema_up(&lock->semaphore);
		return;
	}
	remove_with_lock(lock);
	refresh_priority();
	lock->holder = NULL;
	sema_up(&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool lock_held_by_current_thread(const struct lock* lock)
{
	ASSERT(lock != NULL);

	return lock->holder == thread_current();
}

/* One semaphore in a list. */
struct semaphore_elem
{
	struct list_elem elem;		/* List element. */
	struct semaphore semaphore; /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void cond_init(struct condition* cond)
{
	ASSERT(cond != NULL);

	list_init(&cond->waiters);
}

bool cmp_sem_priority(const struct list_elem* a, const struct list_elem* b, void* aux UNUSED)
{
	/* TODO
	고려 사항 : 전달 받은 인자는 semaphore_elem의 elem이다.
	목표 : 이 elem으로부터 Thread를 찾아야한다.

	1. elem으로 semaphore_elem을 얻는다. -> list_entry
	2. semaphore의 wait list의 첫번째 위차한 놈을 찾는다. -> list_bigin
	 -> 왜냐? 우선 순위에 맞게 삽입을 해놨으니, semaphore 간의 우선 순위를 확인할 때는 각 리스트의 첫번째 놈들끼지만 비교하면 된다.
	3. 첫번째 놈들끼리 우선 순위를 비교한다.
	*/

	struct semaphore_elem* a_sema_elem = list_entry(a, struct semaphore_elem, elem);
	struct semaphore_elem* b_sema_elem = list_entry(b, struct semaphore_elem, elem);

	if (list_empty(&a_sema_elem->semaphore.waiters))
	{
		return false;
	}

	if (list_empty(&b_sema_elem->semaphore.waiters))
	{
		return true;
	}
	struct list_elem* a_elem = list_begin(&(a_sema_elem->semaphore.waiters));
	struct list_elem* b_elem = list_begin(&(b_sema_elem->semaphore.waiters));

	struct thread* a_t = list_entry(a_elem, struct thread, elem);
	struct thread* b_t = list_entry(b_elem, struct thread, elem);

	return (a_t->priority) > (b_t->priority);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void cond_wait(struct condition* cond, struct lock* lock)
{
	struct semaphore_elem waiter;

	ASSERT(cond != NULL);
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(lock_held_by_current_thread(lock));

	sema_init(&waiter.semaphore, 0);
	/* Priority Scheduling-Synchronization */
	// list_push_back (&cond->waiters, &waiter.elem);
	list_insert_ordered(&cond->waiters, &waiter.elem, cmp_sem_priority, NULL);
	lock_release(lock);
	sema_down(&waiter.semaphore);
	lock_acquire(lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_signal(struct condition* cond, struct lock* lock UNUSED)
{
	ASSERT(cond != NULL);
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(lock_held_by_current_thread(lock));

	if (!list_empty(&cond->waiters))
	{
		list_sort(&cond->waiters, cmp_sem_priority, NULL);
		sema_up(&list_entry(list_pop_front(&cond->waiters),
			struct semaphore_elem, elem)
			->semaphore);
	}
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_broadcast(struct condition* cond, struct lock* lock)
{
	ASSERT(cond != NULL);
	ASSERT(lock != NULL);

	while (!list_empty(&cond->waiters))
		cond_signal(cond, lock);
}
