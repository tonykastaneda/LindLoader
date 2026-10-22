#include "netLink.h"

#include "../config/config.h"
#include "../log/log.h"

static uint32_t gLocalAddress;

uint32_t netLinkLocalAddress(void)
{
    return gLocalAddress;
}

void netLinkSetLocalAddress(uint32_t addr)
{
    const unsigned char *b = (const unsigned char *)&addr;
    gLocalAddress = addr;
    log_info("Link play: this cabinet is %u.%u.%u.%u\n", b[0], b[1], b[2], b[3]);
}

int netLinkActive(void)
{
    return getConfig()->loopbackLink && gLocalAddress != 0;
}
