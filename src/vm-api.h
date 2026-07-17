#ifndef VM_API_H
#define VM_API_H

#include <stdint.h>

// acquires a new frame
//   returns 0 if no frame is available
//  extern uint32_t allocateFrame(void);

// gives id for swap space
//    returns 0 if no space is available
//  extern uint32_t allocateSwap(void);

// simulator copies the data from page frame to "disk"
//  extern void copyToSwap(uint32_t frame, uint32_t swap);

// simulator copies the data from "disk" to page frame
//  void copyFromSwap(uint32_t swap, uint32_t frame); 

struct config {
    int pageSize;
    int numFrames;
    int pageTableRoot; // -1 if null/root not in memory, else, PA in RAM
    uint32_t (*allocateFrame)(void);
    uint32_t (*allocateSwap)(void);
    void (*copyToSwap)(uint32_t, uint32_t);
    void (*copyFromSwap)(uint32_t, uint32_t);
};

// To use inside of the simulator itself. config is exposed, pageTableSettings isn't
// Page table entry are each 4 bytes SET
// All entries being -1 means they are null
struct pageTableHeader {
    int pageOffsetBits;
    int levels;
};

// The following functions are defined in the
//   student library implementation.
//  extern int initLibrary(struct config* conf);
//  extern void pageFault(uint32_t address);

#endif
