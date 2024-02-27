/*
 * Utility functions for process management. 
 *
 * Note: in Lab1, only one process (i.e., our user application) exists. Therefore, 
 * PKE OS at this stage will set "current" to the loaded user application, and also
 * switch to the old "current" process after trap handling.
 */

#include "riscv.h"
#include "strap.h"
#include "config.h"
#include "process.h"
#include "elf.h"
#include "string.h"
#include "vmm.h"
#include "pmm.h"
#include "memlayout.h"
#include "spike_interface/spike_utils.h"

//Two functions defined in kernel/usertrap.S
extern char smode_trap_vector[];
extern void return_to_user(trapframe *, uint64 satp);

// current points to the currently running user-mode application.
process* current = NULL;

// points to the first free page in our simple heap. added @lab2_2
uint64 g_ufree_page = USER_FREE_ADDRESS_START;

//
// switch to a user-mode process
//
void switch_to(process* proc) {
  assert(proc);
  current = proc;

  // write the smode_trap_vector (64-bit func. address) defined in kernel/strap_vector.S
  // to the stvec privilege register, such that trap handler pointed by smode_trap_vector
  // will be triggered when an interrupt occurs in S mode.
  write_csr(stvec, (uint64)smode_trap_vector);

  // set up trapframe values (in process structure) that smode_trap_vector will need when
  // the process next re-enters the kernel.
  proc->trapframe->kernel_sp = proc->kstack;      // process's kernel stack
  proc->trapframe->kernel_satp = read_csr(satp);  // kernel page table
  proc->trapframe->kernel_trap = (uint64)smode_trap_handler;

  // SSTATUS_SPP and SSTATUS_SPIE are defined in kernel/riscv.h
  // set S Previous Privilege mode (the SSTATUS_SPP bit in sstatus register) to User mode.
  unsigned long x = read_csr(sstatus);
  x &= ~SSTATUS_SPP;  // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE;  // enable interrupts in user mode

  // write x back to 'sstatus' register to enable interrupts, and sret destination mode.
  write_csr(sstatus, x);

  // set S Exception Program Counter (sepc register) to the elf entry pc.
  write_csr(sepc, proc->trapframe->epc);

  // make user page table. macro MAKE_SATP is defined in kernel/riscv.h. added @lab2_1
  uint64 user_satp = MAKE_SATP(proc->pagetable);

  // return_to_user() is defined in kernel/strap_vector.S. switch to user mode with sret.
  // note, return_to_user takes two parameters @ and after lab2_1.
  return_to_user(proc->trapframe, user_satp);
}


//lab2_challenge2_code3: added for better_malloc and better_free

void update_free_mcb(MCB* mcb){
  MCB* p = (MCB*)user_va_to_pa(current->pagetable,(void*)mcb);
  if(current->mcb_free == NULL){
    //代表这是一个新的页，该页作为一个很大的内存块用一个MCB进行管理
    p->next = NULL;
    current->mcb_free = mcb;
    sprint("now free_mcb aadr is %p.\n",current->mcb_free->addr);
    return;
  }
  MCB* current_va = current->mcb_free;
  MCB* current_pa = (MCB*)user_va_to_pa(current->pagetable,(void*)current_va);
  if(current_pa->size > p->size){
    p->next = current_va;
    current->mcb_free = p;
    return;
  }
  while(current_pa->next){
    MCB* next_va = current_pa->next;
    MCB* next_pa = (MCB*)user_va_to_pa(current->pagetable,(void*)next_va);
    if(next_pa->size > p->size){
      break;
    }
    current_pa = next_pa;
    current_va = next_va;
  }
  p->next = current_pa->next;
  current_pa->next = p;
  return;
}

void update_used_mcb(MCB* mcb){
  if(current->mcb_used == NULL){
    current->mcb_used = mcb;
  }
  else{
    MCB* p = current->mcb_used;
    while(p->next){
      p = p->next;
    }
    p->next = mcb;
  }
}


void insert_into_block(MCB *mcb_free_now,uint64 n){
  uint64 new_block_addr = mcb_free_now->addr + sizeof(MCB) + n + sizeof(MCB) - mcb_free_now->addr/sizeof(MCB);
  MCB *p = (MCB*)user_va_to_pa(current->pagetable,new_block_addr);
  if(mcb_free_now + mcb_free_now->size > new_block_addr + sizeof(MCB)){
    //还有剩余空间，需要对当前内存块进行切割

  }
}


#define SPACE_OK 1
#define SPACE_NOT_ENOUGH 0
int have_enough_space(int size,uint64* va){
  if(current->mcb_free == NULL){
    return SPACE_NOT_ENOUGH;
  }
  MCB* p = current->mcb_free;
  while(p){
    if(p->size >= size){
      
      return SPACE_OK;
    }
    p = p->next;
  }
  return SPACE_NOT_ENOUGH;
}



void* better_malloc(int n){
  uint64 *va;
  if(have_enough_space(n,&va) == SPACE_NOT_ENOUGH){
    return NULL;
  }
  else{

  }
}

void better_free(uint64 va){
  
}