#include "printk.h"
#include "defs.h"

// Please do not modifyS
extern unsigned long get_cycles();

void test() {
    uint64 time = get_cycles();
    while (1) {
        if (get_cycles() - time > 0x100000) {
            time = get_cycles();
            // printk("idle process is running!\n");
        }
    }
}
