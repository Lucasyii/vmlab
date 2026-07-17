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

typedef uint32_t pte_t;

// char * to make them byte indexable
char *physicalMemory = NULL;
char *swapSpace = NULL;
struct pageTableHeader *pt;

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

int frameCount = -1;
uint32_t allocateFrame() // frame is RAM
{
    // Frame 0 is the root of the page table
    static uint32_t nextFrame = 0; // didn't know this is how that worked
    if (nextFrame < (uint32_t)frameCount ||
        frameCount == -1)
    {
        return ++nextFrame;
    }

    return 0;
}

uint32_t allocateSwap(void) // swap is Disk (extension of RAM)
{
    static uint32_t nextSwap = 0;

    // TODO - swap needs actual memory so that contents can be copied

    return ++nextSwap;
}

void copyToSwap(uint32_t frame, uint32_t swap)
{

}

void copyFromSwap(uint32_t swap, uint32_t frame)
{

}

/*
 *
 * @pre assumes correct initialization of library
 * @pre c->pageSize: power of 2
 * @param[in]  addr: Virtual address we tryna get
 * @param[in]     c: config of our RAM
 * @param[out]  ret: Physical address we getting OR -1 on error so page fault
 */
int translate(uint32_t virtualAddr, struct config *c)
{
    // 1. Get vpn from virtual_addr

    // if pageOffsetBits = 4 ==> 000011...11111
    uint32_t vpnMask = ~0 << (32 - pt->pageOffsetBits);
    uint32_t vpn = (vpnMask & virtualAddr) >> pt->pageOffsetBits;
    printf("vpn is %u\n", vpn);
    // unsigned shift is LOGICAL!!!!
    // offsetMask = 0000...001111
    uint32_t offsetMask = (1 << pt->pageOffsetBits) - 1;

    // 2. Try to look into PTEA
    if (c->pageTableRoot == -1) {
        // TODO: handle null case
    }

    int ptPID = c->pageTableRoot;
    int ptAddr = ptPID * c->pageSize;
    int physicalAddr = -1;
    uint32_t ppn;
    if (pt->levels == 1) {
        // I'm so fucking scared
        uint32_t ptea = ptAddr + (vpn * sizeof(pte_t));
        pte_t pte = (pte_t)(physicalMemory[ptea]);

        // TODO: validity check of pte
        // if (pte invalid) {
        //     return -1;
        // }

        ppn = pte & (~0 << (32 - pt->pageOffsetBits));
    } else {
        printf("trying multi-level rn:\n");
        printf("Levels of page table based on size %d: %d\n", c->pageSize, pt->levels);

        uint32_t vpnBits = 32 - pt->pageOffsetBits;
        if (vpnBits % pt->levels != 0)
        {
            printf("vpnBits = %u, pt->levels = %u illegal settings\n", vpnBits, pt->levels);
            return -1;
        }

        uint32_t bitnum = vpnBits / pt->levels;
        uint32_t vpnkMask = ((1 << bitnum) - 1) << (32 - bitnum);
        for (int i = 0; i < pt->levels; i++)
        {
            uint32_t vpnk = (virtualAddr & vpnkMask) >> (32 - ((i + 1) * bitnum));
            uint32_t ptea = ptAddr + (vpnk * sizeof(pte_t));
            pte_t pte = *(pte_t *)(&(physicalMemory[ptea]));
            printf("pte #%d at ptea 0x%x: 0x%x\n", i, ptea, pte);

            // TODO: validity check of pte

            ptAddr = pte & (~0 << pt->pageOffsetBits);
            printf("ptAddr becomes: 0x%x\n", ptAddr);
            vpnkMask >>= bitnum;
        }
        ppn = ptAddr;
    }

    physicalAddr = ppn | (virtualAddr & offsetMask);

    // TODO: physicalAddr post processing
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

    // TODO: read_o should be actual page fault library (fix the colon on getopt as well)
    bool read_t = false, read_o = true, read_n = false;

    // Get opt of arguments
    // For now, page size is kept as optional
    int opt, nread;
    while ((opt = getopt(argc, argv, "hVp:ot:n:")) != -1) {
        switch (opt) {
        case 'h':
            // print help function
            break;
        case 'V':
            // if (sscanf(optarg, "%u", &verbose_level) != 1)
                // fprintf(stderr, "verbose level not set\n");
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


    // open page fault library
    // if (loadLibrary(libName) != 0)
    // {
    //
    // }

    // initialize meta variables
    struct config c;
    c.pageSize = 1 << pageOffBits;
    c.numFrames = frameCount;
    c.pageTableRoot = -1;
    c.allocateFrame = allocateFrame;
    c.allocateSwap = allocateSwap;
    c.copyFromSwap = copyFromSwap;
    c.copyToSwap = copyToSwap;
    // initLibrary(&c); TODO: change this back

    pt = malloc(sizeof(struct pageTableHeader));
    pt->pageOffsetBits = pageOffBits;
    pt->levels = (32 - pageOffBits) / (pageOffBits - 2);

    // physical memory initialization
    physicalMemory = malloc(c.pageSize * c.numFrames);
    printf("physical Memory has %d bytes\n", c.pageSize * c.numFrames);
    c.pageTableRoot = 0;
    *(pte_t *)(&(physicalMemory[4])) = 0x100;
    *(pte_t *)(&(physicalMemory[c.pageSize + 8])) = 0x200;
    *(pte_t *)(&(physicalMemory[c.pageSize * 2 + 12])) = 0x300;
    *(pte_t *)(&(physicalMemory[c.pageSize * 3 + 16])) = 0x400;

    printf("before trace\n");

    // open trace
    FILE* trace = fopen(traceName, "r");
    int addr = -1; // trace just going to be lines of uint32_t addr
    while (fscanf(trace, "%x\n", &addr) > 0)
    {
        uint32_t translated = 0;
        if ((translated = translate(addr, &c)) == -1) {
            pageFault(addr);
        }
    }

    free(pt);

    return 0;
}
