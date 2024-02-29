/*
 * Interface functions between file system and kernel/processes. added @lab4_1
 */

#include "proc_file.h"

#include "hostfs.h"
#include "pmm.h"
#include "process.h"
#include "ramdev.h"
#include "rfs.h"
#include "elf.h"
#include "riscv.h"
#include "vmm.h"
#include "spike_interface/spike_file.h"
#include "spike_interface/spike_utils.h"
#include "util/functions.h"
#include "util/string.h"

//
// initialize file system
//
void fs_init(void) {
  // initialize the vfs
  vfs_init();

  // register hostfs and mount it as the root
  if( register_hostfs() < 0 ) panic( "fs_init: cannot register hostfs.\n" );
  struct device *hostdev = init_host_device("HOSTDEV");
  vfs_mount("HOSTDEV", MOUNT_AS_ROOT);

  // register and mount rfs
  if( register_rfs() < 0 ) panic( "fs_init: cannot register rfs.\n" );
  struct device *ramdisk0 = init_rfs_device("RAMDISK0");
  rfs_format_dev(ramdisk0);
  vfs_mount("RAMDISK0", MOUNT_DEFAULT);
}

//
// initialize a proc_file_management data structure for a process.
// return the pointer to the page containing the data structure.
//
proc_file_management *init_proc_file_management(void) {
  proc_file_management *pfiles = (proc_file_management *)alloc_page();
  pfiles->cwd = vfs_root_dentry; // by default, cwd is the root
  pfiles->nfiles = 0;

  for (int fd = 0; fd < MAX_FILES; ++fd)
    pfiles->opened_files[fd].status = FD_NONE;

  sprint("FS: created a file management struct for a process.\n");
  return pfiles;
}

//
// reclaim the open-file management data structure of a process.
// note: this function is not used as PKE does not actually reclaim a process.
//
void reclaim_proc_file_management(proc_file_management *pfiles) {
  free_page(pfiles);
  return;
}

//
// get an opened file from proc->opened_file array.
// return: the pointer to the opened file structure.
//
struct file *get_opened_file(int fd) {
  struct file *pfile = NULL;

  // browse opened file list to locate the fd
  for (int i = 0; i < MAX_FILES; ++i) {
    pfile = &(current->pfiles->opened_files[i]);  // file entry
    if (i == fd) break;
  }
  if (pfile == NULL) panic("do_read: invalid fd!\n");
  return pfile;
}

//
// open a file named as "pathname" with the permission of "flags".
// return: -1 on failure; non-zero file-descriptor on success.
//
int do_open(char *pathname, int flags) {
  struct file *opened_file = NULL;
  if ((opened_file = vfs_open(pathname, flags)) == NULL) return -1;

  int fd = 0;
  if (current->pfiles->nfiles >= MAX_FILES) {
    panic("do_open: no file entry for current process!\n");
  }
  struct file *pfile;
  for (fd = 0; fd < MAX_FILES; ++fd) {
    pfile = &(current->pfiles->opened_files[fd]);
    if (pfile->status == FD_NONE) break;
  }

  // initialize this file structure
  memcpy(pfile, opened_file, sizeof(struct file));

  ++current->pfiles->nfiles;
  return fd;
}

//
// read content of a file ("fd") into "buf" for "count".
// return: actual length of data read from the file.
//
int do_read(int fd, char *buf, uint64 count) {
  struct file *pfile = get_opened_file(fd);

  if (pfile->readable == 0) panic("do_read: no readable file!\n");

  char buffer[count + 1];
  int len = vfs_read(pfile, buffer, count);
  buffer[count] = '\0';
  strcpy(buf, buffer);
  return len;
}

//
// write content ("buf") whose length is "count" to a file "fd".
// return: actual length of data written to the file.
//
int do_write(int fd, char *buf, uint64 count) {
  struct file *pfile = get_opened_file(fd);

  if (pfile->writable == 0) panic("do_write: cannot write file!\n");

  int len = vfs_write(pfile, buf, count);
  return len;
}

//
// reposition the file offset
//
int do_lseek(int fd, int offset, int whence) {
  struct file *pfile = get_opened_file(fd);
  return vfs_lseek(pfile, offset, whence);
}

//
// read the vinode information
//
int do_stat(int fd, struct istat *istat) {
  struct file *pfile = get_opened_file(fd);
  return vfs_stat(pfile, istat);
}

//
// read the inode information on the disk
//
int do_disk_stat(int fd, struct istat *istat) {
  struct file *pfile = get_opened_file(fd);
  return vfs_disk_stat(pfile, istat);
}

//
// close a file
//
int do_close(int fd) {
  struct file *pfile = get_opened_file(fd);
  return vfs_close(pfile);
}

