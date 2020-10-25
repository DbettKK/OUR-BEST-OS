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

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */


static bool
sema_cmp(const struct list_elem *a, const struct list_elem *b, void *aux);
static bool
cond_cmp(const struct list_elem *a, const struct list_elem *b, void *aux);
static bool
lock_cmp(const struct list_elem *a, const struct list_elem *b, void *aux);

void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      list_push_back (&sema->waiters, &thread_current ()->elem);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) {
    /* 可能存在线程由于优先级捐赠导致的优先级变化 所以需要重新排序. */
    list_sort(&sema->waiters, sema_cmp, NULL);
    thread_unblock(list_entry(list_pop_front(&sema->waiters), struct thread, elem));
  }
  sema->value++;
  // 让优先级高的占据资源 优先级调度
  thread_yield();
  intr_set_level (old_level);
  
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
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
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);
  lock->max_pri = 0;  // 初始化
  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  /* 判断当前持有锁的线程优先级是否比自己低 如果低 则捐赠 */
  /* 判断自己优先级是否比当前锁的最高优先级高 如果是 则更新并捐赠. */
  struct thread *cur_t = thread_current(), *lock_h_t;
  
  if (lock->holder != NULL) {
    /* 锁存在持有者.*/
    /* 当前线程由于该锁而阻塞. */
    cur_t->blocked_lock = lock;
 
    struct lock *cur_lock = lock;
    while (1) {
      lock_h_t = cur_lock->holder;
      
      if (cur_t->priority > cur_lock->max_pri) {
        cur_lock->max_pri = cur_t->priority;
        /* 开始捐赠. */
        if (!list_empty (&lock_h_t->lock_list)) {
          /* 新线程尝试获取锁 锁对应的max_pri得到更新 需要重新sort一下. */
          list_sort(&lock_h_t->lock_list, lock_cmp, NULL);
          /* 获得最大优先级. */
          struct list_elem *tmp_elem = list_front(&lock_h_t->lock_list);
          struct lock *lock_max = list_entry(tmp_elem, struct lock, elem);
          int lock_max_pri = lock_max->max_pri;
          lock_h_t->priority = lock_max_pri > lock_h_t->original_pri ? lock_max_pri : lock_h_t->original_pri;
        } else {
          /* 如果当前线程没有持有任何锁 则跳过上面的if 参考priority-donate-one. */
          lock_h_t->priority = lock_h_t->original_pri;
        }
      }
      if (lock_h_t->blocked_lock == NULL) {
        break;
      }
      cur_lock = lock_h_t->blocked_lock;
    } 
  }

  sema_down (&lock->semaphore);

  /* 当前线程获得锁 无阻塞的锁. */
  cur_t->blocked_lock = NULL;
  
  /* 在当前线程持有的锁的list中加入该锁. */
  list_push_back(&cur_t->lock_list, &lock->elem);

  lock->holder = thread_current ();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  if (!thread_mlfqs) {
    /* 线程的持有锁list中删除该锁. */
    list_remove(&lock->elem);
    
    /* 更新当前线程优先级. */
    struct thread *cur_t = thread_current();
    if (!list_empty (&cur_t->lock_list)) {
      list_sort (&cur_t->lock_list, lock_cmp, NULL);
      struct list_elem *tmp_elem = list_front(&cur_t->lock_list);
      struct lock *lock_max = list_entry(tmp_elem, struct lock, elem);
      int lock_max_pri = lock_max->max_pri;
      cur_t->priority = lock_max_pri > cur_t->original_pri ? lock_max_pri : cur_t->original_pri;
    } else {
      cur_t->priority = cur_t->original_pri;
    }
  }
  

  lock->holder = NULL;
  sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
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
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0); 
  list_push_back (&cond->waiters, &waiter.elem);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) {
    list_sort(&cond->waiters, cond_cmp, NULL);
    sema_up (&list_entry (list_pop_front (&cond->waiters), struct semaphore_elem, elem)->semaphore);
  }
}

    

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}

static bool
sema_cmp(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  int a_priority = list_entry(a, struct thread, elem)->priority;
  int b_priority = list_entry(b, struct thread, elem)->priority;
  if (a_priority > b_priority) return 1;
  return 0;
}

static bool
cond_cmp(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  struct semaphore_elem *c_cond = list_entry(a, struct semaphore_elem, elem);
  struct semaphore_elem *d_cond = list_entry(b, struct semaphore_elem, elem);
  struct semaphore c_sema = c_cond->semaphore;
  struct semaphore d_sema = d_cond->semaphore;
  /* 一个信号量只有一个线程阻塞. */
  struct list_elem *c_sema_elem = list_front(&c_sema.waiters);
  struct list_elem *d_sema_elem = list_front(&d_sema.waiters);
  struct thread *c_thread = list_entry(c_sema_elem, struct thread, elem);
  struct thread *d_thread = list_entry(d_sema_elem, struct thread, elem);
  int c_priority = c_thread->priority;
  int d_priority = d_thread->priority;
  if (c_priority > d_priority) return 1;
  return 0;
}

static bool
lock_cmp(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  // 比较 锁 锁住的线程中的最大优先级 
  int a_priority = list_entry(a, struct lock, elem)->max_pri;
  int b_priority = list_entry(b, struct lock, elem)->max_pri;
  if (a_priority > b_priority) return 1;
  return 0;
}