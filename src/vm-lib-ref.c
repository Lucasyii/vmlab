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
uint32_t *protected = NULL;

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

/*
 * @brief recursively demotes ptes down this order:
 *        1. valid bit & ref bit
 *        2. valid bit & ~ref bit
 *        3. ~valid bit & ~ref bit & soft bit
 *   Note that the path down to a normal page bit should be in descending order
 * of the sets
 * @param[in] pageTableRoot: Starting physical address of root of page table
 * @param[in]         level: Indicator for recursive termination condition
 */
void demoteBits(uint32_t ptableFrame, uint32_t level) {
    if (level == 0)
        printf("calling demoteBits\n");

    for (int i = 0; i < (pageSize / sizeof(pte_t)); i++)
    {
        pte_t *pte = getPTE(ptableFrame, i);
        uint32_t pteBits = *pte & 0x7;

        if (pteBits == (refMask | validMask)) {       // set 1
            *pte &= ~refMask; // clear out ref bit

            if (level != pageTableLevels)
                demoteBits(*pte >> offSetBits, level + 1);

        } else if (pteBits == validMask) {               // set 2
            *pte &= ~validMask; // clear out valid bit
            *pte |= softMask; // set soft page fault bit

            if (level != pageTableLevels)
                demoteBits(*pte >> offSetBits, level + 1);
        } // we don't demote set 3
    }
    return;
}

/*
 * Function called every 1000 address translations
 */
void timer(void)
{
    demoteBits(pageTableFrame, 0);
    return;
}

void printProtected(void)
{
    printf("Protected frames: %d", protected[0]);
    for (int i = 1; i < pageTableLevels; i++) {
        printf(", %d", protected[i]);
    }
    printf("\n");
    return;
}

bool isProtected(uint32_t frame)
{
    for (int i = 0; i < pageTableLevels; i++)
        if (protected[i] == frame)
            return true;
    return false;
}

pte_t *findEvictHelper(uint32_t ptableFrame, uint32_t levels, uint32_t bits)
{
    pte_t *evictingFramePTE = NULL;
    pte_t *currLevelPTE = NULL;

    // loop through each page table entry
    for (int i = 0; i < (pageSize / sizeof(pte_t)); i++) {
        pte_t *pte = getPTE(ptableFrame, i);
        uint32_t pteBits = *pte & 0x7;

        if ((pteBits & validMask) || (pteBits & softMask)) {
            if (levels != pageTableLevels) {
                evictingFramePTE = findEvictHelper(*pte >> offSetBits, levels+1, bits);

                if (evictingFramePTE != NULL)
                    return evictingFramePTE;          // once lower level found, we want that

                if (pteBits == bits && !isProtected(*pte >> offSetBits)) { // this level valid pte
                    printf("frame %d is NOT protected\n", *pte >> offSetBits);
                    currLevelPTE = pte;
                }
            } else {
                if (pteBits == bits && !isProtected(*pte >> offSetBits)) // lowest level valid pte
                    return pte;
            }
        }
    }

    // no found underneath
    return currLevelPTE;
}

// Should return address of pte to evict
// Check one at a time.. There HAS to be a way to make it better
// TODO: make this algorithm more efficient
pte_t *findEvict(void)
{
    pte_t *evictingFramePTE = findEvictHelper(pageTableFrame, 1, softMask); // set 3
    if (evictingFramePTE != NULL)
        return evictingFramePTE;

    evictingFramePTE = findEvictHelper(pageTableFrame, 1, validMask); // set 2
    if (evictingFramePTE != NULL)
        return evictingFramePTE;

    printf("we have to evict a valid one gng :sob:\n");
    printProtected();
    return findEvictHelper(pageTableFrame, 1, validMask | refMask); // set 1
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

    protected = malloc(sizeof(uint32_t) * levels);
    for (int i = 0; i < levels; i++) protected[i] = pageTableFrame;

    for (int i = 1; i <= levels; i++)
    {
        uint32_t vpnk = (address & vpnkMask) >> (32 - (i * levelBits));
        printf("vpnkMask: 0x%x, vpnk: 0x%x\n", vpnkMask, vpnk);
        pte_t pte = *getPTE(currFrame, vpnk);
        protected[i-1] = currFrame;

        // as I'm going down, the invalid one is the only that matters
        if (!(pte & validMask)) {
            if (!(pte & refMask) && (pte & softMask)) { // soft fault phew!
                // just toggle valid mask and we move on
                writePTE(currFrame, vpnk, pte | validMask);
                free(protected);
                return;
            } else if ((pte & refMask) && !(pte & softMask)) { // hard fault + swap
                uint32_t swapID = pte >> offSetBits;

                if (allocateFrame() != 0) { // should never occur
                    printf("swapID before all frames fully allocated! "
                           "frame: %d, index: %d, pte: 0x%x\n", currFrame, vpnk, pte);
                    exit(1);
                }

                pte_t *evictingPTE = findEvict();
                uint32_t evictingFrame = *evictingPTE >> offSetBits;

                // swaps the evictingFrame & swapID page!
                copyToSwap(evictingFrame, -1); // copy to tmp
                copyFromSwap(swapID, evictingFrame);
                copyToSwap(-1, swapID);

                // ref bit used to indicate that ppn holding swap ID & differentiate from null
                *evictingPTE = (((swapID << offSetBits) & (~validMask)) | refMask) & (~softMask);
                writePTE(currFrame, vpnk, ((evictingFrame << offSetBits) | validMask) & (~refMask));
                free(protected);
                return;
            } else if (!(pte & refMask) && !(pte & softMask)) { // hard fault + null
                uint32_t frame;
                if ((frame = allocateFrame()) != 0) {
                    pte_t newPTE = frame << offSetBits | validMask;
                    writePTE(currFrame, vpnk, newPTE);
                    free(protected);
                    return;
                }

                // no more space..
                pte_t *evictingPTE = findEvict();
                uint32_t evictingFrame = *evictingPTE >> offSetBits;

                // shove the evictingFrame into newSwap page!
                uint32_t newSwap = allocateSwap();
                copyToSwap(evictingFrame, -1); // copy to tmp
                copyFromSwap(newSwap, evictingFrame);
                copyToSwap(-1, newSwap);

                // ref bit used to indicate that ppn holding swap ID & differentiate from null
                *evictingPTE = (((newSwap << offSetBits) & (~validMask)) | refMask) & (~softMask);
                writePTE(currFrame, vpnk, ((evictingFrame << offSetBits) | validMask) & (~refMask));
                free(protected);
                return;
            }
        }

        currFrame = pte >> offSetBits;
        vpnkMask >>= levelBits;
    }
    printf("nothing wrong?\n");
    free(protected);
    return;
}
