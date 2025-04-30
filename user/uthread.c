#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/* Possible states of a thread: */
#define FREE        0x0
#define RUNNING     0x1
#define RUNNABLE    0x2

#define STACK_SIZE  8192
#define MAX_THREAD  4

struct context {    // 上下文结构体
  uint64 ra;
  uint64 sp;

  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};
struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct context context;       // 上下文内容
};
struct thread all_thread[MAX_THREAD];
struct thread *current_thread;
extern void thread_switch(struct context* old, struct context* new);
              
void 
thread_init(void)
{
  // main() is thread 0, which will make the first invocation to
  // thread_schedule().  it needs a stack so that the first thread_switch() can
  // save thread 0's state.  thread_schedule() won't run the main thread ever
  // again, because its state is set to RUNNING, and thread_schedule() selects
  // a RUNNABLE thread.
  current_thread = &all_thread[0];
  current_thread->state = RUNNING;
}

void 
thread_schedule(void)
{
  struct thread *t, *next_thread;           // t：用于遍历线程列表,寻找可运行的线程   next_thread：指向将被调度的下一个可运行线程 

  /* Find another runnable thread. */
  next_thread = 0;                          // 初始化 next_thread 为 0（空指针）,表示尚未找到可运行线程 
  t = current_thread + 1;                   // current_thread 是当前正在运行的线程的指针, t 被初始化为 current_thread + 1, 表示从当前线程的下一个线程开始查找 
  for(int i = 0; i < MAX_THREAD; i++){      // 遍历线程
    if(t >= all_thread + MAX_THREAD)        // 如果 t 超出了线程数组的末尾 则从头查找 即环形查找 all_thread 是线程数组的起始地址, 包含所有线程的结构体
      t = all_thread;
    if(t->state == RUNNABLE) {              // 当前线程 t 的状态是否为 RUNNABLE（可运行状态） 找到, 跳出循环
      next_thread = t;
      break;
    }
    t = t + 1;
  }

  if (next_thread == 0) {                   
    printf("thread_schedule: no runnable threads\n");
    exit(-1);
  }

  if (current_thread != next_thread) {         /* switch threads?  */   // 检查是否找到可运行线程
    next_thread->state = RUNNING;
    t = current_thread;                     
    current_thread = next_thread;
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
    thread_switch(&t->context, &next_thread->context);    // 切换上下文
  } else                                        // 无需切换
    next_thread = 0;
}

void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE
  t->context.ra = (uint64)func;               // 返回地址,thread_switch线程切换执行完后返回到ra,设置成线程函数func,就可以切换后执行func
  t->context.sp = (uint64)&t->stack + (STACK_SIZE - 1);   // 栈指针,将线程的栈指针指向其独立的栈,栈的生长是从高地址到低地址,所以要将 sp 设置为指向 stack 的最高地址
}

void 
thread_yield(void)
{
  current_thread->state = RUNNABLE;
  thread_schedule();
}

volatile int a_started, b_started, c_started;
volatile int a_n, b_n, c_n;

void 
thread_a(void)
{
  int i;
  printf("thread_a started\n");
  a_started = 1;
  while(b_started == 0 || c_started == 0)           // 如果 b_started == 0 或 c_started == 0（即线程 b 或线程 c 尚未启动），调用 thread_yield()。
    thread_yield();                                 // 确保 thread_a 等待线程 b 和线程 c 都启动后再继续执行，实现线程间的同步。
  
  for (i = 0; i < 100; i++) {
    printf("thread_a %d\n", i);
    a_n += 1;
    thread_yield();
  }
  printf("thread_a: exit after %d\n", a_n);

  current_thread->state = FREE;                     // FREE 通常表示线程已完成执行，可以被回收或重新分配。
  thread_schedule();
}

void 
thread_b(void)
{
  int i;
  printf("thread_b started\n");
  b_started = 1;
  while(a_started == 0 || c_started == 0)         
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_b %d\n", i);
    b_n += 1;
    thread_yield();
  }
  printf("thread_b: exit after %d\n", b_n);

  current_thread->state = FREE;
  thread_schedule();
}

void 
thread_c(void)
{
  int i;
  printf("thread_c started\n");
  c_started = 1;
  while(a_started == 0 || b_started == 0)
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_c %d\n", i);
    c_n += 1;
    thread_yield();
  }
  printf("thread_c: exit after %d\n", c_n);

  current_thread->state = FREE;
  thread_schedule();
}

int 
main(int argc, char *argv[]) 
{
  a_started = b_started = c_started = 0;
  a_n = b_n = c_n = 0;
  thread_init();
  thread_create(thread_a);
  thread_create(thread_b);
  thread_create(thread_c);
  thread_schedule();
  exit(0);
}
