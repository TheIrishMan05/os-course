#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

extern struct spinlock wait_lock;

extern struct proc proc[NPROC];

enum StatusCode {
  ERROR_PERMISSON_DENIED = -1,
  ERROR_KILLED_PROCESS = -2,
  ERROR_OUT_OF_BOUNDS = -3,
  ERROR_WRITE_ERROR = -4
};


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
    printf("s%d = %d\n", i + 2, (uint32) regdump[i]);
  }

  return 0;
}

uint64
sys_dump2(void) {
    struct proc *p = myproc();
    
    int pid;
    int register_num;
    uint64 *return_value;
    uint64 addr;

    struct proc *target_proc;
    
    int found = 0;
    int is_curr = 0;
    int is_parent = 0;

    argint(0, &pid);
    argint(1, &register_num);
    argaddr(2, &addr);

    return_value = &addr;
    
    initlock(&wait_lock, "wait_lock");

    acquire(&wait_lock);
    for (target_proc = proc; target_proc < &proc[NPROC]; target_proc++) {
      acquire(&target_proc->lock);
      if (target_proc->pid == pid) {
        found = 1;
        break;
      }
      release(&target_proc->lock);
    }

    if(!found) {
      release(&wait_lock);
      return ERROR_PERMISSON_DENIED;
    }
    
    acquire(&p->lock);
    is_curr = (p->pid == pid);
    release(&p->lock);

    acquire(&target_proc->parent->lock);
    is_parent = (target_proc->parent->pid == p->pid);
    release(&target_proc->parent->lock);
    
    if(is_curr == 0 && is_parent == 0) {
      release(&target_proc->lock);
      release(&wait_lock);
      return ERROR_PERMISSON_DENIED;
    }

    if(target_proc->killed) {
      release(&target_proc->lock);
      release(&wait_lock);
      return ERROR_KILLED_PROCESS;
    }

    if(register_num < 2 || register_num > 11) {
      release(&target_proc->lock);
      release(&wait_lock);
      return ERROR_OUT_OF_BOUNDS;
    }

    uint64 *regdump = &target_proc->trapframe->s2;
    int index = 0;
    for(int i = 0; i < 10; i++) {
      if(i + 2 == register_num) {
        index = i;
        break;
      }
    }



    if (copyout(p->pagetable, *return_value, (char*)&regdump[index], sizeof(regdump[index])) < 0) {
        return ERROR_WRITE_ERROR;
    }


    return 0;
    
    
}
