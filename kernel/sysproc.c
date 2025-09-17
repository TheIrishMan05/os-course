#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

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
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
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

uint64
sys_dump(void)
{
  struct proc *p = myproc();

  uint64 regdump[10];

  regdump[0] = p->trapframe->s2;
  regdump[1] = p->trapframe->s3;
  regdump[2] = p->trapframe->s4;
  regdump[3] = p->trapframe->s5;
  regdump[4] = p->trapframe->s6;
  regdump[5] = p->trapframe->s7;
  regdump[6] = p->trapframe->s8;
  regdump[7] = p->trapframe->s9;
  regdump[8] = p->trapframe->s10;
  regdump[9] = p->trapframe->s11;

  for(int i = 0; i < 10; i++) {
    printf("%ld\n", regdump[i]);
  }

  return 0;
}
