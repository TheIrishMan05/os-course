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
static struct spinlock ref_lock;
static int refcnt[(PHYSTOP - KERNBASE) / PGSIZE];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

static int
pageaddr_to_idx(uint64 pa){
  if (pa < KERNBASE || pa >= PHYSTOP)
    panic("pageaddr_to_idx");
  return (pa - KERNBASE) / PGSIZE;
}

void
kinit()
{
  char *p = (char *) PGROUNDUP((uint64) end);
  bd_init(p, (void *)PHYSTOP);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  if (((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  memset(pa, ALLOC_JUNK, PGSIZE);
  bd_free(pa);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  void *p = bd_malloc(PGSIZE);
  if(p) memset(p, JUNK, PGSIZE);
  return p;
}

void
kinc(uint64 pa) {
  acquire(&ref_lock);
  int idx = pageaddr_to_idx(pa);
  if(refcnt[idx] < 1)
    panic("kinc");
  refcnt[idx]++;
  release(&ref_lock);
}
