// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];           // 为每个CPU维护一个结构体

char* kmem_lock_names[] = {   // 锁名称数组
  "kmem_cpu_0",
  "kmem_cpu_1",
  "kmem_cpu_2",
  "kmem_cpu_3",
  "kmem_cpu_4",
  "kmem_cpu_5",
  "kmem_cpu_6",
  "kmem_cpu_7",  
};

void
kinit()
{
  for(int i = 0; i < NCPU; i++) {           // 为每个CPU初始化锁
    initlock(&kmem[i].lock, kmem_lock_names[i]);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();           // 关闭中断

  int cpu = cpuid();    // 获取cpu

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;   // 头插法插到空闲链表表头
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);

  pop_off();           // 开中断
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();           // 关闭中断

  int cpu = cpuid();    // 获取cpu

  acquire(&kmem[cpu].lock); // 加锁

  if(kmem[cpu].freelist == 0) {         // 当前CPU没有空闲页了
    int steal_left = 64;
    for(int i = 0; i < NCPU; i++) {
      if(i == cpu)  
        continue;

      acquire(&kmem[i].lock);           // 加锁

      if(kmem[i].freelist == 0) {       // 当前CPU没有空闲页了
        release(&kmem[i].lock);         // 释放锁    
        continue;          
      }

      struct run* rr = kmem[i].freelist;    // 取出当前cpu的空闲页表头
      
      while(rr && steal_left) {             // 当前CPU有空闲页并且还没取够
        kmem[i].freelist = rr->next;
        rr->next = kmem[cpu].freelist;
        kmem[cpu].freelist = rr;
        rr = kmem[i].freelist;
        steal_left--;
      }

      release(&kmem[i].lock);               // 释放锁
      
      if(steal_left == 0)
        break;      
    }
  }


  r = kmem[cpu].freelist;
  if(r)
    kmem[cpu].freelist = r->next;
  release(&kmem[cpu].lock); // 释放锁

  pop_off();           // 开中断

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
