#ifndef _ELF_H_
#define _ELF_H_

#include "util/types.h"
#include "process.h"

#define MAX_CMDLINE_ARGS 64

extern uint64 symtab_addr_elf;
extern uint64 symtab_size_elf;
extern uint64 strtab_addr_elf;
extern uint64 strtab_size_elf;

// elf header structure
// 挑战实验中重点关注shoff, shentsize和shnum
typedef struct elf_header_t {
  uint32 magic;
  uint8 elf[12];
  uint16 type;      /* Object file type */
  uint16 machine;   /* Architecture */
  uint32 version;   /* Object file version */
  uint64 entry;     /* Entry point virtual address */
  uint64 phoff;     /* Program header table file offset */
  uint64 shoff;     /* Section header table file offset 节头表的文件偏移量 */
  uint32 flags;     /* Processor-specific flags */
  uint16 ehsize;    /* ELF header size in bytes */
  uint16 phentsize; /* Program header table entry size */
  uint16 phnum;     /* Program header table entry count */
  uint16 shentsize; /* Section header table entry size 节头表的入口大小（以字节为单位）*/
  uint16 shnum;     /* Section header table entry count 节头表的入口数目 */
  uint16 shstrndx;  /* Section header string table index */
  /*.strtab节在节头表的序号,读取elf头又可以知道该值-1恰好是.symtab的序号 */

} elf_header;

// Program segment header.
typedef struct elf_prog_header_t {
  uint32 type;   /* Segment type */
  uint32 flags;  /* Segment flags */
  uint64 off;    /* Segment file offset */
  uint64 vaddr;  /* Segment virtual address */
  uint64 paddr;  /* Segment physical address */
  uint64 filesz; /* Segment size in file */
  uint64 memsz;  /* Segment size in memory */
  uint64 align;  /* Segment alignment */
} elf_prog_header;


// Section header.额外定义了节头表的数据结构，方便后面读取使用
typedef struct elf_section_header_t {
  uint32 name;      /* 节名称（字符串表索引） */
  uint32 type;      /* 节类型 */
  uint64 flags;     /* 节标志 */
  uint64 addr;      /* 执行时的节虚拟地址 */
  uint64 offset;    /* 节在文件中的偏移量 */
  uint64 size;      /* 节的大小（以字节为单位） */
  uint32 link;      /* 链接到另一个节 */
  uint32 info;      /* 附加的节信息 */
  uint64 addralign; /* 节对齐方式 */
  uint64 entsize;   /* 如果节包含表，则为表项大小 */
} elf_section_header;


// Symbol table entry.额外定义了.symtab的数据结构，方便后面读取使用
typedef struct elf_symtab_entry_t {
  uint32 name;      /* 符号名称（字符串表索引） */
  uint8 info;       /* 符号类型和绑定属性 */
  uint8 other;      /* 保留 */
  uint16 shndx;     /* 符号所在节的索引 */
  uint64 value;     /* 符号值,也即所在的地址 */
  uint64 size;      /* 符号大小 */
} elf_symtab;

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian
#define ELF_PROG_LOAD 1

typedef enum elf_status_t {
  EL_OK = 0,

  EL_EIO,
  EL_ENOMEM,
  EL_NOTELF,
  EL_ERR,

} elf_status;

typedef struct elf_ctx_t {
  void *info;
  elf_header ehdr;
} elf_ctx;

elf_status elf_init(elf_ctx *ctx, void *info);
elf_status elf_load(elf_ctx *ctx);

void load_bincode_from_host_elf(process *p);

#endif