//
// open a directory
// return: the fd of the directory file
//
int do_opendir(char *pathname) {
  struct file *opened_file = NULL;
  if ((opened_file = vfs_opendir(pathname)) == NULL) return -1;

  int fd = 0;
  struct file *pfile;
  for (fd = 0; fd < MAX_FILES; ++fd) {
    pfile = &(current->pfiles->opened_files[fd]);
    if (pfile->status == FD_NONE) break;
  }
  if (pfile->status != FD_NONE)  // no free entry
    panic("do_opendir: no file entry for current process!\n");

  // initialize this file structure
  memcpy(pfile, opened_file, sizeof(struct file));

  ++current->pfiles->nfiles;
  return fd;
}

//
// read a directory entry
//
int do_readdir(int fd, struct dir *dir) {
  struct file *pfile = get_opened_file(fd);
  return vfs_readdir(pfile, dir);
}

//
// make a new directory
//
int do_mkdir(char *pathname) {
  return vfs_mkdir(pathname);
}

//
// close a directory
//
int do_closedir(int fd) {
  struct file *pfile = get_opened_file(fd);
  return vfs_closedir(pfile);
}

//
// create hard link to a file
//
int do_link(char *oldpath, char *newpath) {
  return vfs_link(oldpath, newpath);
}

//
// remove a hard link to a file
//
int do_unlink(char *path) {
  return vfs_unlink(path);
}


// lab4_challenge2 code2: 加载新应用程序
static void exec_bincode(process *p, char *path)
{
  sprint("Application: %s\n", path);
  // 加载ehdr
  int fp = do_open(path, O_RDONLY);
  // sprint("the fp = %d\n", fp);
  spike_file_t *f = (spike_file_t *)(get_opened_file(fp)->f_dentry->dentry_inode->i_fs_info); // 看第134行。。。。
  elf_header ehdr;
  if (spike_file_read(f, &ehdr, sizeof(elf_header)) != sizeof(elf_header))
  {
      panic("read elf header error\n");
  }
  if (ehdr.magic != ELF_MAGIC)
  {
      panic("do_exec: not an elf file.\n");
  }
  // print_ehdr(&ehdr);

  // 加载代码段 & 数据段
  elf_prog_header ph_addr;
  for (int i = 0, off = ehdr.phoff; i < ehdr.phnum; i++, off += sizeof(ph_addr))
  {
      // step1: 加载程序头entry
      spike_file_lseek(f, off, SEEK_SET); // seek to the program header
      if (spike_file_read(f, &ph_addr, sizeof(ph_addr)) != sizeof(ph_addr))
      {
          panic("read elf program header error\n");
      }
      if (ph_addr.type != ELF_PROG_LOAD) // 通常只有代码段和数据段是ELF_PROG_LOAD的
          continue;
      if (ph_addr.memsz < ph_addr.filesz)
      {
          panic("memsz < filesz error.\n");
      }
      if (ph_addr.vaddr + ph_addr.memsz < ph_addr.vaddr)
      {
          panic("vaddr + memsz < vaddr error.\n");
      }

      // step2: 加载段
      void *pa = alloc_page(); // 分配一页内存
      memset(pa, 0, PGSIZE);
      user_vm_map((pagetable_t)p->pagetable, ph_addr.vaddr, PGSIZE, (uint64)pa, prot_to_type(PROT_WRITE | PROT_READ | PROT_EXEC, 1));
      spike_file_lseek(f, ph_addr.off, SEEK_SET);
      if (spike_file_read(f, pa, ph_addr.memsz) != ph_addr.memsz)
      {
          panic("read program segment error.\n");
      }

      // step3: 填补mapped_info
      int pos;
      for (pos = 0; pos < PGSIZE / sizeof(mapped_region); pos++) // seek the last mapped region
          if (p->mapped_info[pos].va == 0x0)
              break;

      p->mapped_info[pos].va = ph_addr.vaddr;
      p->mapped_info[pos].npages = 1;

      // SEGMENT_READABLE, SEGMENT_EXECUTABLE, SEGMENT_WRITABLE are defined in kernel/elf.h
      if (ph_addr.flags == (SEGMENT_READABLE | SEGMENT_EXECUTABLE))
      {
          p->mapped_info[pos].seg_type = CODE_SEGMENT;
          sprint("CODE_SEGMENT added at mapped info offset:%d\n", pos);
      }
      else if (ph_addr.flags == (SEGMENT_READABLE | SEGMENT_WRITABLE))
      {
          p->mapped_info[pos].seg_type = DATA_SEGMENT;
          sprint("DATA_SEGMENT added at mapped info offset:%d\n", pos);
      }
      else
          panic("unknown program segment encountered, segment flag:%d.\n", ph_addr.flags);

      p->total_mapped_region++;
  }
  // 设置tramframe
  p->trapframe->epc = ehdr.entry;
  do_close(fp); // NOTE:importat!!!!!!!!!!!!!!!!!!!!
  sprint("Application program entry point (virtual address): 0x%lx\n", p->trapframe->epc);
}

int do_exec(char *path)
{
    exec_clean(current);
    exec_bincode(current, path);
    return -1;
}