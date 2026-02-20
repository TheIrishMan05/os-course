#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "defs.h"
#include "fcntl.h"
#include "fs.h"
#include "file.h"

void *mmap(uint64 addr, uint64 length, uint64 offset, int prot, int flags, int fd) {
  struct proc *p = myproc();
  struct file *f = 0;

  if(fd >= 0 && fd < NOFILE && p->ofile[fd]) {
    f = p->ofile[fd];
  } else {
    f = 0;
  } 

  if((flags & MAP_ANON) == 0 && f == 0 && !((flags & MAP_SHARED) && (flags & MAP_FIXED)))
    return MAP_FAILED;

   if((flags & MAP_SHARED) && (flags & MAP_FIXED)){
    if(addr == 0 || (addr % PGSIZE) != 0 || (offset % PGSIZE) != 0 || (length % PGSIZE) != 0)
      return MAP_FAILED;
    
    if(steal_memory(fd, offset, addr, length) < 0)
      return MAP_FAILED;
      
    if(addr + length > p->sz)
      p->sz = addr + length;
      
    return (void*)addr;
  }

  if(flags & MAP_ANON){
    uint64 new_addr = p->sz;
    if(addr != 0 && (flags & MAP_FIXED)){
       if(addr != p->sz) return MAP_FAILED;
       new_addr = addr;
    }

    uint64 newsz = p->sz + length;
    newsz = PGROUNDUP(newsz);
    
    if(newsz >= TRAPFRAME)
      return MAP_FAILED;
      
    if(uvmalloc(p->pagetable, p->sz, newsz, PTE_W | PTE_R | PTE_U) == 0){
      return MAP_FAILED;
    }
    
    p->sz = newsz;
    return (void*)new_addr;
  }

  if((flags & MAP_PRIVATE) && !(flags & MAP_ANON)){
    if(f == 0) return MAP_FAILED;
    if((prot & PROT_READ) && !f->readable) return MAP_FAILED;

    struct vma *v = 0;
    for(int i = 0; i < NVMA; i++){
      if(p->vma_array[i].valid == 0){
        v = &p->vma_array[i];
        break;
      }
    }
    if(v == 0) return MAP_FAILED;

    uint64 start = p->sz;
    if(addr != 0 && (flags & MAP_FIXED)){
        if(addr != p->sz) return MAP_FAILED;
        start = addr;
    }
    uint64 newsz = start + length;
    newsz = PGROUNDUP(newsz);

    v->valid = 1;
    v->addr = start;
    v->len = length;
    v->prot = prot;
    v->flags = flags;
    v->f = filedup(f);
    v->offset = offset;

    p->sz = newsz;
    return (void*)start;
  }
  

  return MAP_FAILED;
}


int munmap(uint64 addr, int length){
  struct proc *p = myproc();

  if(addr % PGSIZE != 0 || length < 0) return -1;
  int npages = PGROUNDUP(length) / PGSIZE;
  if(npages == 0) return -1;

  struct vma *v = 0;
  for(int i = 0; i < NVMA; i++){
    if(p->vma_array[i].valid && 
       p->vma_array[i].addr == addr && 
       p->vma_array[i].len == length){
      v = &p->vma_array[i];
      break;
    }
  }

  if(v){
    if(v->f){
       fileclose(v->f);
       v->f = 0;
    }
    v->valid = 0;
  }
  
  uvmunmap(p->pagetable, addr, npages, 1);

  return 0;
}
