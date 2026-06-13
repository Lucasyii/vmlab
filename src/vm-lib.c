#include <stdlib.h>
#include <stdint.h>
#include "vm-api.h"

uint32_t (*allocateFrame)(void) = NULL;
uint32_t (*allocateSwap)(void) = NULL;
void (*copyToSwap)(uint32_t, uint32_t) = NULL;
void (*copyFromSwap)(uint32_t, uint32_t) = NULL;

int initLibrary(struct config* conf)
{
    if (conf == NULL) return -1;
    
    allocateFrame = conf->allocateFrame;
    allocateSwap = conf->allocateSwap;
    copyToSwap = conf->copyToSwap;
    copyFromSwap = conf->copyFromSwap;
    
    return 0;
}

void pageFault(uint32_t address)
{
    
}