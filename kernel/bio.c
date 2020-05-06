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

#define NBUCKET 13

extern uint ticks;
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct buf buckets[NBUCKET];
  struct spinlock locks[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  // initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  initlock(&bcache.lock, "bcache");
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    initsleeplock(&b->lock, "buffer");
    b->ticks = 0;
    b->refcnt = 0;
  }
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.locks[i], "bcache.bucket");
    bcache.buckets[i].next = &bcache.buckets[i];
    bcache.buckets[i].prev = &bcache.buckets[i];
  }
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.list.next;
  //   b->prev = &bcache.list;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.list.next->prev = b;
  //   bcache.list.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint id = (dev * PGSIZE + blockno) % NBUCKET;
  acquire(&bcache.locks[id]);

  // Is the block already cached?
  for(b = bcache.buckets[id].next; b != &bcache.buckets[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  acquire(&bcache.lock);
  int min_stamp = 0xffffffff;
  for (struct buf* r = bcache.buf; r < bcache.buf + NBUF; r++) {
    if (r->refcnt == 0 && r->ticks < min_stamp) {
      b = r;
      min_stamp = r->ticks;
    }
  }
  if (min_stamp < 0xffffffff) {
    b->ticks = 0xffffffff;
    release(&bcache.lock);
    acquiresleep(&b->lock);
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->prev = &bcache.buckets[id];
    b->next = bcache.buckets[id].next;
    bcache.buckets[id].next->prev = b;
    bcache.buckets[id].next = b;
    release(&bcache.locks[id]);
    return b;
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b->dev, b, 0);
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
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint id = (b->dev * PGSIZE + b->blockno) % NBUCKET;
  acquire(&bcache.locks[id]);
  b->refcnt--;
  if (b->refcnt > 0) {
    release(&bcache.locks[id]);
    return;
  }
  b->ticks = ticks;
  b->next->prev = b->prev;
  b->prev->next = b->next;
  release(&bcache.locks[id]);
}

void
bpin(struct buf *b) {
  uint id = (b->dev * PGSIZE + b->blockno) % NBUCKET;
  acquire(&bcache.locks[id]);
  b->refcnt++;
  release(&bcache.locks[id]);
}

void
bunpin(struct buf *b) {
  uint id = (b->dev * PGSIZE + b->blockno) % NBUCKET;
  acquire(&bcache.locks[id]);
  b->refcnt--;
  release(&bcache.locks[id]);
}
