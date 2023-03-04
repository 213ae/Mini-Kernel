#include "string.h"

void* memset(void* dst, int c, uint64 n) {
    char* cdst = (char*)dst;
    for (uint64 i = 0; i < n; ++i)
        cdst[i] = c;

    return dst;
}

void* memcpy(void* to, void* from, uint64 size) {
    char* cfrom = (char*)from;
    char* cto = (char*)to;
    for (uint64 i = 0; i < size; i++) {
        cto[i] = cfrom[i];
    }
    return to;
}