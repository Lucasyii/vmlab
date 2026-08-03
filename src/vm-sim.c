/*
 * vm-sim.c - VM hardware simulator
 * Lucas Yi <hyi2@andrew.cmu.edu>
 * 15-213 Summer 2026
 */
#include <stdio.h>
#include <dlfcn.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "vm-api.h"

int (*initLibrary)(struct config* conf) = NULL;
void (*pageFault)(uint32_t address) = NULL;
void (*timer)(void) = NULL;

const char *USAGE = "%s -t <trace> -o <page fault library> -p <page offset bits> -V <verbose level> -n <frame count>\n";

// char * to make them byte indexable
char *physicalMemory = NULL;
char *swapSpace = NULL;
struct pageTableHeader *pt;
bool verbose = false;
uint32_t validBitMask = 0x1;
uint32_t refBitMask = 0x2;
int frameCount = -1;
uint32_t swapCount = 0;

int loadLibrary(char* fileName)
{
    void* handle = dlopen(fileName, RTLD_LAZY);
    if (handle == NULL)
    {
        fprintf(stderr, "Failed to load %s library: %s\n",
                fileName, dlerror());
        return -1;
    }

    initLibrary = dlsym(handle, "initLibrary");
    pageFault = dlsym(handle, "pageFault");
    timer = dlsym(handle, "timer");

    if (initLibrary == NULL ||
        pageFault == NULL)
    {
        dlclose(handle);
        fprintf(stderr, "Failed to load interface for %s library\n", fileName);
        return -1;
    }

    return 0;
}

uint32_t allocateFrame(void) // frame is RAM
{
    // Frame 0 is the root of the page table
    static uint32_t nextFrame = 0; // didn't know this is how that worked
    if (nextFrame < (uint32_t)frameCount ||
        frameCount == -1)
    {
        // initLibrary should call allocateFrame() to assign frame 0 as pageTableRoot
        printf("allocating new frame %u!\n", nextFrame);
        return nextFrame++;
    }

    return 0;
}

uint32_t allocateSwap(void) // swap is Disk (extension of RAM)
{
    if (swapCount == 0) {
        swapSpace = malloc(pt->pageSize);
        swapCount = 1;
    } else {
        swapSpace = realloc(swapSpace, ++swapCount * pt->pageSize);
    }

    printf("allocating new swap %u!\n", swapCount - 1);
    memset(swapSpace + ((swapCount - 1) * pt->pageSize), 0, pt->pageSize);

    return swapCount - 1;
}

/*
 * Directly copies the contents of a frame to a swap page
 *
 * @param[in] frame: frameID to swap from, if -1, use tmp swap
 * @param[in]  swap: swapID to swap to, if -1 use tmp swap
 */
void copyToSwap(uint32_t frame, uint32_t swap)
{
    printf("copyToSwap= frame: %d, swap: %d\n", (int)frame, (int)swap);

    if (frame == -1) {
        memcpy(&(swapSpace[pt->pageSize * swap]),
               pt->tmpSwap,
               pt->pageSize);
    } else if (swap == -1) {
        memcpy(pt->tmpSwap,
               &(physicalMemory[frame * pt->pageSize]),
               pt->pageSize);
    } else {
        // need to make sure memory not overlapping
        memcpy(&(swapSpace[pt->pageSize * swap]),
               &(physicalMemory[frame * pt->pageSize]),
               pt->pageSize);
    }
    return;
}

// memcpy (*dest, *src, n bytes)

/*
 * Directly copies the contents of a swap page to a frame
 *
 * @param[in]  swap: swapID to swap to, if -1 use tmp swap
 * @param[in] frame: frameID to swap from, if -1, use tmp swap
 */
void copyFromSwap(uint32_t swap, uint32_t frame)
{
    printf("copyFromSwap= swap: %d, frame: %d\n", (int)swap, (int)frame);
    if (swap == -1) {
        memcpy(&(physicalMemory[frame * pt->pageSize]),
               pt->tmpSwap,
               pt->pageSize);
    } else if (frame == -1) {
        memcpy(pt->tmpSwap,
               &(swapSpace[pt->pageSize * swap]),
               pt->pageSize);
    } else {
        // need to make sure memory not overlapping
        memcpy(&(physicalMemory[frame * pt->pageSize]),
               &(swapSpace[pt->pageSize * swap]),
               pt->pageSize);
    }
    return;
}

/*
 * @pre assumes frame given holds page table
 * @param[in] frame: Physical frame number to index into
 * @param[in] index: Index page table entry index
 * @param[out]  pte: page table entry according to indexing
 */
