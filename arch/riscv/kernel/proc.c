#include "proc.h"
#include "mm.h"
#include "vm.h"
#include "rand.h"
#include "printk.h"

extern void clock_set_next_event();
extern void __dummy();
extern void __switch_to(struct task_struct* prev, struct task_struct* next);

struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[512]; // 线程数组，所有的线程都保存在此
uint64 task_idx;

void task_init() {
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle
    idle = (struct task_struct*)kalloc();
    idle->state = TASK_RUNNING;
    idle->counter = 0;
    idle->priority = 0;
    idle->pid = task_idx++;
    current = idle;
    task[0] = idle;

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, counter 为 0, priority 使用 rand() 来设置, pid 为该线程在线程数组中的下标。
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址， `sp` 设置为 该线程申请的物理页的高地址
    uint64 va, pa, sz;
    uint64 status = csr_read(sstatus);
    //csr_write(sstatus, status | SSTATUS_SUM);
    for (int i = 1; i < NR_TASKS; i++) {
        task[i] = (struct task_struct*)kalloc();

        task[i]->state = TASK_RUNNING;
        task[i]->counter = 0;
        task[i]->priority = rand() % PRIORITY_MAX + PRIORITY_MIN;
        task[i]->pid = task_idx++;

        task[i]->thread.ra = (uint64)__dummy;
        task[i]->thread.sp = (uint64)task[i] + PGSIZE;

        task[i]->thread.sscratch = USER_END;
        task[i]->thread.sstatus = SSTATUS_SD | SSTATUS_FS | SSTATUS_SUM | SSTATUS_SPIE;
        task[i]->thread.sepc = USER_START;

        task[i]->pgd = (pagetable_t)ualloc();
        task[i]->mm = mm_alloc();

        task[i]->thread_info = ti_alloc();
        task[i]->thread_info->user_sp = NULL;

        // 复制内核页表，这样U模式切换成S模式时，不需要切换页表
        for (int j = 0; j < 512; ++j)
            if (swapper_pg_dir[j]) task[i]->pgd[j] = swapper_pg_dir[j];

        // do_mmap user stack U|-|W|R|V
        va = USER_END - PGSIZE;
        // pa = ualloc() - PA2KVA_OFFSET;
        sz = PGSIZE;
        do_mmap(task[i]->mm, va, sz, VM_WRITE | VM_READ);
        // create_mapping(task[i]->pgd, va, pa, sz, U | R | W | V);

        // do_mmap uapp U|X|W|R|V
        va = USER_START;
        // pa = PGROUNDDOWN((uint64)uapp_start) - PA2KVA_OFFSET;
        sz = PGROUNDUP((uint64)uapp_end) - PGROUNDDOWN((uint64)uapp_start);
        // create_mapping(task[i]->pgd, va, pa, sz, U | X | R | W | V);
        do_mmap(task[i]->mm, va, sz, VM_EXEC | VM_WRITE | VM_READ);

        task[i]->pgd -= PA2KVA_OFFSET >> 3;
    }

    //csr_write(sstatus, status & (~SSTATUS_SUM));

    printk("...proc_init done!\n");
}

void dummy() {
    uint64 MOD = 1000000007;
    int last_counter = -1;
    while (1) {
        if (last_counter == -1 || current->counter != last_counter) {
            last_counter = (int)current->counter;
            printk("[PID = %d] is running. counter = %d. thread space begin at %lx\n", current->pid, current->counter, current);
        }
    }
}

void switch_to(struct task_struct* next) {
    if (current->pid != next->pid) {
        struct task_struct* prev = current;
        current = next;
        __switch_to(prev, current);
    }
}

void do_timer(void) {
    clock_set_next_event();
    if (current->pid == 0) {
        // 1. 如果当前线程是 idle 线程 直接进行调度
        schedule();
    } else {
        // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间任然大于0 则直接返回 否则进行调度*/
        if (--current->counter == 0) {
            schedule();
        }
    }
}

#ifdef DSJF
void schedule(void) { //用有序数组维护
    int sum = 0;
    for (int i = 1; i < task_idx; i++) {
        sum += task[i]->counter;
        if (sum) break;
    }
    if (sum == 0) {
        printk("\n");
        for (int i = 1; i < task_idx; i++) {
            task[i]->counter = rand() % TIME_MAX + TIME_MIN;
            printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
        }
    }

    int min_time = TIME_MAX + 1;
    int idx = 0;
    for (int i = 1; i < task_idx; i++) {
        uint64 time = task[i]->counter;
        uint64 state = task[i]->state;
        if (time != 0 && state == TASK_RUNNING && time < min_time) {
            min_time = task[i]->counter;
            idx = i;
        }
    }

    printk("\nswitch to [PID = %d COUNTER = %d]\n", task[idx]->pid, task[idx]->counter);
    switch_to(task[idx]);
}
#else
void schedule(void) { //用有序数组维护
    int sum = 0;
    for (int i = 1; i < task_idx; i++) {
        sum += task[i]->counter;
        if (sum) break;
    }
    if (sum == 0) {
        printk("\n");
        for (int i = 1; i < task_idx; i++) {
            task[i]->counter = task[i]->priority;
            printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
        }
    }

    int max_prior = PRIORITY_MIN - 1;
    int idx = 0;
    for (int i = 1; i < task_idx; i++) {
        uint64 prior = task[i]->counter;
        uint64 state = task[i]->state;
        if (prior != 0 && state == TASK_RUNNING && prior > max_prior) {
            max_prior = task[i]->counter;
            idx = i;
        }
    }

    printk("\nswitch to [PID = %d COUNTER = %d]\n", task[idx]->pid, task[idx]->counter);
    switch_to(task[idx]);
}
#endif
