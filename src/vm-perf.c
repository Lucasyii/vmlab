/*
 * vm-perf.c - Performance probe for vm-simulator
 * Lucas Yi <hyi2@andrew.cmu.edu>
 * 15-213 Summer 2026
 */
#include <stdio.h>
#include <fcntl.h> // open
#include "vm-perf.h"

struct checker *c = NULL;
FILE* logFile = NULL;

/*
 * Initializes performance checker with output FILE*
 *
 * If verbosity > 1, output FILE* = stdout
 * otherwise, output FILE* = fopen("log.txt", w+);
 *
 * exits upon error and returns upon success
 */
void initPerf(int verbosity)
{
    c = malloc(sizeof(struct checker));
    if (malloc == NULL) {
        fprintf(stderr, "malloc returned error\n");
        exit(1);
    }

    c->translatedNum = 0;
    c->pagefaultNum = 0;
    c->swapNum = 0;

    if (verbosity > 1) {
        logFile = stdout;
    } else {
        logFile = fopen("log.txt", w+);
        if (logFile == NULL) {
            perror("File opening failed");
            exit(1);
        }
    }
    return;
}

/*
 * Increments successful translation count & logs translation
 *
 * Should be called after every successful translation
 */
void translated(uint32_t vAddr, uint32_t pAddr)
{
    if (c == NULL) {
        fprintf(stderr, "translated called with no checker... exiting\n");
        exit(1);
    }
    c->translatedNum++;
    fprintf(logFile, "Successful translation! vAddr 0x%x -> pAddr 0x%x\n",
            vAddr, pAddr);
    return;
}


void pagefaulted(uint32_t virtualAddress)
{
    if (c == NULL) {
        fprintf(stderr, "pagefaulted called with no checker... exiting\n");
        exit(1);
    }
    c->pagefaultNum++;

    if (logFile != NULL)
        fprintf(logFile, "Page fault on vAddr 0x%x\n", vAddr);

    return;
}

void swapped(uint32_t swapID)
{
    if (c == NULL) {
        fprintf(stderr, "swapped called with no checker... exiting\n");
        exit(1);
    }
    c->pagefaultNum++;

    if (logFile != NULL)
        fprintf(logFile, "Accessing disk swapID %u\n", swapID);

    return;
}

void printPerf(void)
{
    if (c == NULL) {
        fprintf(stderr, "Error: Performance checker is not initalized.\n");
        return;
    }

    printf("=== Performance Score ===\n"
           "Translations : %zu\n"
           "Page Faults  : %zu\n"
           "Swaps        : %zu\n"
           "=========================\n",
           c->translateNum, c->pagefaultNum, c->swapNum);

    return;
}

void freePerf(void)
{
    if (c == NULL) {
        fprintf(stderr, "Error: attempt to free null checker.\n");
        exit(1);
    }

    free(c);
    c = NULL;

    if (logFile != NULL && logFile != stdout)
        fclose(logFile);

    logFile = NULL;

    return;
}
