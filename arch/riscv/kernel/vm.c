#include "vm.h"
#include "mm.h"
#include "proc.h"
#include "printk.h"
#include "string.h"

/* early_pgtbl: 第一回映射的页表, 大小恰好为一页 4 KiB, 开头对齐页边界 */
unsigned long early_pgtbl[512] __attribute__((__aligned__(0x1000)));

void setup_vm(void) {
    /*
    1. 如前文所说，只使用第一级页表
    2. 因此 VA 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
        high bit 可以忽略
        中间 9 bit 作为 early_pgtbl 的 index
        低 30 bit 作为页内偏移
    3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    */
    //等值映射
    uint64 entry_idx = PHY_START >> (12 + 9 + 9) & 0x1ff;
    long entry = ((long)PHY_START >> 2) | V | R | W | X;
    (early_pgtbl - (PA2KVA_OFFSET >> 3))[entry_idx] = entry;
    //偏移映射
    entry_idx = KVM_START >> (12 + 9 + 9) & 0x1ff;
    (early_pgtbl - (PA2KVA_OFFSET >> 3))[entry_idx] = entry;
}

/* swapper_pg_dir: kernel pagetable 根目录， 在 setup_vm_final 进行映射。 */
unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

void setup_vm_final(void) {
    memset(swapper_pg_dir, 0x0, PGSIZE);

    // No OpenSBI mapping required
    uint64 va, pa, sz;
    // mapping kernel text X|-|R|V
    va = PGROUNDDOWN((uint64)_stext);
    pa = va - PA2KVA_OFFSET;
    sz = PGROUNDUP((uint64)_etext) - va;
    create_mapping(swapper_pg_dir, va, pa, sz, X | R | V);

    // mapping kernel rodata -|-|R|V
    va = PGROUNDDOWN((uint64)_srodata);
    pa = va - PA2KVA_OFFSET;
    sz = PGROUNDUP((uint64)_erodata) - va;
    create_mapping(swapper_pg_dir, va, pa, sz, R | V);

    // mapping other memory -|W|R|V
    va = PGROUNDUP((uint64)uapp_end);
    pa = va - PA2KVA_OFFSET;
    sz = PHY_END - pa;
    create_mapping(swapper_pg_dir, va, pa, sz, W | R | V);

    // uint64* temp_page = swapper_pg_dir;
    // int i = 0;
    // for (; i < 512; i++) {
    //     if (temp_page[i] > 0) {
    //         printk("%d %lx\n", i, temp_page[i]);
    //         break;
    //     }
    // }
    // temp_page = (temp_page[i] & (long)-2) << 2;
    // for (i = 0; i < 512; i++) {
    //     if (temp_page[i] > 0) {
    //         printk("%d %lx\n", i, temp_page[i]);
    //     }
    // }

    // set satp with swapper_pg_dir
    uint64 satpw = ((((uint64)swapper_pg_dir - PA2KVA_OFFSET) >> 12) | SV39_MODE);
    csr_write(satp, satpw);

    // flush TLB
    asm volatile("sfence.vma zero, zero");
    return;
}

/* 创建多级页表映射关系 */
void create_mapping(uint64* pgtbl, uint64 va, uint64 pa, uint64 sz, int perm) {
    /*
    pgtbl 为根页表的基地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的大小
    perm 为映射的读写权限

    创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
    可以使用 V bit 来判断页表项是否存在
    */
    for (uint64 pg_num = sz / PGSIZE; pg_num > 0; pg_num--) {
        uint64* temp_pgt = pgtbl;
        for (uint64 i = 2; i >= 0; i--) {
            uint64 idx = va >> (12 + 9 * i) & 0x1ff;
            if (i != 0 && temp_pgt[idx] == 0) {
                uint64 new_page;
                if (perm & U) {
                    new_page = ualloc() - PA2KVA_OFFSET;
                } else {
                    new_page = kalloc() - PA2KVA_OFFSET;
                }
                temp_pgt[idx] = ((long)new_page >> 2) | ((U | V) & perm);
            } else if (i == 0 && temp_pgt[idx] == 0) {
                temp_pgt[idx] = ((long)pa >> 2) | perm;
                break;
            } else if (i == 0 && temp_pgt[idx] != 0) {
                panic("Page fault exception");
            }
            temp_pgt = (uint64*)(((temp_pgt[idx] & (~0x3ff)) << 2) + PA2KVA_OFFSET);
        }
        va += PGSIZE;
        pa += PGSIZE;
    }
}

uint64 user_pgs[512];
uint64 pg_idx;

