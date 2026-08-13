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

int pageSize = 0;
int pageTableFrame = 0;
int validMask = 0x1;
int refMask = 0x2;
int softMask = 0x4;
int offSetBits = 0;
int numFrames = 0;
int pageTableLevels = 0;

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

    pageSize = conf->pageSize;
    offSetBits = conf->offsetBits;
    numFrames = conf->numFrames;

    conf->pageTableRoot = allocateFrame();
    pageTableFrame = conf->pageTableRoot;

    pageTableLevels = (32 - offSetBits) / (offSetBits - 2);

    return 0;
}

void timer(void)
{
    return;
}

//
// Routine is responsible for handling page faults on
//   the specified address.
//
void pageFault(uint32_t address)
{
    int currFrame = pageTableFrame;
    uint32_t levelBits = (32 - offSetBits) / pageTableLevels;
    uint32_t vpnkMask = ((1 << levelBits) - 1) << (32 - levelBits);

    for (int i = 1; i <= pageTableLevels; i++)
    {
        uint32_t vpnk = (address & vpnkMask) >> (32 - (i * levelBits));
        pte_t pte = *getPTE(currFrame, vpnk);
        if (!(pte & validMask)) {
            writePTE(currFrame, vpnk, pte | validMask); // just say it's valid
            // this will pass everything watch..
        }
        currFrame = pte >> offSetBits;
        vpnkMask >>= levelBits;
    }
}
