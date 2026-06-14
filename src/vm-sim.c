#include <stdio.h>
#include <dlfcn.h>
#include "vm-api.h"

int (*initLibrary)(struct config* conf) = NULL;
void (*pageFault)(uint32_t address) = NULL;

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
uint32_t allocateFrame(void)
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

uint32_t allocateSwap(void)
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

// vm-sim -t <trace> -o <page fault library> -p <page size> -V <verbose level> -n <frame count>
int main(int argc, char** argv)
{
    char* traceName = NULL;
    char* libName = NULL;
    
    int pageSize = 256;
    
    // Get opt of arguments
    
    // open page fault library
    if (loadLibrary(libName) != 0)
    {
        
    }
    struct config c;
    c.pageSize = pageSize;
    c.numFrames = frameCount;
    c.allocateFrame = allocateFrame;
    c.allocateSwap = allocateSwap;
    c.copyFromSwap = copyFromSwap;
    c.copyToSwap = copyToSwap;
    initLibrary(&c);
    
    // open trace
    FILE* trace = fopen(traceName, "r");
    int addr = -1;
    while (fscanf(trace, "%x\n", &addr) > 0)
    {
        
    }
    
    return 0;
}