pte_t *getPTE(uint32_t frame, uint32_t index)
{
    uint32_t ptea = frame * pt->pageSize + (index * sizeof(pte_t));
    pte_t *pte = (pte_t *)(&(physicalMemory[ptea]));
    return pte;
}

/*
 * @pre assumes frame given holds page table
 * @param[in] frame: Physical frame number to index into
 * @param[in] index: Index page table entry index
 * @param[in]   pte: page table entry to write
 * @param[out] code: 0 if success, -1 if out of bounds
 */
int writePTE(uint32_t frame, uint32_t index, pte_t pte)
{
    printf("writePTE = frame: 0x%x, index: 0x%x, pte: 0x%x\n", frame, index, pte);
    uint32_t ptea = frame * pt->pageSize + (index * sizeof(pte_t));
    if (ptea > pt->pageSize * frameCount) {
        fprintf(stderr, "call to writePTE(%u, %u, %x) out of bounds",
                frame, index, pte);
        return -1;
    }
    *(pte_t *)(&(physicalMemory[ptea])) = pte;
    return 0;
}

void printMemory(struct config *c)
{
    for (int i = 0; i < c->numFrames; i++) {
        printf("\n-----------------page index %d--------------\n", i);
        for (int j = 0; j < c->pageSize; j += sizeof(pte_t)) {
            pte_t *pte = (pte_t *)(&(physicalMemory[i * c->pageSize + j]));
            if (*pte != 0) // only print non-null
                printf("index %lu: 0x%x\n", j / sizeof(pte_t),  *pte);
        }
    }
    return;
}

void printSwap(struct config *c)
{
    for (int i = 0; i < swapCount; i++) {
        printf("\n-----------------swap index %d--------------\n", i);
        for (int j = 0; j < c->pageSize; j += sizeof(pte_t)) {
            pte_t *pte = (pte_t *)(&(swapSpace[i * c->pageSize + j]));
            if (*pte != 0)
                printf("index %lu: 0x%x\n", j / sizeof(pte_t), *pte);
        }
    }

}

/*
 *
 * @pre assumes correct initialization of library
 * @pre c->pageSize: power of 2
 * @param[in]  addr: Virtual address we tryna get
 * @param[in]     c: config of our RAM
 * @param[out]  ret: Physical address we getting OR -1 to signal try again
 */
int translate(uint32_t virtualAddr, struct config *c)
{
    printf("\n----------------HARDWARE------------------\n");

    uint32_t vpnBits = 32 - pt->pageOffsetBits;
    uint32_t offsetMask = pt->pageSize - 1; // offsetMask = 0000...001111

    printf("translate called for addr: 0x%x\n",virtualAddr);
    // 2. Try to look into PTEA
    if (c->pageTableRoot == -1) {
        fprintf(stderr, "pageTableRoot should NEVER be null during a trace\n");
        exit(1);
    }

    if (vpnBits % pt->levels != 0) {
        printf("vpnBits = %u, pt->levels = %u illegal settings\n",
                vpnBits, pt->levels);
        exit(1);
    }

    // Physical address of page table
    int ptAddr = c->pageTableRoot;

    // vpnkMask is a sliding window of bits that moves down each level
    uint32_t levelBits = vpnBits / pt->levels;
    uint32_t vpnkMask = ((1 << levelBits) - 1) << (32 - levelBits);
    for (int i = 1; i <= pt->levels; i++)
    {
        uint32_t vpnk = (virtualAddr & vpnkMask) >> (32 - (i * levelBits));
        uint32_t ptea = ptAddr + (vpnk * sizeof(pte_t));
        pte_t *pte = (pte_t *)(&(physicalMemory[ptea]));
        if (verbose)
            printf("pte level #%d at ptea 0x%x: 0x%x\n", i, ptea, *pte);

        if (!(*pte & validBitMask)) { // pageFault determined by validBit
            // calls pageFault
            return -1;
        }

        // referenced this page table entry!
        *pte |= refBitMask;

        ptAddr = *pte & ~offsetMask;

        if (verbose)
            printf("ptAddr becomes: 0x%x\n", ptAddr);
        vpnkMask >>= levelBits;
    }

    // ptAddr == physical page number
    int physicalAddr = ptAddr | (virtualAddr & offsetMask);

    // TODO: physicalAddr post processing?
    if (verbose)
        printf("physicalAddr: 0x%x for virtualAddr: 0x%x\n", physicalAddr, virtualAddr);

    return physicalAddr;
}

