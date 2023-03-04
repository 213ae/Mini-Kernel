#include "defs.h"
#include "printk.h"
#include "proc.h"
#include "mm.h"
#include "vm.h"
#include "rand.h"
#include "string.h"

extern void clock_set_next_event();
extern void do_timer();
extern void ret_from_fork(struct pt_regs* regs);

extern struct task_struct* current;
extern struct task_struct* task[];
extern uint64 task_idx;

void write(struct pt_regs* regs) {
    if (regs->a0 == STDOUT) {
        regs->a0 = printk("%s", (char*)regs->a1);
    }
}

void getpid(struct pt_regs* regs) {
    regs->a0 = ((struct task_struct*)PGROUNDDOWN(regs->sp - 1))->pid;
}

void forkret() {
    ret_from_fork(current->trapframe);
}

void copy_trapframe(struct pt_regs* to, struct pt_regs* from) {
    memcpy(to, from, sizeof(struct pt_regs));
}

uint64 do_fork(struct pt_regs* regs) {
    /*
    1. 参考 task_init 创建一个新的 子进程, 将 thread.ra 设置为 forkret
    2. 为子进程申请 user stack, 并将父进程的 user stack 数据复制到其
       中. 同时将子进程的 user stack 的地址保存在 thread_info->user_sp 中
    3. 在 task_init 中我们进程的 user-mode sp 可以直接设置为
       USER_END, 但是这里需要设置为当前进程(父进程)的 user-mode sp
    4. 将父进程上下文环境保存到子进程的 trapframe 中
    5. 将子进程的 trapframe->a0 修改为 0
    6. 复制父进程的 mm_struct 以及 vm_area_struct
    7. 创建子进程的页表 这里只做内核的映射,即 swapper_pg_dir
    8. 返回子进程的 pid
    */
    task[task_idx] = (struct task_struct*)kalloc();
    task[task_idx]->state = TASK_RUNNING;
    task[task_idx]->counter = 0;
    task[task_idx]->priority = rand() % PRIORITY_MAX + PRIORITY_MIN;
    task[task_idx]->pid = task_idx;

    task[task_idx]->thread.ra = (uint64)forkret;
    task[task_idx]->thread.sp = (uint64)task[task_idx] + PGSIZE;

    task[task_idx]->thread.sscratch = csr_read(sscratch);
    task[task_idx]->thread.sstatus = regs->sstatus;
    task[task_idx]->thread.sepc = regs->sepc;

    task[task_idx]->pgd = (pagetable_t)ualloc();
    task[task_idx]->mm = mm_alloc();

    // 复制内核页表，这样U模式切换成S模式时，不需要切换页表
    for (int j = 0; j < 512; ++j)
        if (swapper_pg_dir[j]) task[task_idx]->pgd[j] = swapper_pg_dir[j];

    // 复制用户堆栈
    task[task_idx]->thread_info = ti_alloc();
    task[task_idx]->thread_info->user_sp = (uint64)memcpy(ualloc(), PGROUNDDOWN(csr_read(sscratch) - 1), PGSIZE);

    uint64 va, sz;
    // do_mmap user stack U|-|W|R|V
    va = USER_END - PGSIZE;
    sz = PGSIZE;
    do_mmap(task[task_idx]->mm, va, sz, VM_WRITE | VM_READ);

    // do_mmap uapp U|X|W|R|V
    va = USER_START;
    sz = PGROUNDUP((uint64)uapp_end) - PGROUNDDOWN((uint64)uapp_start);
    do_mmap(task[task_idx]->mm, va, sz, VM_EXEC | VM_WRITE | VM_READ);

    task[task_idx]->trapframe = pr_alloc();
    copy_trapframe(task[task_idx]->trapframe, regs);
    task[task_idx]->trapframe->a0 = 0;
    task[task_idx]->trapframe->sp = task[task_idx]->thread.sp;

    task[task_idx]->pgd -= PA2KVA_OFFSET >> 3;

    return task_idx++;
}

uint64 clone(struct pt_regs* regs) {
    return do_fork(regs);
}

