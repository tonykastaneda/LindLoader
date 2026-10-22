#ifdef _WIN32
/* Linux network ioctls for games running on Windows.

   Lindbergh network code (e.g. Sega Race TV's klib::lan) lists the network
   interfaces with SIOCGIFCONF, reads their addresses with SIOCGIF*, and
   makes its sockets non-blocking with FIONBIO. Winsock has none of the
   interface ioctls, so this reports two interfaces the way Linux would:
   "lo" (127.0.0.1) and "eth0" (this PC's IPv4 address). Structures use the
   i386 Linux layout: struct ifreq is 32 bytes (16-byte name, then a 16-byte
   union whose first member is a struct sockaddr). */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "netIoctlWin.h"
#include "../patching/netLink.h"

#define L_FIONREAD 0x541b
#define L_FIONBIO 0x5421
#define L_SIOCGIFCONF 0x8912
#define L_SIOCGIFFLAGS 0x8913
#define L_SIOCGIFADDR 0x8915
#define L_SIOCGIFBRDADDR 0x8919
#define L_SIOCGIFNETMASK 0x891b
#define L_SIOCGIFHWADDR 0x8927

#define L_IFF_UP 0x1
#define L_IFF_BROADCAST 0x2
#define L_IFF_LOOPBACK 0x8
#define L_IFF_RUNNING 0x40
#define L_IFF_MULTICAST 0x1000

#define IFREQ_SIZE 32
#define IFNAME_SIZE 16

typedef struct
{
    const char *name;
    uint32_t addr, mask; /* network byte order */
    uint16_t flags;
} NetIf;

/* This PC's first non-loopback IPv4 address, or 0. */
static uint32_t localIPv4(void)
{
    static uint32_t cached;
    static int done;
    char host[256];
    struct addrinfo hints, *res, *p;

    if (done)
        return cached;
    done = 1;
    if (gethostname(host, sizeof(host)) != 0)
        return 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    if (getaddrinfo(host, NULL, &hints, &res) != 0)
        return 0;
    for (p = res; p; p = p->ai_next)
    {
        uint32_t a = ((struct sockaddr_in *)p->ai_addr)->sin_addr.s_addr;
        if ((ntohl(a) >> 24) != 127)
        {
            cached = a;
            break;
        }
    }
    freeaddrinfo(res);
    return cached;
}

static int listInterfaces(NetIf *out)
{
    int n = 0;
    out[n].name = "lo";
    out[n].addr = htonl(0x7f000001);
    out[n].mask = htonl(0xff000000);
    out[n].flags = L_IFF_UP | L_IFF_RUNNING | L_IFF_LOOPBACK;
    n++;
    if (netLinkActive())
    {
        /* Link play on this PC: eth0 is this cabinet's loopback address. */
        out[n].name = "eth0";
        out[n].addr = netLinkLocalAddress();
        out[n].mask = htonl(0xffffff00);
        out[n].flags = L_IFF_UP | L_IFF_RUNNING | L_IFF_BROADCAST | L_IFF_MULTICAST;
        n++;
    }
    else if (localIPv4())
    {
        out[n].name = "eth0";
        out[n].addr = localIPv4();
        out[n].mask = htonl(0xffffff00);
        out[n].flags = L_IFF_UP | L_IFF_RUNNING | L_IFF_BROADCAST | L_IFF_MULTICAST;
        n++;
    }
    return n;
}

static void putSockaddrIn(unsigned char *dst, uint32_t addr)
{
    memset(dst, 0, 16);
    *(uint16_t *)dst = AF_INET; /* sa_family, host order on Linux */
    memcpy(dst + 4, &addr, 4);  /* sin_port stays 0 */
}

static const NetIf *findInterface(const char *name, NetIf *all, int n)
{
    for (int i = 0; i < n; i++)
        if (strncmp(name, all[i].name, IFNAME_SIZE) == 0)
            return &all[i];
    return NULL;
}

int netIoctlWin(int fd, unsigned long request, void *argp, int *handled)
{
    NetIf ifs[2];
    int n;
    unsigned char *ifr = (unsigned char *)argp;
    const NetIf *nif;

    *handled = 1;
    switch (request)
    {
        case L_FIONBIO:
        case L_FIONREAD:
        {
            u_long v = argp ? *(int *)argp : 0;
            if (ioctlsocket((SOCKET)fd, request == L_FIONBIO ? FIONBIO : FIONREAD, &v) != 0)
                return -1;
            if (request == L_FIONREAD && argp)
                *(int *)argp = (int)v;
            return 0;
        }
        case L_SIOCGIFCONF:
        {
            /* struct ifconf { int ifc_len; char *ifc_buf; } */
            int *len = (int *)argp;
            unsigned char *buf = *(unsigned char **)((char *)argp + 4);
            int room = *len / IFREQ_SIZE, i;
            n = listInterfaces(ifs);
            if (!buf)
            {
                *len = n * IFREQ_SIZE;
                return 0;
            }
            for (i = 0; i < n && i < room; i++)
            {
                unsigned char *r = buf + i * IFREQ_SIZE;
                memset(r, 0, IFREQ_SIZE);
                strncpy((char *)r, ifs[i].name, IFNAME_SIZE - 1);
                putSockaddrIn(r + IFNAME_SIZE, ifs[i].addr);
            }
            *len = i * IFREQ_SIZE;
            return 0;
        }
        case L_SIOCGIFFLAGS:
        case L_SIOCGIFADDR:
        case L_SIOCGIFBRDADDR:
        case L_SIOCGIFNETMASK:
        case L_SIOCGIFHWADDR:
            n = listInterfaces(ifs);
            nif = findInterface((const char *)ifr, ifs, n);
            if (!nif)
            {
                errno = ENODEV;
                return -1;
            }
            if (request == L_SIOCGIFFLAGS)
            {
                memset(ifr + IFNAME_SIZE, 0, 16);
                *(uint16_t *)(ifr + IFNAME_SIZE) = nif->flags;
            }
            else if (request == L_SIOCGIFADDR)
                putSockaddrIn(ifr + IFNAME_SIZE, nif->addr);
            else if (request == L_SIOCGIFNETMASK)
                putSockaddrIn(ifr + IFNAME_SIZE, nif->mask);
            else if (request == L_SIOCGIFBRDADDR)
                putSockaddrIn(ifr + IFNAME_SIZE, (nif->addr & nif->mask) | ~nif->mask);
            else
            {
                /* ARPHRD_ETHER, a stable locally administered MAC from the address */
                unsigned char *hw = ifr + IFNAME_SIZE;
                memset(hw, 0, 16);
                *(uint16_t *)hw = 1;
                if (!(nif->flags & L_IFF_LOOPBACK))
                {
                    hw[2] = 0x02;
                    hw[3] = 0x00;
                    memcpy(hw + 4, &nif->addr, 4);
                }
            }
            return 0;
    }
    *handled = 0;
    return 0;
}
#endif
