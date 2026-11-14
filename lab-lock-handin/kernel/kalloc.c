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
} kmem;

struct {
  struct spinlock lock;
  struct run *freelist;
} cpu_kmems[NCPU];

// struct kmem kmems[NCPU];
char kname[NCPU][10];

void
kinit()
{
   for (int i = 0; i < NCPU; i++) {
    snprintf(kname[i], sizeof(kname[i]), "kmem%d", i);
    initlock(&(cpu_kmems[i].lock), kname[i]);
    cpu_kmems[i].freelist = 0; 
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  int core_num = 0;
  struct run *r;
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    core_num = core_num % NCPU;
    r = (struct run*)p;
    memset(p, 1, PGSIZE);
    acquire(&cpu_kmems[core_num].lock);
    r->next = cpu_kmems[core_num].freelist;
    cpu_kmems[core_num].freelist = r;
    release(&cpu_kmems[core_num].lock);
    core_num++;
    // kfree(p);
  }
}



// Free the page of physical memory pointed at by pa,
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

  int core_num = cpuid();
  push_off();
  acquire(&cpu_kmems[core_num].lock);
  r->next = cpu_kmems[core_num].freelist;
  cpu_kmems[core_num].freelist = r;
  release(&cpu_kmems[core_num].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.


void *
kalloc(void)
{
  struct run *r;
  struct run *stolenlst;
  int core_num = cpuid();
  push_off();
  acquire(&cpu_kmems[core_num].lock);
  r = cpu_kmems[core_num].freelist;
  if(r){
    cpu_kmems[core_num].freelist = r->next;
    release(&cpu_kmems[core_num].lock);
    pop_off(); 
    memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
  }
  release(&cpu_kmems[core_num].lock);


  
  for(int i = (core_num + 1) % NCPU; i != core_num; i = (i + 1) % NCPU){
    if (i == core_num) continue;
    acquire(&cpu_kmems[i].lock);
    stolenlst = cpu_kmems[i].freelist;
    if(stolenlst){
      cpu_kmems[i].freelist = stolenlst->next;
      release(&cpu_kmems[i].lock);
      pop_off();
      memset((char*)stolenlst, 5, PGSIZE);
      return (void*)stolenlst;
    }

    release(&cpu_kmems[i].lock);
  }
    
  pop_off();

  return 0;
}
