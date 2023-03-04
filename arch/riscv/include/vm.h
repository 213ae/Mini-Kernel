#ifndef _VM_H
#define _VM_H
#include "defs.h"
#include "proc.h"

extern uint64 swapper_pg_dir[];

extern char _stext[];
extern char _etext[];
extern char _srodata[];
extern char _erodata[];
extern char uapp_start[];
extern char uapp_end[];

extern void create_mapping(uint64* pgtbl, uint64 va, uint64 pa, uint64 sz, int perm);
extern void mark_usr_pte();
extern uint64 ualloc();
extern struct vm_area_struct* find_vma(struct mm_struct* mm, uint64 addr);
extern uint64 do_mmap(struct mm_struct* mm, uint64 addr, uint64 length, int prot);
extern struct vm_area_struct* vma_alloc();
extern struct mm_struct* mm_alloc();
extern struct thread_info* ti_alloc();
extern struct pt_regs* pr_alloc();

#define SSTATUS_SD   0x1ul << 63
#define SSTATUS_MXR  0x1ul << 19
#define SSTATUS_SUM  0x1ul << 18
#define SSTATUS_FS   0x3ul << 13
#define SSTATUS_SPP  0x1ul << 8
#define SSTATUS_UBE  0x1ul << 6
#define SSTATUS_SPIE 0x1ul << 5
#define SSTATUS_SIE  0x1ul << 1

#define V 0x1
#define R 0x2
#define W 0x4
#define X 0x8
#define U 0x10
#define G 0x20
#define A 0x40
#define D 0x80

#endif