// frame count * page size == total RAM size
// vm-sim -t <trace> -o <page fault library> -p <page offset bits> -V <verbose level> -n <frame count>
int main(int argc, char** argv)
{
    char traceName[100];
    char libName[100];

    // int pageSize = 256;
    int pageOffBits = 0;

    bool read_t = false, read_o = false, read_n = false;

    // Get opt of arguments
    int opt, nread;
    while ((opt = getopt(argc, argv, "hVp:o:t:n:")) != -1) {
        switch (opt) {
        case 'h':
            fprintf(stderr, USAGE, argv[0]);
            return 1;
        case 'V':
            verbose = true;
            break;
        case 't':
            nread = sscanf(optarg, "%s", traceName);
            read_t = nread == 1;
            break;
        case 'o':
            nread = sscanf(optarg, "%s", libName);
            read_o = nread == 1;
            break;
        case 'p':
            nread = sscanf(optarg, "%d", &pageOffBits);
            // read_p = nread == 1;
            break;
        case 'n':
            nread = sscanf(optarg, "%d", &frameCount);
            read_n = nread == 1;
            break;
        default:
            fprintf(stderr, "Error while parsing arguments.\n");
            fprintf(stderr, USAGE, argv[0]);
            return 1;
        }
    }

    // Check for and reject any extra arguments
    if (optind < argc) {
        fprintf(stderr, "Extra arguments passed.\n");
        fprintf(stderr, USAGE, argv[0]);
        return 1;
    }

    // Check if mandatory arguments were given
    if (!read_t || !read_o || !read_n || traceName == NULL) {
        fprintf(stderr, "Mandatory arguments missing or zero.\n");
        fprintf(stderr, USAGE, argv[0]);
        return 1;
    }

    // Check if page offset bits valid
    if (pageOffBits < 1) {
        fprintf(stderr, "Page offset bits less than 1.\n");
        fprintf(stderr, USAGE, argv[0]);
        return 1;
    }

    // open page fault library
    if (loadLibrary(libName) != 0) {
        fprintf(stderr, "Failed to load student library file\n");
        return 1;
    }

    // initialize meta variables
    struct config c;
    c.pageSize = 1 << pageOffBits;
    c.numFrames = frameCount;
    c.pageTableRoot = -1;
    c.allocateFrame = allocateFrame;
    c.allocateSwap = allocateSwap;
    c.copyFromSwap = copyFromSwap;
    c.copyToSwap = copyToSwap;
    c.getPTE = getPTE;
    c.writePTE = writePTE;
    c.offsetBits = pageOffBits; // wanna get rid of this
    c.tmpSwap = malloc(c.pageSize);
    initLibrary(&c);

    pt = malloc(sizeof(struct pageTableHeader));
    pt->pageOffsetBits = pageOffBits;
    pt->pageSize = c.pageSize;
    pt->levels = (32 - pageOffBits) / (pageOffBits - 2);
    pt->tmpSwap = c.tmpSwap;

    // physical memory initialization
    physicalMemory = calloc(c.pageSize, c.numFrames);
    c.pageTableRoot = 0;
    printf("physical Memory has %d bytes\n", c.pageSize * c.numFrames);
    printf("before trace\n");

    // open trace
    FILE* trace = fopen(traceName, "r");
    int addr = -1; // trace just going to be lines of uint32_t addr in hex
    int pageFaultCount = 0;
    int timerCount = 0;
    uint32_t lastPageFault = -1;
    uint32_t timerLimit = 10;
    while (fscanf(trace, "%x\n", &addr) > 0)
    {
        uint32_t translated = 0;
        while ((translated = translate(addr, &c)) == -1) {
            if (lastPageFault != addr) { // checking if we're stuck in a loop
                lastPageFault = addr;
                pageFaultCount = 1;
            } else {
                if (++pageFaultCount > (pt->levels) + 2) {
                    printf("stuck in a loop.. exiting\n");
                    return 1;
                }
            }
            pageFault(addr);
            printMemory(&c);
        }

        printSwap(&c);
        printf("\n----------------FOUND TRANSLATION!-----------------\n");
        printf("translated addr: 0x%x\n", translated);
        if (timerCount++ == timerLimit) {
            timer();
            timerCount = 0;
            printMemory(&c);
        }
    }

    if (swapSpace != NULL) free(swapSpace);
    free(c.tmpSwap);
    free(physicalMemory);
    free(pt);

    return 0;
}
