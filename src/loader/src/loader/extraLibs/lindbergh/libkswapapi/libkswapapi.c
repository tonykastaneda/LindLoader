#include "libkswapapi.h"

/**
 * Hook for the only function provided by kswapapi.so
 * @param p No idea this gets discarded
 */
int kswap_collect(void *p)
{
    return 0;
}
