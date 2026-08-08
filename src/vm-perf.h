#ifndef VM_PERF_H
#define VM_PERF_H

#include <stdint.h>

// extern FILE* logFile

// initialize checker and output FILE*
//   verbosity > 1 --> output FILE* = stdout
//   o.w. --> output FILE* = fopen("log.txt", w+);
//   should not fail
// extern void initPerf(int verbosity);

// increments translateNum and logs translation of vAddr to pAddr
//   should not fail
// extern void traslated(uint32_t vAddr, uint32_t pAddr);

// increments pagefaultNum and logs which address was called
//   should not fail
// extern void pagefaulted(uint32_t virtualAddress);

// increments swapNum and logs which swapID was called
//   should not fail
// extern void swapped(uint32_t swap);

// prints performance score along with number each points
//   should not fail
// extern void printPerf(void);

// frees performance
//   fails upon null of checker
// extern void freePerf(void);

struct checker {
    size_t translateNum;
    size_t pagefaultNum;
    size_t swapNum;
};

#endif
