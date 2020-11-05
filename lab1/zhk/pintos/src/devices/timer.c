#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/self-compute.h"

//#include "threads/thread.c"
//#include "threads/fix.c"
  
/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);
static void time_minus(struct thread *thread, void* aux);

/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (loops_per_tick | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/* Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
void
timer_sleep(int64_t ticks){
  /*
    原函数作用：
      1. 首先获取当前时间
      2. 通过while循环，在ticks时间内，如果线程处于运行状态，就将其放入就绪队列
      3. 会一直占用资源
    更改思路：
      1. 调用该函数时，将当前线程阻塞
      2. 然后设置当前线程的阻塞时间
      3. 通过timer_interrupt()来控制阻塞时间的流动(见timer_interrupt()函数)
      4. 然后在ticks时间过后，将其唤醒
    更改位置：
      timer.c:
        * timer_sleep()
        * timer_interrupt()
        * 尾部新增 timer_minus()
      thread.h line-95:
        * 新增一条字段 waiting_time
      thread.c line-202:
        * 初始化时将 waiting_time 置为 0
  */
  if (ticks <= 0) return; // 防止ticks为负值或零
  ASSERT (intr_get_level() == INTR_ON); // 保证中断的打开
  struct thread *cur_t = thread_current(); // 获取当前线程指针
  cur_t->waiting_time = ticks; // 设置阻塞时间
  enum intr_level old_level = intr_disable();    // 关闭中断 保证原子操作
  thread_block(); // 阻塞线程
  intr_set_level(old_level); // 将中断还原
  /*
    原函数：
    int64_t start = timer_ticks (); // 获取当前时间

    ASSERT (intr_get_level () == INTR_ON);  // 保证中断开启
    while (timer_elapsed (start) < ticks)   // 循环
      thread_yield ();  // 将当前线程放入就绪队列
  */
}

/* Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/* Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}

/* Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  struct thread *cur_t = thread_current();
  ticks++;
  thread_foreach(time_minus, NULL); // 对每一个线程执行time_minus()
  if (thread_mlfqs) {
    
    if (!is_idle(cur_t)) {
      // 每次非空闲线程使用时 recent_cpu++
      cur_t->recent_cpu = add_between_fixedPoints_and_integer(cur_t->recent_cpu, 1);
    }
    if (!(ticks % TIMER_FREQ)){
      // 每个TIMER_FREQ更新recent_cpu
      // recent_cpu = (2*load_avg)/(2*load_avg + 1) * recent_cpu + nice.
      // load_avg = (59/60)*load_avg + (1/60)*ready_threads.
      // load_avg 为 fixed_point
      int ready_threads = get_ready_size();
      if (!is_idle(cur_t)) ready_threads++;
      load_avg = (
        divide_between_fixedPoint_and_integer(multiply_between_fixedPoints_and_integer(load_avg, 59), 60) +
        divide_between_fixedPoint_and_integer(integer_convert_to_fixedPoint(ready_threads), 60)
      );
      struct list *all = get_all_list();
      struct list_elem *elem = list_begin(all); // 获得所有线程列表的头号指针
      while (1) {
        struct thread *t = list_entry(elem, struct thread, allelem);
        if (!is_idle(t)) {
          t->recent_cpu = (
            add_between_fixedPoints_and_integer(
              multiply_between_fixedPoints(
                divide_between_fixedPoints(multiply_between_fixedPoints_and_integer(load_avg, 2), 
                  add_between_fixedPoints_and_integer(multiply_between_fixedPoints_and_integer(load_avg, 2), 1))
              , t->recent_cpu)
            , t->nice)
          );
          t->priority = fixedPoint_convert_to_integer_roundToZero(
            integer_convert_to_fixedPoint(PRI_MAX) - 
            divide_between_fixedPoint_and_integer(t->recent_cpu, 4) - 
            integer_convert_to_fixedPoint(t->nice * 2)
          );
          t->priority = check_priority(t->priority);
        }
        if (elem == list_back(all)) break; 
        elem = list_next(elem);
      }
      /*for (e = list_begin(all); e != list_end(all); e = list_next(e)) {
        struct thread *t = list_entry(e, struct thread, allelem);
        if (!is_idle(t)) {
          t->recent_cpu = (
            add_between_fixedPoints_and_integer(
              multiply_between_fixedPoints(
                divide_between_fixedPoints(multiply_between_fixedPoints_and_integer(load_avg, 2), 
                  add_between_fixedPoints_and_integer(multiply_between_fixedPoints_and_integer(load_avg, 2), 1))
              , t->recent_cpu)
            , t->nice)
          );
          t->priority = fixedPoint_convert_to_integer_roundToZero(
            integer_convert_to_fixedPoint(PRI_MAX) - 
            divide_between_fixedPoint_and_integer(t->recent_cpu, 4) - 
            integer_convert_to_fixedPoint(t->nice * 2)
          );
          t->priority = check_priority(t->priority);
        }
      } */
    }
      
    else if (!(ticks % 4)) {
      // 每4个ticks更新priority
      // priority = PRI_MAX - (recent_cpu / 4) - (nice * 2).
      if (!is_idle(cur_t)){
        cur_t->priority = fixedPoint_convert_to_integer_roundToZero(
          integer_convert_to_fixedPoint(PRI_MAX) - 
          divide_between_fixedPoint_and_integer(cur_t->recent_cpu, 4) - 
          integer_convert_to_fixedPoint(cur_t->nice * 2)
        );
        cur_t->priority = check_priority(cur_t->priority);
      }
      
    }
  }
  thread_tick ();
  
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      /* We're waiting for at least one full timer tick.  Use
         timer_sleep() because it will yield the CPU to other
         processes. */                
      timer_sleep (ticks); 
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing. */
      real_time_delay (num, denom); 
    }
}

/* Busy-wait for approximately NUM/DENOM seconds. */
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}

static void 
time_minus (struct thread *thread, void *aux UNUSED)
{
  /*
    判断当前线程是否为阻塞 如果为阻塞状态 则将其阻塞时间-1
    如果阻塞时间已经减为0 则将其唤醒 放入就绪队列中
  */
  if (thread->status!=THREAD_BLOCKED) {
    return;
  }
  else {
    thread->waiting_time--;
    if (!thread->waiting_time) thread_unblock(thread);
  }
}
