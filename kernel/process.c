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

void update_free_mcb(uint64 mcb){
  // sprint("update free mcb.\n");
  MCB* p = (MCB*)user_va_to_pa(current->pagetable,(void*)mcb);
  // sprint("pa is %p\n, size is %d\n",p,p->size);
  if(current->mcb_free == -1 ||current->mcb_free%PGSIZE == 0){
    //代表这是一个新的页，该页作为一个很大的内存块用一个MCB进行管理
    p->next = -1;
    current->mcb_free = mcb;
    // sprint("test\n");
    return;
  }
  uint64 current_va = current->mcb_free;
  // sprint("current_va is %p.\n",current_va);
  MCB* current_pa = (MCB*)user_va_to_pa(current->pagetable,(void*)current_va);
  // sprint("current_pa is %p.\n",current_pa);
  if(current_pa->size > p->size){
    //如果链表的第一个块就是预计插入的块
    p->next = current_va;
    current->mcb_free = mcb;
    // sprint("tset\n");
    return;
  }
  while(current_pa->next){
    uint64 next_va = current_pa->next;
    MCB* next_pa = (MCB*)user_va_to_pa(current->pagetable,(void*)next_va);
    if(next_pa->size > p->size){
      break;
    }
    current_pa = next_pa;
    current_va = next_va;
  }
  p->next = current_pa->next;
  current_pa->next = mcb;
  return;
}

void remove_mcb(uint64* head_mab, uint64 va){
  // sprint("remove mcb.\n");
  if(current->mcb_free == -1){
    return;
  }
  MCB* pa = (MCB*)user_va_to_pa(current->pagetable,(void*)*head_mab);
  MCB* next_pa = (MCB*)user_va_to_pa(current->pagetable,(void*)pa->next);
  if(*head_mab == va){
    if(current->mcb_free == va && pa->next==0){
      current->mcb_free = -1;
    }
    else
      *head_mab = pa->next;
    // sprint("now head is %p.\n",*head_mab);
    // sprint("remove mcb success.\n");
    return;
  }
  while(pa->next){
    uint64 next_va = pa->next;
    next_pa = (MCB*)user_va_to_pa(current->pagetable,(void*)next_va);
    if(next_va == va){
      pa->next = next_pa->next;
      return;
    }
  }
}

void update_used_mcb(uint64 mcb){
  MCB* p = (MCB*)user_va_to_pa(current->pagetable,(void*)mcb);
  if(current->mcb_used == -1){
    current->mcb_used = mcb;
  }
  else{
    // mcb->next = current->mcb_used;
    current->mcb_used = mcb;
  }
  return;
}


uint64 insert_into_block(uint64 va, uint64 n){
  // sprint("insert into block.\n");
  // sprint("n is %d.\n",n);
  // sprint("va is %p.\n",va);
  MCB* mcb_free_now = (MCB*)user_va_to_pa(current->pagetable,(void*)va);
  // sprint("pa is %p, size is %d.\n",mcb_free_now,mcb_free_now->size);
  
  uint64 new_block_va = va + n + sizeof(MCB) + (sizeof(MCB) - n%sizeof(MCB));
  // sprint("new block va is %p.\n",new_block_va);

  if(mcb_free_now->size - sizeof(MCB)> n){
    //还有剩余空间，需要对当前内存块进行切割
    MCB *new_block_pa = (MCB*)user_va_to_pa(current->pagetable,(void*)new_block_va);
    // sprint("new block pa is %p.\n",new_block_pa);
    new_block_pa->size = mcb_free_now->size - (n + sizeof(MCB));
    mcb_free_now->size = n;
    // sprint("%d %d\n",new_block_pa->size,mcb_free_now->size);

    remove_mcb(&current->mcb_free,va);
    // sprint("test\n");
    update_free_mcb(new_block_va);
    update_used_mcb(va);
  }
  else{
    //正好用完了或者剩余的内存无法再利用，直接从剩余空间中将该块删除，加入到已用空间中
    remove_mcb(&current->mcb_free,va);
    update_used_mcb(va);
  }
  // sprint("insert into block success.\n");
  return (va+sizeof(MCB));
}

