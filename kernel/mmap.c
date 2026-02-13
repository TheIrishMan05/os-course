#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"

static inline int prot_to_perm(int prot){
  int perm = PTE_U;
  if(prot & PROT_READ)
    perm |= PTE_R;
  if(prot & PROT_WRITE)
    perm |= PTE_W;
  if(prot & PROT_EXEC)
    perm |= PTE_X;
  return perm;
}


int mmap(uint64 addr, uint64 length, uint64 offset, int prot, int flags, int fd){
  struct proc *p = myproc();
  struct file *f = 0;
  if(!(flags & MAP_ANONYMOUS)) {
    if(fd < 0 || fd >= NOFILE)
      return MAP_FAILED;
    if((f = p->ofile[fd]) == 0)
      return MAP_FAILED;

    if((prot & PROT_READ) && !(f->readable))
      return MAP_FAILED;
    if((prot & PROT_WRITE) && !(f->writable))
      return MAP_FAILED;

    if((flags & MAP_SHARED) && (prot & PROT_WRITE) && !f->writable)
      return MAP_FAILED;

    filedup(f);
  }
  int vma_idx = -1;
  for(uint64 i = 0; i < NVMA; i++){
    if(p->vma_array[i].used == 0){
      vma_idx = i;
      break;
    }
  }

  if(vma_idx == -1){
    if(f != 0) {
      fileclose(f);
    }
    return MAP_FAILED;
  }
  if((flags & MAP_SHARED) && f != 0) {
    struct proc* src_proc = find_proc_with_mapping(f, offset, length);
    if(src_proc) {
      acquire(&src_proc->lock);
      for(int i = 0; i < NVMA; i++) {
        if(src_proc->vma_array[i].used && 
           src_proc->vma_array[i].file == f &&
           src_proc->vma_array[i].offset == offset &&
           src_proc->vma_array[i].length >= length) {
          uint64 map_addr = PGROUNDUP(p->sz); 
          p->sz = map_addr + length;
          
          if(mappages_with_transfer(p->pagetable, map_addr, length,
                                   src_proc->vma_array[i].addr,
                                   prot_to_perm(prot),
                                   src_proc->pagetable) == 0) {
            p->vma_array[vma_idx].used = 1;
            p->vma_array[vma_idx].addr = addr;
            p->vma_array[vma_idx].length = length;
            p->vma_array[vma_idx].prot = prot;
            p->vma_array[vma_idx].flags = flags;
            p->vma_array[vma_idx].file = f;
            p->vma_array[vma_idx].offset = offset;
            p->vma_array[vma_idx].pa_start = src_proc->vma_array[i].pa_start;
            p->vma_array[vma_idx].src_pgdir = src_proc->pagetable;
            filedup(f);
            release(&src_proc->lock);
            return addr;
          } else {
            p->sz = map_addr;
            release(&src_proc->lock);
            return MAP_FAILED;
          }
        } else {
          return MAP_FAILED;
        }
      }
    }
    return MAP_FAILED;
  } else if((flags & MAP_PRIVATE) && f != 0) {
    struct proc* src_proc = find_proc_with_mapping(f, offset, length);
    if(src_proc) {
      for(int i = 0; i < NVMA; i++) {
        if(src_proc->vma_array[i].used && 
           src_proc->vma_array[i].file == f &&
           src_proc->vma_array[i].offset == offset &&
           src_proc->vma_array[i].length >= length) {
          uint64 map_addr = PGROUNDUP(p->sz); 
          p->sz = map_addr + length;
          
          if(uvmcopy_range(src_proc->pagetable, p->pagetable,
                           src_proc->vma_array[i].addr, map_addr, length) == 0) {
            p->vma_array[vma_idx].used = 1;
            p->vma_array[vma_idx].addr = addr;
            p->vma_array[vma_idx].length = length;
            p->vma_array[vma_idx].prot = prot;
            p->vma_array[vma_idx].flags = flags;
            p->vma_array[vma_idx].file = f;
            p->vma_array[vma_idx].offset = offset;
            p->vma_array[vma_idx].pa_start = src_proc->vma_array[i].pa_start;
            p->vma_array[vma_idx].src_pgdir = src_proc->pagetable;
            filedup(f);
            return addr;
          } else {
            p->sz = map_addr;
            return MAP_FAILED;
          }
        } else {
          return MAP_FAILED;
        }
      }
    }
    return MAP_FAILED;
  } else {
    if(f != 0) {
      fileclose(f);
    }
    return MAP_FAILED;
  }
}