void mark_pte(uint64 va) {
    uint64* temp_pgt = swapper_pg_dir;
    for (uint64 i = 2; i >= 0; i--) {
        uint64 idx = va >> (12 + 9 * i) & 0x1ff;
        if (i == 0 && temp_pgt[idx] != 0) {
            temp_pgt[idx] = temp_pgt[idx] | U;
            return;
        } else if (i == 0 && temp_pgt[idx] == 0) {
            panic("Something wrong when remove pte");
        }
        temp_pgt = (uint64*)(((temp_pgt[idx] & ~0x3ff) << 2) + PA2KVA_OFFSET);
    }
}

void mark_usr_pte() {
    for (int i = 0; i < pg_idx; ++i) {
        mark_pte(user_pgs[i]);
    }
    pg_idx = 0;
}

uint64 ualloc() {
    uint64 kva = kalloc();
    //标记分配给用户的页
    user_pgs[pg_idx++] = kva;
    //返回物理地址
    return kva;
}

/*
 * @mm          : current thread's mm_struct
 * @address     : the va to look up
 *
 * @return      : the VMA if found or NULL if not found
 */
struct vm_area_struct* find_vma(struct mm_struct* mm, uint64 addr) {
    struct vm_area_struct* temp_vma = mm->mmap;
    while (temp_vma && temp_vma->vm_end < addr) {
        temp_vma = temp_vma->vm_next;
    }
    if (temp_vma == NULL || temp_vma->vm_start > addr) return NULL;

    return temp_vma;
}

/*
 * @mm     : current thread's mm_struct
 * @addr   : the suggested va to map
 * @length : memory size to map
 * @prot   : protection
 *
 * @return : start va
 */
uint64 do_mmap(struct mm_struct* mm, uint64 addr, uint64 length, int prot) {
    /*
    需要注意的是 do_mmap 中的 addr 可能无法被满足
    比如 addr 已经被占用, 因此需要判断，
    如果不合法，需要重新找到一个合适的位置进行映射。
    */
    struct vm_area_struct* vma = vma_alloc();
    vma->vm_mm = mm;
    vma->vm_flags = prot;

    // 寻找合法addr
    while (find_vma(mm, addr) != NULL || find_vma(mm, addr + length - 1) != NULL) {
        addr = (addr + PGSIZE) % USER_END;
    }
    vma->vm_start = addr;
    vma->vm_end = addr + length;

    // 插入
    struct vm_area_struct* temp_vma = mm->mmap;
    if (temp_vma == NULL || temp_vma->vm_start >= addr + length) {
        vma->vm_next = temp_vma;
        vma->vm_prev = NULL;
        if (temp_vma) temp_vma->vm_prev = vma;
        mm->mmap = vma;
    } else {
        struct vm_area_struct* next_vma = temp_vma->vm_next;
        while (next_vma && next_vma->vm_end < addr) {
            temp_vma = temp_vma->vm_next;
            next_vma = next_vma->vm_next;
        }
        vma->vm_prev = temp_vma;
        temp_vma->vm_next = vma;
        vma->vm_next = next_vma;
        next_vma->vm_prev = vma;
    }

    return vma->vm_start;
}

struct vm_area_struct* vma_alloc() {
    static struct vm_area_struct* vma_array = NULL;
    static uint64 vma_idx = 0;
    if (vma_array == NULL || vma_idx == (PGSIZE / sizeof(struct vm_area_struct))) {
        vma_array = (struct vm_area_struct*)kalloc();
        vma_idx = 0;
    }
    return &vma_array[vma_idx++];
}

struct mm_struct* mm_alloc() {
    static struct mm_struct* mm_array = NULL;
    static uint64 mm_idx = 0;
    if (mm_array == NULL || mm_idx == (PGSIZE / sizeof(struct mm_struct))) {
        mm_array = (struct mm_struct*)kalloc();
        mm_idx = 0;
    }
    return &mm_array[mm_idx++];
}

struct thread_info* ti_alloc() {
    static struct thread_info* ti_array = NULL;
    static uint64 ti_idx = 0;
    if (ti_idx == NULL || ti_idx == (PGSIZE / sizeof(struct thread_info))) {
        ti_array = (struct thread_info*)kalloc();
        ti_idx = 0;
    }
    return &ti_array[ti_idx++];
}

struct pt_regs* pr_alloc() {
    static struct pt_regs* pr_array = NULL;
    static uint64 pr_idx = 0;
    if (pr_idx == NULL || pr_idx == (PGSIZE / sizeof(struct pt_regs))) {
        pr_array = (struct pt_regs*)kalloc();
        pr_idx = 0;
    }
    return &pr_array[pr_idx++];
}