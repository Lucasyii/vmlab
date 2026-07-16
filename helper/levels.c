#include <stdio.h>
#include <stdlib.h>

void calculate(int psize_bits)
{
    int vpn_bits = 32 - psize_bits;
    int page_level_bits = psize_bits - 2;

    if (page_level_bits <= 0)  {
        printf("page_level bits too low: %d\n", page_level_bits);
        return;
    }

    if (vpn_bits % page_level_bits == 0)
    {
        printf("Page size: %d\n", 1 << psize_bits);
        printf("VPN bits: %d, Bits for 1 level: %d, # of levels: %d\n",
                vpn_bits, page_level_bits, vpn_bits/page_level_bits);
    }
    return;
}

int main(int argc, char **argv)
{
    size_t page_size = 0;

    printf("insert page offset bits (log2 of page size):");
    scanf("%lu", &page_size);

    if (page_size <= 32 && page_size > 0) {
        calculate(page_size);
    } else if (page_size == 0) {
        for (int i = 1; i <= 32; i++) {
            calculate(i);
        }
    } else {
        printf("invalid page_size");
        exit(1);
    }

    return 0;
}
