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


#define NBUCKETS 13
#define HASH(blockno) (blockno % NBUCKETS)


struct bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct bucket buckets[NBUCKETS];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

char name[NBUCKETS][10];

void
binit(void)
{
 
  
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKETS; i++) {
    snprintf(name[i], sizeof(name[i]), "bcache%d", i);
    initlock(&bcache.buckets[i].lock, name[i]); 

    bcache.buckets[i].head.prev = &bcache.buckets[i].head; 
    bcache.buckets[i].head.next = &bcache.buckets[i].head; 
  }

  
  // acquire(&bcache.lock);
  for (int i = 0; i < NBUF; i++) {
    b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer"); 

    // Determine the bucket for this buffer
    int bucket_index = i % NBUCKETS; 

    // Add buffer to the bucket's LRU list
    acquire(&bcache.buckets[bucket_index].lock);
    b->next = bcache.buckets[bucket_index].head.next; 
    b->prev = &bcache.buckets[bucket_index].head;
    bcache.buckets[bucket_index].head.next->prev = b;
    bcache.buckets[bucket_index].head.next = b;

    release(&bcache.buckets[bucket_index].lock);
  }
    // release(&bcache.lock);
   
}

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }


// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  int hash = HASH(blockno); 
  struct bucket *bucket = &bcache.buckets[hash];
  struct buf *b;

  acquire(&bucket->lock);
  // printf("get in\n");

  // acquire(&bcache.lock);
  // Is the block already cached?
  for(b = bucket->head.next; b != &bucket->head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket->lock);
      // release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // release(&bucket->lock);

  for(b = bucket->head.prev; b != &bucket->head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }



  // acquire(&bcache.lock);
  for(int i = 0; i < NBUCKETS; i++){
    for(b = bcache.buckets[i].head.prev; b != &bcache.buckets[i].head; b = b->prev){
        if(i == hash){
          continue;
        }
        acquire(&bcache.buckets[i].lock);
        if(b->refcnt == 0) {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;

          // acquire(&bucket->lock);
          b->next->prev = b->prev;
          b->prev->next = b->next;
          b->next = bucket->head.next;
          bucket->head.next->prev = b;
          bucket->head.next = b;
          b->prev = &(bucket->head);
          release(&bucket->lock);
          release(&bcache.buckets[i].lock);
         
          acquiresleep(&b->lock);
          return b;
        }
        release(&bcache.buckets[i].lock);
    }
  }
  // printf("get out\n");
  release(&bucket->lock);

  panic("bget: no buffers");
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
  // printf("brelse in\n");
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);


  int hash = HASH(b->blockno); 
  struct bucket *bucket = &bcache.buckets[hash];

  acquire(&bucket->lock);
  // acquire(&bcache.lock);
  b->refcnt--;

  // release(&bcache.lock);
  release(&bucket->lock);
}

void
bpin(struct buf *b) {
  int hash = HASH(b->blockno); 
  struct bucket *bucket = &bcache.buckets[hash];
  acquire(&bucket->lock);
  // acquire(&bcache.lock);
  b->refcnt++;
  // release(&bcache.lock);
  release(&bucket->lock);
}

void
bunpin(struct buf *b) {
  int hash = HASH(b->blockno); 
  struct bucket *bucket = &bcache.buckets[hash];
  acquire(&bucket->lock);
  // acquire(&bcache.lock);
  b->refcnt--;
  // release(&bcache.lock);
  release(&bucket->lock);
}


