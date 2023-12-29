/*
 * contains the implementation of all syscalls.
 */

#include <stdint.h>
#include <errno.h>

#include "util/types.h"
#include "syscall.h"
#include "string.h"
#include "process.h"
#include "util/functions.h"

#include "spike_interface/spike_utils.h"

#include "elf.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  sprint(buf);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  sprint("User exit with code:%d.\n", code);
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()
  shutdown(code);
}



//
//lab1_challeng1:添加main.c中调用的print_backtrace函数
//
ssize_t sys_user_backtrace(uint64 n){
  uint64 *now_fp = (uint64 *)current->trapframe->regs.s0;
  uint64 *last_fp = (uint64 *) *(now_fp - 1);
  while(last_fp && (n--)>0){
    now_fp = last_fp;
    uint64 *ra=(uint64 *) *(now_fp - 1);
    last_fp = (uint64 *) *(now_fp - 2);
    uint64 x = (uint64)ra;
    for(int i=0;i<symtab_size_elf;i+=24){
      elf_symtab *symtab = (elf_symtab *)(symtab_addr_elf+i);
      int type = symtab->info & 0xf;
      if(type == 2 && x<=symtab->value+symtab->size && x>=symtab->value){
        uint64 addr = symtab->value;
        char *name = (char *)(strtab_addr_elf + symtab->name);
        sprint("%s\n",name);
        break;
      }
    }
    
  }
  return 0;
}


//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
  switch (a0) {
    case SYS_user_print:
      return sys_user_print((const char*)a1, a2);
    case SYS_user_exit:
      return sys_user_exit(a1);
    //lab1_challeng1:添加main.c中调用的print_backtrace函数
    case SYS_user_backtrace:
      return sys_user_backtrace(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
