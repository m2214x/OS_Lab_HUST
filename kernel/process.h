#ifndef _PROC_H_
#define _PROC_H_

#include "riscv.h"


// lab2_challenge2_code1: define the MCB_t struct
typedef struct MCB_t{
  uint64 addr;
  uint64 size;
  struct MCB_t* next;
}MCB;


typedef struct trapframe_t {
  // space to store context (all common registers)
  /* offset:0   */ riscv_regs regs;

  // process's "user kernel" stack
  /* offset:248 */ uint64 kernel_sp;
  // pointer to smode_trap_handler
  /* offset:256 */ uint64 kernel_trap;
  // saved user process counter
  /* offset:264 */ uint64 epc;

  // kernel page table. added @lab2_1
  /* offset:272 */ uint64 kernel_satp;
}trapframe;

// the extremely simple definition of process, used for begining labs of PKE
typedef struct process_t {
  // pointing to the stack used in trap handling.
  uint64 kstack;
  // user page table
  pagetable_t pagetable;
  // trapframe storing the context of a (User mode) process.
  trapframe* trapframe;

  // lab2_challenge2_code1: 扩展对虚拟空间的管理
  MCB* mcb_used;
  MCB* mcb_free;
}process;

// switch to run user app
void switch_to(process*);

// current running process
extern process* current;

// address of the first free page in our simple heap. added @lab2_2
extern uint64 g_ufree_page;

// lab2_challenge2_code1: define the function to manage the virtual space
void* better_malloc(int n);
void better_free(uint64 va);

#endif
