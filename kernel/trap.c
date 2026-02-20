#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "fcntl.h"
#include "file.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  if(killed(p))
    exit(-1);

  p->trapframe->epc = r_sepc();

  if(r_scause() == 8){
    if(killed(p))
      exit(-1);

    p->trapframe->epc += 4;
    intr_on();
    syscall();

  } else if((which_dev = devintr()) != 0){
    // ok

  } else if(r_scause() == 12 || r_scause() == 13 || r_scause() == 15){
    uint64 bad = r_stval();
    uint64 va  = PGROUNDDOWN(bad);

    if(va >= MAXVA || va >= TRAPFRAME || va >= p->sz) {
      setkilled(p);
      goto done;
    }

    pte_t *pte = walk(p->pagetable, va, 0);

    if(pte == 0 || (*pte & PTE_V) == 0){
      if(pte && (*pte & PTE_STOLEN)) {
        setkilled(p);
        goto done;
      }
      struct vma *v = 0;
      for(int i = 0; i < NVMA; i++){
        if(p->vma_array[i].valid && va >= p->vma_array[i].addr && va < p->vma_array[i].addr + p->vma_array[i].len){
          v = &p->vma_array[i];
          break;
        }
      }

      char *mem = kalloc();
      if(mem == 0)
        exit(-1);
      memset(mem, 0, PGSIZE);

      if(v && v->f){
        ilock(v->f->ip);
        uint64 off = v->offset + (va - v->addr);
        int load_n = PGSIZE;
        if(va + PGSIZE > v->addr + v->len){
          load_n = (v->addr + v->len) - va;
        }
        readi(v->f->ip, 0, (uint64)mem, off, load_n);
        iunlock(v->f->ip);
      }

      int pte_flags = PTE_R|PTE_U;
      if(v){
        if(v->prot & PROT_WRITE) pte_flags |= PTE_W;
      } else {
        pte_flags |= PTE_W;
      }

      if(mappages(p->pagetable, va, PGSIZE, (uint64)mem, PTE_R|PTE_W|PTE_U) != 0){
        kfree(mem);
        setkilled(p);
        goto done;
      }
      sfence_vma();
      goto done;
    }

    if((*pte & PTE_U) == 0){
      setkilled(p);
      goto done;
    }

    if((*pte & PTE_COW) && ((*pte & PTE_W) == 0) && (r_scause() == 15)){
      uint64 pa = PTE2PA(*pte);

      if(pa == 0){
        printf("usertrap: zero pa in COW handling\n");
        setkilled(p);
        goto done;
      }

      if(kref(pa) == 1){
        *pte = (*pte | PTE_W) & ~PTE_COW;
        sfence_vma();
        goto done;
      }

      char *mem = kalloc();
      if(mem == 0){
        setkilled(p);
        goto done;
      }

      memmove(mem, (void*)pa, PGSIZE);

      uint flags = PTE_FLAGS(*pte);
      flags = (flags | PTE_W) & ~PTE_COW;

      *pte = PA2PTE(mem) | flags | PTE_V;
      sfence_vma();

      kfree((void*)pa);
      goto done;
    }   
    setkilled(p);

  } else {
    setkilled(p);
  }

done:
  if(killed(p))
    exit(-1);

  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    // interrupt or trap from an unknown source
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  if(cpuid() == 0){
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
  }

  // ask for the next timer interrupt. this also clears
  // the interrupt request. 1000000 is about a tenth
  // of a second.
  w_stimecmp(r_time() + 1000000);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if(scause == 0x8000000000000009L){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    // timer interrupt.
    clockintr();
    return 2;
  } else {
    return 0;
  }
}

