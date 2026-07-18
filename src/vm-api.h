#ifndef VM_API_H
#define VM_API_H

#include <stdint.h>

typedef uint32_t pte_t;

// acquires a new frame
//   returns 0 if no frame is available
// extern uint32_t allocateFrame(void);

// gives id for swap space
//    returns 0 if no space is available
// extern uint32_t allocateSwap(void);

// simulator copies the data from page frame to "disk"
// extern void copyToSwap(uint32_t frame, uint32_t swap);

// simulator copies the data from "disk" to page frame
// extern void copyFromSwap(uint32_t swap, uint32_t frame);

// simulator gets value of pte entry from frame[index]
// extern pte_t getPTE(uint32_t frame, uint32_t index);

// simulator writes value of pte entry from frame[index]
//    returns -1 if unsuccessful
// extern int writePTE(uint32_t frame, uint32_t index, pte_t pte);

struct config {
    int offsetBits; // wanna get rid of this
    int pageSize;
    int numFrames;
    int pageTableRoot; // -1 if null/root not in memory, else, PA in RAM
    uint32_t (*allocateFrame)(void);
    uint32_t (*allocateSwap)(void);
    void (*copyToSwap)(uint32_t, uint32_t);
    void (*copyFromSwap)(uint32_t, uint32_t);
    pte_t (*getPTE)(uint32_t frame, uint32_t index);
    int (*writePTE)(uint32_t frame, uint32_t index, pte_t pte);
};

// To use inside of the simulator itself. config is exposed, pageTableHeader isn't
// Page table entry are each 4 bytes SET
// All entries being -1 means they are null
struct pageTableHeader {
    int pageSize;
    short pageOffsetBits;
    short levels;
};

// The following functions are defined in the
//   student library implementation.
// extern int initLibrary(struct config* conf);
// extern void pageFault(uint32_t address);

#endif