uint64 malloc_new_page(uint64 va,int n){
  // sprint("malloc new page.\n");
  int page_needed = (n + sizeof(MCB) - 1) / PGSIZE + 1;
  uint64 first_page_va = g_ufree_page;
  current->mcb_free = first_page_va;
  uint64 last_page_va = first_page_va + (page_needed-1) * PGSIZE;

  //分配所有需要申请的页面
  for(int i=0;i<page_needed;i++){
    uint64 pa = (uint64)alloc_page();
    // sprint("pa is %p.\n",pa);
		uint64 va = g_ufree_page;
    // sprint("va is %p.\n",va);
		g_ufree_page += PGSIZE;
		memset((void *)pa, 0, PGSIZE); // 页面置'\0'
		// user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, pa,prot_to_type(PROT_WRITE | PROT_READ, 1));
    //将va和pa对应起来
    user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, pa,prot_to_type(PROT_WRITE | PROT_READ, 1));
    MCB *mcb = (MCB*)pa;
    mcb->size = PGSIZE - sizeof(MCB);
  }
  // sprint("test\n");
  // 操纵物理内存
  // sprint("n is %d\n",n);
	insert_into_block(last_page_va, (n+sizeof(MCB))%PGSIZE);
  sprint("%d\n",last_page_va-first_page_va);
  // sprint("malloc new page success.\n");
  // sprint("first_page_va is %p.\n",first_page_va);
	return (first_page_va + sizeof(MCB));
  
}

uint64 malloc_new_page_updated(uint64 va_origin,int n){
  // sprint("malloc new page.\n");
  int x=0;
  uint64 first_page_va = g_ufree_page;
  uint64 va_last = first_page_va;
  if(current->mcb_free != -1){
    va_last = current->mcb_free;
    MCB* pa_last = (MCB*)user_va_to_pa(current->pagetable,(void*)va_last);
    while(pa_last->next != -1){
      // sprint("test\n");
      va_last = pa_last->next;
      // sprint("va_last:%p",va_last);
      pa_last = (MCB*)user_va_to_pa(current->pagetable,(void*)va_last);
    }
    x=pa_last->size;
    // sprint("x: %d\n",x);
    insert_into_block(va_last,x);
  }
  else{
    current->mcb_free = first_page_va;
  }
  int page_needed = (n + sizeof(MCB) - 1-x) / PGSIZE + 1;
  uint64 last_page_va = first_page_va + (page_needed-1) * PGSIZE;
  //分配所有需要申请的页面
  for(int i=0;i<page_needed;i++){
    uint64 pa = (uint64)alloc_page();
    // sprint("pa is %p.\n",pa);
    uint64 va = g_ufree_page;
    // sprint("va is %p.\n",va);
    g_ufree_page += PGSIZE;
    memset((void *)pa, 0, PGSIZE); // 页面置'\0'
    // user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, pa,prot_to_type(PROT_WRITE | PROT_READ, 1));
    //将va和pa对应起来
    user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, pa,prot_to_type(PROT_WRITE | PROT_READ, 1));
    MCB *mcb = (MCB*)pa;
    mcb->size = PGSIZE - sizeof(MCB);
  }
  // sprint("test\n");
  // 操纵物理内存
  // sprint("n is %d\n",n);
  
  insert_into_block(last_page_va, (n-x)%PGSIZE);
  // sprint("%d\n",last_page_va-first_page_va);
  // sprint("malloc new page success.\n");
  // sprint("first_page_va is %p.\n",first_page_va);
  // sprint("va_last: %p\n",va_last);
  return (va_last + sizeof(MCB));
  
}

#define SPACE_OK 1
#define SPACE_NOT_ENOUGH 0
int have_enough_space(int size,uint64* va){
  if(current->mcb_free == -1){
    return SPACE_NOT_ENOUGH;
  }
  // sprint("current->mcb_free is %p.\n",current->mcb_free);
  uint64 va_available = current->mcb_free;
  while(va_available!=-1){
    MCB* pa = (MCB*)user_va_to_pa(current->pagetable,(void*)va_available);
    // sprint("pa is %p.\n",pa);
    if(pa->size >= size){
      *va = va_available;
      return SPACE_OK;
    }
    va_available = pa->next;
  }
  return SPACE_NOT_ENOUGH;
}



uint64 better_malloc(int n){
  // sprint("malloc size is %d.\n",n);
  uint64 va;
  uint64 ret=0;
  // sprint("current free va is %p\n",current->mcb_free);
  if(have_enough_space(n,&va) == SPACE_NOT_ENOUGH){
    // sprint("no enough space for malloc.\n");
    ret=malloc_new_page_updated(va,n);
  }
  else{
    // sprint("va is %p.\n",va);
    ret=insert_into_block(va,n);
  }
  // sprint("ret: %p\n",ret);
  // sprint("now free size: %d\n",((MCB*)user_va_to_pa(current->pagetable,(void*)current->mcb_free))->size);
  return ret;
}

void better_free(uint64 va){
  uint64 p = va - sizeof(MCB);
  remove_mcb(&current->mcb_used, p);
  update_free_mcb(p);
}