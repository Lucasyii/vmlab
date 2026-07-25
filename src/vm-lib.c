#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "vm-api.h"

uint32_t (*allocateFrame)(void) = NULL;
uint32_t (*allocateSwap)(void) = NULL;
void (*copyToSwap)(uint32_t, uint32_t) = NULL;
void (*copyFromSwap)(uint32_t, uint32_t) = NULL;
pte_t *(*getPTE)(uint32_t frame, uint32_t index) = NULL;
int (*writePTE)(uint32_t frame, uint32_t index, pte_t pte) = NULL;

//
// initLibrary is what happens to virtual memory during
//   start of process
//
// Should allocate a frame to set PTBR
//
int initLibrary(struct config* conf)
{
    if (conf == NULL) return -1;

    allocateFrame = conf->allocateFrame;
    allocateSwap = conf->allocateSwap;
    copyToSwap = conf->copyToSwap;
    copyFromSwap = conf->copyFromSwap;
    getPTE = conf->getPTE;
    writePTE = conf->writePTE;

    conf->pageTableRoot = allocateFrame();

    return 0;
}

//
// Routine is responsible for handling page faults on
//   the specified address.
//
void pageFault(uint32_t address)
{

}
