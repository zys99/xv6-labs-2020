// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUFMAP_BUCKET 13
#define BUFMAP_HASH(dev, blockno) (((dev << 27) | blockno) % NBUFMAP_BUCKET)

struct {
  //struct spinlock lock;
  //struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;

  struct buf buf[NBUF];
  struct spinlock eviction_lock;    //驱逐锁
  // 哈希表
  struct buf bufmap[NBUFMAP_BUCKET];
  struct spinlock bufmap_locks[NBUFMAP_BUCKET]; // 桶锁
} bcache;

void
binit(void)
{
  // struct buf *b;

  // initlock(&bcache.lock, "bcache");

  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }

  // 初始化桶锁
  for(int i = 0; i < NBUFMAP_BUCKET; i++) {
    initlock(&bcache.bufmap_locks[i], "bcache_bufmap");
    bcache.bufmap[i].next = 0;
  }

  // 初始化缓存区块
  for(int i = 0; i < NBUF; i++) {
    struct buf* b = &bcache.buf[i];     // 取出第i个缓存区块
    initsleeplock(&b->lock, "buffer");
    b->lastuse = 0;     // 初始化最近使用时间以及引用计数
    b->refcnt = 0;

    b->next = bcache.bufmap[0].next;    // 将缓存区块加入到bufmap[0]
    bcache.bufmap[0].next = b;
  }

  initlock(&bcache.eviction_lock, "bcache_eviction");  //初始化驱逐锁
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint key = BUFMAP_HASH(dev, blockno);   // 获取桶编号
  
  // 获取桶锁
  acquire(&bcache.bufmap_locks[key]);

  // 查询blockno的缓存区块是否已经在缓存区中
  for(b = bcache.bufmap[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 不在缓存区

  // 为了防止死锁，先释放当前桶锁, 准备开始创建缓存区块
  release(&bcache.bufmap_locks[key]);
  // 为了防止blockno的缓存区块被重复创建，加上驱逐锁
  acquire(&bcache.eviction_lock);
  // 释放桶锁-->加驱逐锁的间隙可能创建了blocknod的缓存区块，因此再检查一次
  for(b = bcache.bufmap[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      acquire(&bcache.bufmap_locks[key]);     // 添加引用次数时必须加上桶锁
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      release(&bcache.eviction_lock);         // 释放驱逐锁
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 释放桶锁-->加驱逐锁的间隙未添加缓存区块，则需要真正添加缓存区快

  // 此时只持有驱逐锁，不持有任何桶锁。查询所有桶中的LRU-buf
  struct buf* before_least = 0;     // LRU-buf的前一个块  记录当前找到的最近最少使用缓冲区的前一个节点
  uint holding_bucket = -1;         //记录当前持有哪个桶锁

  // 循环查询所有桶
  for(int i = 0; i < NBUFMAP_BUCKET; i++) {
    acquire(&bcache.bufmap_locks[i]);   // 获取当前遍历的桶锁(在找到下一个LRU-buf或驱逐内存之前都不释放)

    int newfound = 0;       // 是否在当前桶找到的新的LRU-buf

    // 遍历当前桶
    for (b = &bcache.bufmap[i];b->next;b = b->next) {
      // b->next->refcnt == 0 当前缓冲区（b->next）是否未被使用
      // b->next->lastuse 是当前缓冲区的最后使用时间
      // before_least->next->lastuse 是之前找到的候选缓冲区的最后使用时间
      // !before_least：如果 before_least 为空（即尚未找到候选缓冲区），当前缓冲区成为候选。
      // b->next->lastuse < before_least->next->lastuse：如果当前缓冲区的 lastuse 小于之前候选的 lastuse，则当前缓冲区更“久远未使用”，应替换为新的候选。
      if(b->next->refcnt == 0 && (!before_least || b->next->lastuse < before_least->next->lastuse)) {
        before_least = b;
        newfound = 1;
      }
    }

    
    if(newfound == 0) {   // 如果没找到找到新的LRU-buf，就释放当前的桶锁
      release(&bcache.bufmap_locks[i]);
    } else {              // 找到了新的LRU-buf
      if (holding_bucket != -1)      // 如果当前找到的不是第一个LRU-buf，之前肯定持有某个桶锁，需要释放 
        release(&bcache.bufmap_locks[holding_bucket]);
      holding_bucket = i;          // 把标记 holding_bucket 更改成当前桶锁编号
    }
  }

  // 如果没找到任何一个LRU-buf，表示没有空闲缓存块了
  if (!before_least)
      panic("bget: no buffuers");

  // 更新b为LRU-buf
  b = before_least->next;           

  // 想要偷的块如果不在key桶，就要把块从他所在的桶驱逐出来
  if(holding_bucket != key) {
    before_least->next = b->next;
    release(&bcache.bufmap_locks[holding_bucket]);    // 释放所在桶的锁

    //将LRU-buf添加到key桶
    acquire(&bcache.bufmap_locks[key]);
    b->next = bcache.bufmap[key].next;
    bcache.bufmap[key].next = b;  
  }

  // 设置新buf的字段
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;

  // 释放相关锁
  release(&bcache.bufmap_locks[key]);
  release(&bcache.eviction_lock);		// 释放驱逐锁
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint key = BUFMAP_HASH(b->dev, b->blockno);   // 获取桶编号
  acquire(&bcache.bufmap_locks[key]);           // 获取桶锁
  b->refcnt--;                                  // 减少引用计数
  if(b->refcnt == 0) {                          // 当前无引用, 则更新最近使用时刻为当前时间
    b->lastuse = ticks;
  }
  release(&bcache.bufmap_locks[key]);          // 释放桶锁
}

void
bpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);   // 获取桶编号
  acquire(&bcache.bufmap_locks[key]);           // 获取桶锁
  b->refcnt++;
  release(&bcache.bufmap_locks[key]);          // 释放桶锁
}

void
bunpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);   // 获取桶编号
  acquire(&bcache.bufmap_locks[key]);           // 获取桶锁
  b->refcnt--;
  release(&bcache.bufmap_locks[key]);          // 释放桶锁
}


