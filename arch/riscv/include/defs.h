#ifndef _DEFS_H
#define _DEFS_H

#include "types.h"

#define csr_read(csr)                 \
    ({                                \
        register uint64 __v;          \
        asm volatile("csrr %0, " #csr \
                     : "+r"(__v)      \
                     :                \
                     : "memory");     \
        __v;                          \
    })

#define csr_write(csr, val)              \
    ({                                   \
        uint64 __v = (uint64)(val);      \
        asm volatile("csrw " #csr ", %0" \
                     :                   \
                     : "r"(__v)          \
                     : "memory");        \
    })

#define PHY_START 0x0000000080000000
#define PHY_SIZE  128 * 1024 * 1024 // 128MB， QEMU 默认内存大小
#define PHY_END   (PHY_START + PHY_SIZE)

#define PGSIZE            0x1000ul // 4KB
#define PGROUNDUP(addr)   ((addr + PGSIZE - 1) & (~(PGSIZE - 1)))
#define PGROUNDDOWN(addr) (addr & (~(PGSIZE - 1)))

#define OPENSBI_SIZE (0x200000)

#define KVM_START (0xffffffe000000000)
#define KVM_END   (0xffffffff00000000)
#define KVM_SIZE  (KVM_END - KVM_START)

#define PA2KVA_OFFSET (KVM_START - PHY_START)

#define PAGE_SHIFT (12)
#define SV39_MODE  (0x8000ul << 48)

#define USER_START (0x0000000000000000) // user space start virtual address
#define USER_END   (0x0000004000000000) // user space end virtual address

#define panic(str)      \
    ({                  \
        printk(str);    \
        while (1) { ; } \
    })

#define SYS_WRITE    64
#define SYS_GETPID   172
#define SYS_MUNMAP   215
#define SYS_CLONE    220 // fork
#define SYS_MMAP     222
#define SYS_MPROTECT 226

#define STDOUT     1

#endif
