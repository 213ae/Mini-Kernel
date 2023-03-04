#ifndef _PROC_H
#define _PROC_H

#include "defs.h"

#define NR_TASKS (1 + 1) // 用于控制 最大线程数量 （idle 线程 + 31 内核线程）

#define TASK_RUNNING 0 // 为了简化实验，所有的线程都只有一种状态

#define PRIORITY_MIN 1
#define PRIORITY_MAX 10

#define TIME_MIN 1
#define TIME_MAX 5

/* vm_area_struct vm_flags */
#define VM_READ  0x00000001
#define VM_WRITE 0x00000002
#define VM_EXEC  0x00000004

typedef unsigned long* pagetable_t;


struct pt_regs {
    uint64 sstatus;
    uint64 sepc;
    uint64 t6, t5, t4, t3;
    uint64 s11, s10, s9, s8, s7, s6, s5, s4, s3, s2;
    uint64 a7, a6, a5, a4, a3, a2, a1, a0;
    uint64 s1, s0;
    uint64 t2, t1, t0;
    uint64 tp;
    uint64 gp;
    uint64 sp;
    uint64 ra;
    uint64 zero;
};

struct mm_struct {
    struct vm_area_struct* mmap; /* list of VMAs */
};

struct vm_area_struct {
    struct mm_struct* vm_mm; /* The mm_struct we belong to. */
    uint64 vm_start;         /* Our start address within vm_mm. */
    uint64 vm_end;           /* The first byte after our end address
                                   within vm_mm. */

    /* linked list of VM areas per task, sorted by address */
    struct vm_area_struct *vm_next, *vm_prev;

    uint64 vm_flags; /* Flags as listed above. */
};

/* 用于记录 `线程` 的 `内核栈与用户栈指针` */
/* (lab3中无需考虑，在这里引入是为了之后实验的使用) */
struct thread_info {
    uint64 kernel_sp;
    uint64 user_sp;
};

/* 线程状态段数据结构 */
struct thread_struct {
    uint64 ra;
    uint64 sp;
    uint64 s[12];

    uint64 sepc;
    uint64 sstatus;
    uint64 sscratch;
};

/* 线程数据结构 */
struct task_struct {
    struct thread_info* thread_info;

    uint64 state;
    uint64 counter;
    uint64 priority;
    uint64 pid;

    struct thread_struct thread;

    pagetable_t pgd;

    struct mm_struct* mm;
    
    struct pt_regs *trapframe;
};

/* 线程初始化 创建 NR_TASKS 个线程 */
void task_init();

/* 在时钟中断处理中被调用 用于判断是否需要进行调度 */
void do_timer();

/* 调度程序 选择出下一个运行的线程 */
void schedule();

/* 线程切换入口函数*/
void switch_to(struct task_struct* next);

/* dummy funciton: 一个循环程序，循环输出自己的 pid 以及一个自增的局部变量*/
void dummy();

#endif