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

int main(int argc, char** argv)
{
    
    
    return 0;
}