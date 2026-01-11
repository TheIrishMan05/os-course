// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
static struct spinlock ref_lock;
static int refcnt[(PHYSTOP - KERNBASE) / PGSIZE];

static int
pa2idx(uint64 pa)
{
  if(pa < KERNBASE || pa >= PHYSTOP)
    panic("pa2idx");
  return (pa - KERNBASE) / PGSIZE;
}

void
kinit()
{
  char *p = (char *) PGROUNDUP((uint64) end);
  bd_init(p, (void*)PHYSTOP);
  initlock(&ref_lock, "refcnt");
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  uint64 a = (uint64)pa;

  acquire(&ref_lock);
  int idx = pa2idx(a);
  if(refcnt[idx] < 1)
    panic("kfree ref");
  refcnt[idx]--;
  int r = refcnt[idx];
  release(&ref_lock);

  if(r > 0)
    return;

  bd_free(pa);
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  void *p = bd_malloc(PGSIZE);
  if(p == 0)
    return 0;

  acquire(&ref_lock);
  refcnt[pa2idx((uint64)p)] = 1;
  release(&ref_lock);

  return p;
}

void
kinc(uint64 pa)
{
  acquire(&ref_lock);
  int idx = pa2idx(pa);
  if(refcnt[idx] < 1)
    panic("kinc");
  refcnt[idx]++;
  release(&ref_lock);
}

int
kref(uint64 pa)
{
  acquire(&ref_lock);
  int r = refcnt[pa2idx(pa)];
  release(&ref_lock);
  return r;
}


