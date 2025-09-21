#include <limits.h>
#include <stdio.h>

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

  uint64 *regdump = &p->trapframe->s2;

  for(int i = 0; i < 10; i++) {
    printf("%ld\n", regdump[i]);
  }

  return 0;
}

uint64
sys_dump2(void) {
  struct proc *p = myproc();
  
  int pid;
  int register_num;
  uint64 *return_value;

  uint64 *regdump = &p->trapframe->s2;
  
  acquire(&p->lock);
  int is_curr = (pid == p->pid);
  release(&p->lock);
  
  acquire(&wait_lock);
  int is_parent = (pid == p->parent->pid);
  release(&wait_lock);
  if (is_curr == 0 && is_parent == 0) {
    return -1;
  } else if (killed(p)) {
    return -2;
  } else if (register_num < 2 || register_num > 11) {
    return -3;
  } else {
    for(int i = 0; i < 10; i++) {
      if (i == register_num) {
        int buffer_length = sizeof(long) * CHAR_BIT * 3 / 3; 
        char buffer[buffer_length];
        
        int chars_written = snprintf(buffer, sizeof(buffer), "%ld", regdump[i]);

        if (chars_written < 0 || chars_written >= sizeof(buffer)) {
          printf("Error of converting or buffer is too small.\n");
        } else {
          copyout(p->pagetable, return_value[i], buffer, chars_written);
        }
      }
    }
  }
  return 0;
}
