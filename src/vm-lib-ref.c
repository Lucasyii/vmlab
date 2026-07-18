#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "vm-api.h"

uint32_t (*allocateFrame)(void) = NULL;
uint32_t (*allocateSwap)(void) = NULL;
void (*copyToSwap)(uint32_t, uint32_t) = NULL;
void (*copyFromSwap)(uint32_t, uint32_t) = NULL;
pte_t (*getPTE)(uint32_t frame, uint32_t index) = NULL;
int (*writePTE)(uint32_t frame, uint32_t index, pte_t pte) = NULL;

int pageSize = 0;
int pageTableFrame = 0;
int validMask = 0x1;
int offSetBits = 0;

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

    conf->pageTableRoot = allocateFrame();
    pageTableFrame = conf->pageTableRoot;

    return 0;
}

// TODO: implement LRU
uint32_t findEvict(void)
{
    return 10;
}

//
// Routine is responsible for handling page faults on
//   the specified address.
//
void pageFault(uint32_t address)
{
    printf("-----------------PAGE FAULT HANDLER---------------\n");
    printf("pageFault called with address 0x%x\n", address);
    int currFrame = pageTableFrame;
    if (currFrame == -1)
        exit(1);

    uint32_t levels = (32 - offSetBits) / (offSetBits - 2);
    uint32_t levelBits = (32 - offSetBits) / levels;
    uint32_t vpnkMask = ((1 << levelBits) - 1) << (32 - levelBits);
    for (int i = 1; i <= levels; i++)
    {
        uint32_t vpnk = (address & vpnkMask) >> (32 - (i * levelBits));
        printf("vpnkMask: 0x%x, vpnk: 0x%x\n", vpnkMask, vpnk);
        pte_t pte = getPTE(currFrame, vpnk);

        // upper bits all 1, pte is null
        printf("-pageSize = 0x%x, pte = 0x%x\n", -pageSize, pte);
        if ((pte & (-pageSize)) == (-pageSize)) {
            printf("null page table entry\n");
            pte_t newpte;

            // try allocating a page for it
            int newFrame = allocateFrame();
            if (newFrame != 0) { // frame allocation succeed!

                newpte = newFrame << offSetBits;
                newpte |= validMask; // valid bit

                if (writePTE(currFrame, vpnk, newpte) == -1) {
                    printf("writePTE failed :(\n");
                    exit(1);
                }
                return;
            }

            // need to swap out a frame and get a free page
            int newSwap = allocateSwap();
            if (newSwap == -1) {
                printf("allocate Swap failed :(\n");
                exit(1);
            }

            int evictingFrame = findEvict();
            copyToSwap(evictingFrame, newSwap);
            newpte = evictingFrame << offSetBits;
            newpte |= validMask; // valid bit
            if (writePTE(currFrame, vpnk, newpte) == -1) {
                printf("writePTE failed :(\n");
                exit(1);
            }
            return;
        }

        if (!(pte & validMask)) { // swap exists, just not in memory
            int swapID = pte & (-pageSize) >> offSetBits;
            int evictingFrame = findEvict();
            copyFromSwap(swapID, evictingFrame);
            writePTE(currFrame, vpnk, pte | validMask);
            return;
        }

        currFrame = (pte & (-pageSize)) >> offSetBits;
        vpnkMask >>= levelBits;
    }
    printf("nothing wrong?\n");
    return;
}
