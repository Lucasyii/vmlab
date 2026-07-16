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

const char *USAGE = "TODO %s\n";

uint32_t *physicalMemory = NULL;
uint32_t *swapSpace = NULL;
int pageOffsetBits = 0;

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
uint32_t allocateFrame(void) // frame is RAM
{
    // Frame 0 is the root of the page table
    static uint32_t nextFrame = 0;
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
 * @param[in] addr: Virtual address we tryna get
 * @param[in] c:    config of our RAM
 */
int evaluate(uint32_t virtual_addr, struct config *c)
{
    // 1. Get vpn from virtual_addr

    // if pageOffsetBits = 4 ==> 000011...11111
    uint32_t vpn_mask = ~(~0 << (32 - pageOffsetBits));
    uint32_t vpn = vpn_mask & (virtual_addr >> pageOffsetBits);

    // 2. Try to look into PTEA
}

// frame count * page size == total RAM size
// vm-sim -t <trace> -o <page fault library> -p <page size> -V <verbose level> -n <frame count>
int main(int argc, char** argv)
{
    char* traceName = NULL;
    char* libName = NULL;

    int pageSize = 256;
    pageOffsetBits = 0;

    bool read_t = false, read_o = false, read_n = false;

    // Get opt of arguments
    // For now, page size is kept as optional
    int opt, nread;
    while ((opt = getopt(argc, argv, "hVpt:o:n:")) != -1) {
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
            nread = sscanf(optarg, "%d", &pageOffsetBits);
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
    if (loadLibrary(libName) != 0)
    {

    }

    struct config c;
    c.pageSize = 1 << pageOffsetBits;
    c.numFrames = frameCount;
    c.pageTableRoot = 0;
    c.allocateFrame = allocateFrame;
    c.allocateSwap = allocateSwap;
    c.copyFromSwap = copyFromSwap;
    c.copyToSwap = copyToSwap;
    initLibrary(&c);

    // open trace
    FILE* trace = fopen(traceName, "r");
    int addr = -1; // trace just going to be lines of uint32_t addr
    while (fscanf(trace, "%x\n", &addr) > 0)
    {
        while (evaluate(addr, &c) != -1) {

        }
    }

    return 0;
}
