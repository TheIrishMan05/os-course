#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_mmap(void)
{
  uint64 addr, len, offset;
  int prot, flags, fd;

  argaddr(0, &addr);
  argaddr(1, &len);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  argaddr(5, &offset);
  return (uint64)mmap(addr, len, offset, prot, flags, fd);
} 

uint64
sys_munmap(void)
{
  uint64 addr;
  int len;

  argaddr(0, &addr);
  argint(1, &len);

  return munmap(addr, len);
}

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int n;
  struct proc *p = myproc();

  argint(0, &n);

  uint64 addr = p->sz;

  if(n > 0){
    if(p->sz + (uint64)n < p->sz)       
      return -1;
    if(p->sz + (uint64)n >= TRAPFRAME)  
      return -1;
    p->sz += (uint64)n;
    return addr;
  }

  if(n < 0){
    uint64 un = (uint64)(-n);
    if(un > p->sz)
      return -1;
    uint64 newsz = p->sz - un;
    p->sz = uvmdealloc(p->pagetable, p->sz, newsz);
    return addr;
  }

  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}