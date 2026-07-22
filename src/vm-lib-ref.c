#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "vm-api.h"

uint32_t (*allocateFrame)(void) = NULL;
uint32_t (*allocateSwap)(void) = NULL;
void (*copyToSwap)(uint32_t, uint32_t) = NULL;
void (*copyFromSwap)(uint32_t, uint32_t) = NULL;
pte_t* (*getPTE)(uint32_t frame, uint32_t index) = NULL;
int (*writePTE)(uint32_t frame, uint32_t index, pte_t pte) = NULL;

int pageSize = 0;
int pageTableFrame = 0;
int validMask = 0x1;
int refMask = 0x2;
int softMask = 0x4;
int offSetBits = 0;
int swapCount = 0;
int numFrames = 0;
int pageTableLevels = 0;
char *tmpSwap = NULL;

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
    tmpSwap = conf->tmpSwap;

    conf->pageTableRoot = allocateFrame();
    pageTableFrame = conf->pageTableRoot;

    pageTableLevels = (32 - offSetBits) / (offSetBits - 2);

    return 0;
}

bool swapExists(int swapID)
{
    return swapID < swapCount;
}

// TODO: implement clock algorithm
// Should return address of pte to evict
pte_t *findEvict(uint32_t ptableFrame, uint32_t levels, char *foundSet3)
{
    if (levels == 1) {
        char foundBest = 0;
        foundSet3 = &foundBest;
        printf("findEvict called\n");
    }

    uint32_t ppnMask = ~(pageSize - 1);

    // final level
    if (levels == pageTableLevels) {
        printf("reached final level\n");
        printf("pte's at final level:\n");
        pte_t *retPTE = NULL;
        for (int i = 0; i < (pageSize / sizeof(pte_t)); i++) {
            pte_t *pte = getPTE(ptableFrame, i);
            printf("    pte: 0x%x\n", *pte);

            if (!(*pte & refMask)) {
                if (!(*pte & validMask) && (*pte & softMask)) {
                    // set 3 (~valid bit, ~ref bit, soft bit)
                    *foundSet3 = 1;
                    return pte;
                } else if (*pte & validMask) { // set 2 (valid bit + ~ref bit)
                    retPTE = pte;
                }
            } else if ((*pte & validMask) && retPTE == NULL) { // set 1 (valid bit + ref bit)
                retPTE = pte;
            }
        }

        return retPTE;
    }

    // recursively look downwards
    pte_t *evictingFramePTE = NULL;
    for (int i = 0; i < (pageSize / sizeof(pte_t)); i++) {
        pte_t pte = *getPTE(ptableFrame, i);
        for (int j = 0; j < levels - 1; j++) printf("    ");
        printf("currpte: 0x%x level %d\n", pte, levels);
        if (pte & validMask) { // only go down if valid mask
            printf("going down:\n");
            pte_t *tmp = findEvict((pte & ppnMask) >> offSetBits, levels+1, foundSet3);
            if (tmp != NULL)
                evictingFramePTE = tmp;
        }
        if (evictingFramePTE != NULL && (*foundSet3 == 1)) {
            if (levels == 1)
                printf("returning evictingFramePTE:0x%x set 3 return!\n", *evictingFramePTE);
            return evictingFramePTE;
        }
    }
    if (levels == 1) {
        if (evictingFramePTE == NULL) {
            printf("evictingFramePTE is NULL!!\n");
            // exit(1);
        }
        printf("returning evictingFramePTE:0x%x non-set 3 return\n", *evictingFramePTE);
    }
    return evictingFramePTE;
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
        pte_t pte = *getPTE(currFrame, vpnk);

        // as I'm going down, the invalid one is the only that matters
        if (!(pte & validMask)) {
            if (!(pte & refMask) && (pte & softMask)) { // soft fault phew!
                // just toggle valid mask and we move on
                writePTE(currFrame, vpnk, pte | validMask);
                return;
            } else if ((pte & refMask) && !(pte & softMask)) { // hard fault + swap
                uint32_t swapID = (pte & ~(pageSize - 1)) >> offSetBits;

                if (allocateFrame() != 0) {
                    printf("swapID before all frames fully allocated! "
                           "frame: %d, index: %d, pte: 0x%x\n", currFrame, vpnk, pte);
                    exit(1);
                }

                pte_t *evictingPTE = findEvict(pageTableFrame, 1, NULL);
                uint32_t evictingFrame = (*evictingPTE & ~(pageSize - 1)) >> offSetBits;
                // switch swap frame galaxy
                copyToSwap(evictingFrame, -1); // copy to tmp
                copyFromSwap(swapID, evictingFrame);
                copyToSwap(-1, swapID);

                *evictingPTE = (((swapID << offSetBits) & (~validMask)) | refMask) & (~softMask);
                writePTE(currFrame, vpnk, (pte | validMask) & (~refMask));
                return;
            } else if (!(pte & refMask) && !(pte & softMask)) { // hard fault + null
                uint32_t frame;
                if ((frame = allocateFrame()) != 0) {
                    pte_t newPTE = frame << offSetBits | validMask;
                    writePTE(currFrame, vpnk, newPTE);
                    return;
                }
                // no more space..
                pte_t *evictingPTE = findEvict(pageTableFrame, 1, NULL);
                uint32_t evictingFrame = (*evictingPTE & ~(pageSize - 1)) >> offSetBits;
                // switch swap frame galaxy
                uint32_t newSwap = allocateSwap();
                copyToSwap(evictingFrame, -1); // copy to tmp
                copyFromSwap(newSwap, evictingFrame);
                copyToSwap(-1, newSwap);

                *evictingPTE = (((newSwap << offSetBits) & (~validMask)) | refMask) & (~softMask);
                writePTE(currFrame, vpnk, (evictingFrame | validMask) & (~refMask));
                return;
            }
        }

        currFrame = (pte & ~(pageSize - 1)) >> offSetBits;
        vpnkMask >>= levelBits;
    }
    printf("nothing wrong?\n");
    return;
}
