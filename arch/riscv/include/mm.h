#include "types.h"

struct run {
    struct run* next;
};

uint64 kalloc();
void kfree(uint64);
void mm_init();