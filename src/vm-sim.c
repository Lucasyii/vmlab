/*
 * vm-sim.c: Simulates page management of process traces
 *
 *
 *
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

const char *USAGE = "%s -t <trace> -o <page fault library> -p <page offset bits> -V <verbose level> -n <frame count>\n";

// char * to make them byte indexable
char *physicalMemory = NULL;
char *swapSpace = NULL;
struct pageTableHeader *pt;
bool verbose = false;
uint32_t validBitMask = 0x1;
uint32_t lastPageFault = -1;
int frameCount = -1;

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
        printf("allocating new frame!\n");
        return nextFrame++;
    }

    return 0;
}

uint32_t allocateSwap(void) // swap is Disk (extension of RAM)
{
    static uint32_t nextSwap = 0;

    if (nextSwap == 0)
        swapSpace = malloc(++nextSwap * pt->pageSize);
    else
        swapSpace = realloc(swapSpace, ++nextSwap * pt->pageSize);

    return nextSwap;
}

void copyToSwap(uint32_t frame, uint32_t swap)
{
    // need to make sure memory not overlapping
    memcpy(&(physicalMemory[frame * pt->pageSize]),
           &(swapSpace[frame * swap]),
           pt->pageSize);
    return;
}

void copyFromSwap(uint32_t swap, uint32_t frame)
{
    // need to make sure memory not overlapping
    memcpy(&(swapSpace[frame * swap]),
           &(physicalMemory[frame * pt->pageSize]),
           pt->pageSize);
    return;
}

/*
 * @pre assumes frame given holds page table
 * @param[in] frame: Physical frame number to index into
 * @param[in] index: Index page table entry index
 * @param[out]  pte: page table entry according to indexing
 */
pte_t getPTE(uint32_t frame, uint32_t index)
{
    uint32_t ptea = frame * pt->pageSize + (index * sizeof(pte_t));
    pte_t pte = *(pte_t *)(&(physicalMemory[ptea]));
    printf("frame: 0x%x, index: 0x%x, pageSize: %u, ptea: 0x%x, pte: 0x%x\n",
            frame, index, pt->pageSize, ptea, pte);
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
    uint32_t ptea = frame * pt->pageSize + (index * sizeof(pte_t));
    if (ptea > pt->pageSize * frameCount) {
        fprintf(stderr, "call to writePTE(%u, %u, %x) out of bounds",
                frame, index, pte);
        return -1;
    }
    *(pte_t *)(&(physicalMemory[ptea])) = pte;
    return 0;
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
    // 1. Get vpn from virtual_addr

    // if pageOffsetBits = 4 ==> 000011...11111
    uint32_t vpnBits = 32 - pt->pageOffsetBits;
    uint32_t offsetMask = (1 << pt->pageOffsetBits) - 1; // offsetMask = 0000...001111

    printf("translate called\n");
    // 2. Try to look into PTEA
    if (c->pageTableRoot == -1) {
        fprintf(stderr, "pageTableRoot should NEVER be null during a trace\n");
        return -1;
    }

    if (vpnBits % pt->levels != 0) {
        printf("vpnBits = %u, pt->levels = %u illegal settings\n",
                vpnBits, pt->levels);
        return -1;
    }

    // Physical address of page table
    int ptAddr = c->pageTableRoot * c->pageSize;
    int physicalAddr = -1;

    // vpnMask is a sliding window of bits that moves for every round
    uint32_t levelBits = vpnBits / pt->levels;
    uint32_t vpnkMask = ((1 << levelBits) - 1) << (32 - levelBits);
    for (int i = 1; i <= pt->levels; i++)
    {
        uint32_t vpnk = (virtualAddr & vpnkMask) >> (32 - (i * levelBits));
        uint32_t ptea = ptAddr + (vpnk * sizeof(pte_t));
        pte_t pte = *(pte_t *)(&(physicalMemory[ptea]));
        if (verbose)
            printf("pte level #%d at ptea 0x%x: 0x%x\n", i, ptea, pte);

        if (!(pte & validBitMask)) {
            // calls pageFault
            return -1;
        }

        ptAddr = pte & (~0 << pt->pageOffsetBits);

        // (~0 << pt->pageOffsetBits) == -(c->pageSize)
        if (ptAddr == -(c->pageSize)) {
            // calls pageFault
            return -1;
        }

        if (verbose)
            printf("ptAddr becomes: 0x%x\n", ptAddr);
        vpnkMask >>= levelBits;
    }

    // ptAddr == physical page number
    physicalAddr = ptAddr | (virtualAddr & offsetMask);

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
    c.offsetBits = pageOffBits;
    initLibrary(&c);

    pt = malloc(sizeof(struct pageTableHeader));
    pt->pageOffsetBits = pageOffBits;
    pt->pageSize = c.pageSize;
    pt->levels = (32 - pageOffBits) / (pageOffBits - 2);

    // physical memory initialization
    physicalMemory = calloc(c.pageSize, c.numFrames);
    c.pageTableRoot = 0;
    printf("physical Memory has %d bytes\n", c.pageSize * c.numFrames);
    for (int i = 0; i < c.numFrames; i += sizeof(pte_t)) {
        // null with 0's on meta data bits
        *(pte_t *)(&(physicalMemory[i])) = -c.pageSize;
    }
    printf("before trace\n");

    // open trace
    FILE* trace = fopen(traceName, "r");
    int addr = -1; // trace just going to be lines of uint32_t addr in hex
    int pageFaultCount = 0;
    while (fscanf(trace, "%x\n", &addr) > 0)
    {
        uint32_t translated = 0;
        while ((translated = translate(addr, &c)) == -1) {
            printf("----------------HARDWARE------------------\n");
            if (lastPageFault != addr) {
                lastPageFault = addr;
                pageFaultCount = 1;
            } else {
                if (++pageFaultCount > (pt->levels) + 2) {
                    printf("stuck in a loop.. exiting\n");
                    return 1;
                }
            }
            pageFault(addr);
        }
        printf("translated addr: 0x%x\n", translated);
    }

    if (swapSpace != NULL) free(swapSpace);
    free(physicalMemory);
    free(pt);

    return 0;
}
