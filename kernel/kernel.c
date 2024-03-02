/*
 * Supervisor-mode startup codes
 */

#include "riscv.h"
#include "string.h"
#include "elf.h"
#include "process.h"
#include "riscv-pke/spike_interface/atomic.h"

#include "spike_interface/spike_utils.h"

// process is a structure defined in kernel/process.h
process user_app[NCPU];  // 多核，应该对应多个app
spinlock_t user_app_lock;

//
// load the elf, and construct a "process" (with only a trapframe).
// load_bincode_from_host_elf is defined in elf.c
//
void load_user_program(process *proc) {
  // USER_TRAP_FRAME is a physical address defined in kernel/config.h
  proc->trapframe = (trapframe *)USER_TRAP_FRAME;
  memset(proc->trapframe, 0, sizeof(trapframe));
  // USER_KSTACK is also a physical address defined in kernel/config.h
  proc->kstack = USER_KSTACK;
  proc->trapframe->regs.sp = USER_STACK;

  // load_bincode_from_host_elf() is defined in kernel/elf.c
  load_bincode_from_host_elf(proc);
}

//
// s_start: S-mode entry point of riscv-pke OS kernel.
//
extern spinlock_t cpu_lock;
int s_start(void) {
  int cpuid = read_tp();  // read the hartid
  sprint("hartid = %d: Enter supervisor mode...\n", cpuid);
  // Note: we use direct (i.e., Bare mode) for memory mapping in lab1.
  // which means: Virtual Address = Physical Address
  // therefore, we need to set satp to be 0 for now. we will enable paging in lab2_x.
  // 
  // write_csr is a macro defined in kernel/riscv.h
  write_csr(satp, 0);

  // the application code (elf) is first loaded into memory, and then put into execution
  load_user_program(&user_app[cpuid]);

  sprint("hartid = %d: Switch to user mode...\n", cpuid);
  spinlock_unlock(&cpu_lock);
  // switch_to() is defined in kernel/process.c
  spinlock_lock(&user_app_lock);
  switch_to(&user_app[cpuid]);

  // we should never reach here.
  return 0;
}