void Ucall(struct pt_regs* regs) {
    switch (regs->a7) {
    case SYS_WRITE: write(regs); break;
    case SYS_GETPID: getpid(regs); break;
    case SYS_CLONE: regs->a0 = clone(regs); break;
    default: break;
    }
}

void do_page_fault(struct pt_regs* regs) {
    /*
    1. 通过 stval 获得访问出错的虚拟内存地址（Bad Address）
    2. 通过 sepc 获得当前的 Page Fault 类型
    3. 通过 find_vm() 找到匹配的 vm_area
    4. 通过 vm_area 的 vm_flags 对当前的 Page Fault 类型进行检查
        4.1 Instruction Page Fault      -> VM_EXEC
        4.2 Load Page Fault             -> VM_READ
        4.3 Store Page Fault            -> VM_WRITE
    5. 最后调用 create_mapping 对页表进行映射
    */
    printk("scause = %lx, sepc = %lx, stval = %lx\n", csr_read(scause), csr_read(sepc), csr_read(stval));

    uint64 pgft_addr = csr_read(stval);
    uint64 cause = csr_read(scause);
    struct task_struct* cur_task = (struct task_struct*)PGROUNDDOWN(regs->sp - 1);
    struct vm_area_struct* vma = find_vma(cur_task->mm, pgft_addr);
    if (cause == 12 && !(vma->vm_flags & VM_EXEC) || cause == 13 && !(vma->vm_flags & VM_READ) || cause == 15 && !(vma->vm_flags & VM_WRITE)) {
        return;
    }
    uint64 va, pa, sz;
    va = vma->vm_start;
    sz = vma->vm_end - va;
    if (va == USER_START) {
        pa = PGROUNDDOWN((uint64)uapp_start) - PA2KVA_OFFSET;
    } else {
        if (cur_task->thread_info->user_sp)
            pa = cur_task->thread_info->user_sp - PA2KVA_OFFSET;
        else
            pa = ualloc() - PA2KVA_OFFSET;
    }
    create_mapping(cur_task->pgd + (PA2KVA_OFFSET >> 3), va, pa, sz, U | (vma->vm_flags << 1) | V);
    // mark_usr_pte();
    register uint64 a0 asm("a0") = ((uint64)(cur_task->pgd) >> PAGE_SHIFT) | SV39_MODE;
    asm volatile("csrw satp, a0\n"
                 "sfence.vma" ::"r"(a0));
}

void trap_handler(unsigned long scause, unsigned long sepc, struct pt_regs* regs) {
    // 通过 `scause` 判断trap类型
    if (scause & (0x1ul << 63)) {
        // 如果是interrupt 判断是否是timer interrupt
        if ((scause & (0x8ffffffful)) == 5) {
            // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
            printk("[S] Supervisor Mode Timer Interrupt\n");
            do_timer();
        }
    } else {
        // printk("scause = %lx, sepc = %lx, stval = %lx, ra = %lx\n", csr_read(scause), csr_read(sepc), csr_read(stval), regs->ra);
        switch (scause) {
        case 0: printk("Instruction address misaligned\n"); break;
        case 1: printk("Instruction access fault\n"); break;
        case 2: printk("Illegal instruction\n"); break;
        case 3: printk("Breakpoint\n"); break;
        case 4: printk("Load address misaligned\n"); break;
        case 5: printk("Load access fault\n"); break;
        case 6: printk("Store/AMO address misaligned\n"); break;
        case 7: printk("Store/AMO access fault\n"); break;
        case 8: {
            // printk("Environment call from U-mode\n");
            regs->sepc += 4;
            Ucall(regs);
            break;
        }
        case 9: printk("Environment call from S-mode\n"); break;
        case 12: {
            printk("Instruction page fault\n");
            do_page_fault(regs);
            break;
        }
        case 13: {
            printk("Load page fault\n");
            do_page_fault(regs);
            break;
        }
        case 15: {
            printk("Store/AMO page fault\n");
            do_page_fault(regs);
            break;
        }
        default: break;
        }
    }
    // 其他interrupt / exception 可以直接忽略
}
