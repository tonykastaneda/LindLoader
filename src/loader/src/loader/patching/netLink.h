#pragma once

#include <stdint.h>

/* Loopback link play (LOOPBACK_LINK = true): several cabinets of a linked game run on one PC, each
   loader instance being one cabinet. Their LAN addresses become loopback ones (127.0.49.N for Sega
   Race TV), the address a cabinet gives its eth0 is its own, and the Windows network layer turns
   the LAN broadcasts the games use into one datagram per cabinet. */

#ifdef __cplusplus
extern "C" {
#endif

#define NET_LINK_CABINETS 4
#define NET_LINK_SHADOW_PORT_OFFSET 1000 /* broadcast receivers listen on port + this */

/* This cabinet's address (network byte order), as the game assigned it to eth0; 0 until then. */
uint32_t netLinkLocalAddress(void);
void netLinkSetLocalAddress(uint32_t addr);

/* True once loopback link play is on and this cabinet knows its address. */
int netLinkActive(void);

#ifdef __cplusplus
}
#endif
