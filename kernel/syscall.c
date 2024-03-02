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
#include "sync_utils.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  sprint("hartid = %d: %s\n", read_tp(), buf);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
static int exit_count = 0;
ssize_t sys_user_exit(uint64 code) {
  // sprint("checking at exit\n");
  sprint("hartid = %d: User exit with code:%d.\n", read_tp(), code); // comment:修改打印信息 将cpu的id打印出
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()
  // sprint("checking at exit\n");
  sync_barrier(&exit_count, NCPU);
  // sprint("checking at exit\n");
  if (read_tp() == 0) {
    sprint("hartid = %d: shutdown with code:%d.\n", 0, code); // comment:修改打印信息 将cpu的id打印出
    shutdown(code);
  }
  return 0;  // never return
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
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
