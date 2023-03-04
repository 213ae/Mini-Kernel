#include "printk.h"
#include "sbi.h"
#include "proc.h"
#include "defs.h"

extern void test();
extern void mm_init();
extern void task_init();

int start_kernel() {
    printk("idle process is running!\n");
    do_timer();
    test(); // DO NOT DELETE !!!
    return 0;
}