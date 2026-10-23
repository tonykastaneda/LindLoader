#if defined(_WIN32) || defined(__MINGW32__)
// #include <mutex>
#include "networkBridge.hpp"
#include "symbolResolver.hpp"
#include "../log/log.h"
#include "../patching/patchNetwork.h"
#include "../patching/netLink.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>
#include <cerrno>

static std::mutex g_net_mutex;

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

// Helper to map WSA errors to POSIX errno

// Some POSIX error codes might not be defined in MinGW's <cerrno>
#ifndef ENOTSOCK
#define ENOTSOCK EINVAL
#endif
#ifndef EDESTADDRREQ
#define EDESTADDRREQ EINVAL
#endif
#ifndef EMSGSIZE
#define EMSGSIZE EINVAL
#endif
#ifndef EPROTOTYPE
#define EPROTOTYPE EINVAL
#endif
#ifndef ENOPROTOOPT
#define ENOPROTOOPT EINVAL
#endif
#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT EINVAL
#endif
#ifndef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT EINVAL
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP EINVAL
#endif
#ifndef EPFNOSUPPORT
#define EPFNOSUPPORT EINVAL
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT EINVAL
#endif
#ifndef EADDRINUSE
#define EADDRINUSE EINVAL
#endif
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL EINVAL
#endif
#ifndef ENETDOWN
#define ENETDOWN EINVAL
#endif
#ifndef ENETUNREACH
#define ENETUNREACH EINVAL
#endif
#ifndef ENETRESET
#define ENETRESET EINVAL
#endif
#ifndef ECONNABORTED
#define ECONNABORTED EINVAL
#endif
#ifndef ECONNRESET
#define ECONNRESET EINVAL
#endif
#ifndef ENOBUFS
#define ENOBUFS EINVAL
#endif
#ifndef EISCONN
#define EISCONN EINVAL
#endif
#ifndef ENOTCONN
#define ENOTCONN EINVAL
#endif
#ifndef ESHUTDOWN
#define ESHUTDOWN EINVAL
#endif
#ifndef ETOOMANYREFS
#define ETOOMANYREFS EINVAL
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT EINVAL
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED EINVAL
#endif
#ifndef EHOSTDOWN
#define EHOSTDOWN EINVAL
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH EINVAL
#endif
#ifndef EPROCLIM
#define EPROCLIM EINVAL
#endif
#ifndef EUSERS
#define EUSERS EINVAL
#endif
#ifndef EDQUOT
#define EDQUOT EINVAL
#endif
#ifndef ESTALE
#define ESTALE EINVAL
#endif
#ifndef EREMOTE
#define EREMOTE EINVAL
#endif

/* Winsock error -> errno as the game sees it. The game reads errno
   through __errno_location (the C runtime's errno), and expects Linux
   numbers: 1-34 match the C runtime, but the network codes do not
   (e.g. EINPROGRESS is 115 on Linux, 112 in MinGW), so return Linux
   values here. */
static int mapWSAErrorToErrno(int wsaError)
{
    switch (wsaError)
    {
        case WSAEINTR: return 4;            /* EINTR */
        case WSAEBADF: return 9;            /* EBADF */
        case WSAEACCES: return 13;          /* EACCES */
        case WSAEFAULT: return 14;          /* EFAULT */
        case WSAEINVAL: return 22;          /* EINVAL */
        case WSAEMFILE: return 24;          /* EMFILE */
        case WSAEWOULDBLOCK: return 11;     /* EAGAIN / EWOULDBLOCK */
        case WSAEINPROGRESS: return 115;    /* EINPROGRESS */
        case WSAEALREADY: return 114;       /* EALREADY */
        case WSAENOTSOCK: return 88;        /* ENOTSOCK */
        case WSAEDESTADDRREQ: return 89;    /* EDESTADDRREQ */
        case WSAEMSGSIZE: return 90;        /* EMSGSIZE */
        case WSAEPROTOTYPE: return 91;      /* EPROTOTYPE */
        case WSAENOPROTOOPT: return 92;     /* ENOPROTOOPT */
        case WSAEPROTONOSUPPORT: return 93; /* EPROTONOSUPPORT */
        case WSAESOCKTNOSUPPORT: return 94; /* ESOCKTNOSUPPORT */
        case WSAEOPNOTSUPP: return 95;      /* EOPNOTSUPP */
        case WSAEPFNOSUPPORT: return 96;    /* EPFNOSUPPORT */
        case WSAEAFNOSUPPORT: return 97;    /* EAFNOSUPPORT */
        case WSAEADDRINUSE: return 98;      /* EADDRINUSE */
        case WSAEADDRNOTAVAIL: return 99;   /* EADDRNOTAVAIL */
        case WSAENETDOWN: return 100;       /* ENETDOWN */
        case WSAENETUNREACH: return 101;    /* ENETUNREACH */
        case WSAENETRESET: return 102;      /* ENETRESET */
        case WSAECONNABORTED: return 103;   /* ECONNABORTED */
        case WSAECONNRESET: return 104;     /* ECONNRESET */
        case WSAENOBUFS: return 105;        /* ENOBUFS */
        case WSAEISCONN: return 106;        /* EISCONN */
        case WSAENOTCONN: return 107;       /* ENOTCONN */
        case WSAESHUTDOWN: return 108;      /* ESHUTDOWN */
        case WSAETOOMANYREFS: return 109;   /* ETOOMANYREFS */
        case WSAETIMEDOUT: return 110;      /* ETIMEDOUT */
        case WSAECONNREFUSED: return 111;   /* ECONNREFUSED */
        case WSAELOOP: return 40;           /* ELOOP */
        case WSAENAMETOOLONG: return 36;    /* ENAMETOOLONG */
        case WSAEHOSTDOWN: return 112;      /* EHOSTDOWN */
        case WSAEHOSTUNREACH: return 113;   /* EHOSTUNREACH */
        case WSAENOTEMPTY: return 39;       /* ENOTEMPTY */
        case WSAEUSERS: return 87;          /* EUSERS */
        case WSAEDQUOT: return 122;         /* EDQUOT */
        case WSAESTALE: return 116;         /* ESTALE */
        case WSAEREMOTE: return 66;         /* EREMOTE */
        default: return 22;                 /* EINVAL */
    }
}

namespace NetworkBridge
{
    void initBridges()
    {
        log_info("Initializing Network Bridges...");

        MAP("socket", bridgeSocket);
        MAP("connect", bridgeConnect);
        MAP("bind", bridgeBind);
        MAP("listen", bridgeListen);
        MAP("accept", bridgeAccept);
        MAP("send", bridgeSend);
        MAP("recv", bridgeRecv);
        MAP("sendto", bridgeSendto);
        MAP("recvfrom", bridgeRecvfrom);
        MAP("shutdown", shutdown);
        MAP("setsockopt", bridgeSetsockopt);
        MAP("getsockopt", bridgeGetsockopt);
        MAP("getsockname", bridgeGetsockname);
        MAP("sendmsg", bridgeSendmsg);
        MAP("recvmsg", bridgeRecvmsg);
        MAP("inet_pton", bridgeInet_pton);
        MAP("inet_aton", bridgeInet_aton);
        MAP("inet_addr", bridgeInet_addr);
        MAP("inet_ntoa", bridgeInet_ntoa);
        MAP("ntohs", bridgeNtohs);
        MAP("htons", bridgeHtons);
        MAP("htonl", bridgeHtonl);
        MAP("ntohl", bridgeNtohl);
        MAP("gethostbyname_r", bridgeGethostbyname_r);
        MAP("gethostbyaddr_r", bridgeGethostbyaddr_r);
        MAP("gethostbyaddr", bridgeGethostbyaddr_r);
        MAP("gethostbyname", bridgeGethostbyname_r);
        MAP("gethostname", bridgeGethostname);
        MAP("getservbyname", bridgeGethostbyname_r);
    }

    /* Same per-game address remapping as the Linux interposers
       (patching/patchNetwork.c), e.g. Sega Race TV's cabinet addresses. */
    unsigned long bridgeInet_addr(const char *cp)
    {
        return inet_addr(networkRemapInetAddr(cp));
    }

    int bridgeInet_aton(const char *cp, struct in_addr *inp)
    {
        cp = networkRemapInetAton(cp);
        unsigned long addr = inet_addr(cp);
        if (addr == INADDR_NONE && strcmp(cp, "255.255.255.255") != 0)
            return 0;
        if (inp)
            inp->s_addr = addr;
        return 1;
    }

    int bridgeInet_pton(int af, const char *src, void *dst)
    {
        return InetPtonA(af, src, dst);
    }

    char *bridgeInet_ntoa(struct in_addr in)
    {
        return inet_ntoa(in);
    }
    uint16_t bridgeNtohs(uint16_t netshort)
    {
        return ntohs(netshort);
    }
    uint16_t bridgeHtons(uint16_t hostshort)
    {
        return htons(hostshort);
    }
    uint32_t bridgeNtohl(uint32_t netlong)
    {
        return ntohl(netlong);
    }
    uint32_t bridgeHtonl(uint32_t hostlong)
    {
        return htonl(hostlong);
    }
}; // namespace NetworkBridge

/* Link play on this PC (patching/netLink.h). Every cabinet is a loopback address, 127.0.49.N for
   Sega Race TV; the games' LAN broadcasts don't exist on loopback, so a broadcast becomes one datagram
   to each cabinet, sent to a shadow port where the cabinets' broadcast sockets listen instead. */
static bool linkIsBroadcast(const struct sockaddr *a)
{
    return a && a->sa_family == AF_INET && (ntohl(((const struct sockaddr_in *)a)->sin_addr.s_addr) & 0xff) == 0xff;
}

static bool linkIsMulticast(const struct sockaddr *a)
{
    return a && a->sa_family == AF_INET && (ntohl(((const struct sockaddr_in *)a)->sin_addr.s_addr) >> 28) == 0xe;
}

static u_short linkShadowPort(u_short port)
{
    return (u_short)(port + NET_LINK_SHADOW_PORT_OFFSET);
}

/* Datagrams and connections must come from this cabinet's address, not 127.0.0.1: bind the
   socket to it first if the game hasn't bound it. */
static void linkBindLocal(SOCKET s)
{
    struct sockaddr_in a;
    int len = sizeof(a);
    if (getsockname(s, (struct sockaddr *)&a, &len) == 0)
        return;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = netLinkLocalAddress();
    bind(s, (const struct sockaddr *)&a, sizeof(a));
}

extern "C" SOCKET bridgeSocket(int af, int type, int protocol)
{
    log_info(">>> socket called: af=%d, type=%d, protocol=%d", af, type, protocol);
    SOCKET s = socket(af, type, protocol);
    if (s == INVALID_SOCKET)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
        return (SOCKET)-1;
    }
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
    /* Linux doesn't fail an unconnected UDP socket's next receive when an earlier datagram found no
       listener (ICMP port unreachable); Windows does (WSAECONNRESET) unless told not to. Link play
       sends to cabinets that may not be running. */
    if (type == SOCK_DGRAM)
    {
        BOOL report = FALSE;
        DWORD bytes = 0;
        WSAIoctl(s, SIO_UDP_CONNRESET, &report, sizeof(report), NULL, 0, &bytes, NULL, NULL);
    }
    log_info(">>> socket EXIT: returning %lld", (long long)s);
    return s;
}

extern "C" int bridgeConnect(SOCKET s, const struct sockaddr *name, int namelen)
{
    const struct sockaddr_in *to = (const struct sockaddr_in *)name;
    log_info(">>> connect called: socket=%lld to %s:%u", (long long)s,
             name && name->sa_family == AF_INET ? inet_ntoa(to->sin_addr) : "?",
             name && name->sa_family == AF_INET ? ntohs(to->sin_port) : 0);
    if (netLinkActive())
        linkBindLocal(s);
    int ret = connect(s, name, namelen);
    if (ret == SOCKET_ERROR)
    {
        int wsa = WSAGetLastError();
        /* A non-blocking connect that has started: Linux says EINPROGRESS,
           Winsock says WSAEWOULDBLOCK. */
        errno = wsa == WSAEWOULDBLOCK ? 115 : mapWSAErrorToErrno(wsa);
    }
    log_info(">>> connect EXIT: returning %d (WSAError=%d)", ret, ret < 0 ? WSAGetLastError() : 0);
    return ret;
}

extern "C" int bridgeBind(SOCKET s, const struct sockaddr *name, int namelen)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)name;
    log_info(">>> bind called: socket=%lld %s:%u", (long long)s, name && name->sa_family == AF_INET ? inet_ntoa(in->sin_addr) : "?",
             name && name->sa_family == AF_INET ? ntohs(in->sin_port) : 0);
    if (netLinkActive() && name && name->sa_family == AF_INET)
    {
        /* Link play: a broadcast socket listens on its shadow port, any other socket on this
           cabinet's address instead of every address (the other cabinets use the same ports). */
        struct sockaddr_in local = *in;
        if (linkIsBroadcast(name))
        {
            BOOL on = TRUE;
            setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
            local.sin_port = htons(linkShadowPort(ntohs(in->sin_port)));
        }
        if (linkIsBroadcast(name) || in->sin_addr.s_addr == htonl(INADDR_ANY))
            local.sin_addr.s_addr = netLinkLocalAddress();
        int ret = bind(s, (const struct sockaddr *)&local, sizeof(local));
        log_info(">>> bind (link play): %s:%u -> %d", inet_ntoa(local.sin_addr), ntohs(local.sin_port), ret);
        if (ret == SOCKET_ERROR)
            errno = mapWSAErrorToErrno(WSAGetLastError());
        return ret;
    }
    int ret = bind(s, name, namelen);
    /* Linux lets a socket bind to a broadcast address (e.g. 127.255.255.255
       or 192.168.1.255) to receive that subnet's broadcasts; Windows refuses
       (WSAEADDRNOTAVAIL). Bind to any address on the same port instead,
       which receives the same broadcasts. */
    if (ret == SOCKET_ERROR && WSAGetLastError() == WSAEADDRNOTAVAIL && name && name->sa_family == AF_INET &&
        (ntohl(in->sin_addr.s_addr) & 0xff) == 0xff)
    {
        struct sockaddr_in any = *in;
        any.sin_addr.s_addr = htonl(INADDR_ANY);
        ret = bind(s, (const struct sockaddr *)&any, sizeof(any));
        log_info(">>> bind: broadcast address not bindable on Windows, bound 0.0.0.0:%u instead (%d)", ntohs(in->sin_port), ret);
    }
    if (ret == SOCKET_ERROR)
    {
        int wsa = WSAGetLastError();
        log_info(">>> bind failed: WSA error %d", wsa);
        errno = mapWSAErrorToErrno(wsa);
    }
    log_info(">>> bind EXIT: returning %d", ret);
    return ret;
}

extern "C" int bridgeListen(SOCKET s, int backlog)
{
    log_info(">>> listen called: socket=%lld, backlog=%d", (long long)s, backlog);
    int ret = listen(s, backlog);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> listen EXIT: returning %d", ret);
    return ret;
}

extern "C" SOCKET bridgeAccept(SOCKET s, struct sockaddr *addr, int *addrlen)
{
    log_info(">>> accept ENTRY: socket=%lld (THIS MAY BLOCK!)", (long long)s);
    SOCKET ret = accept(s, addr, addrlen);
    if (ret == INVALID_SOCKET)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
        return (SOCKET)-1;
    }
    log_info(">>> accept EXIT: returning %lld (WSAError=%d)", (long long)ret, ret == INVALID_SOCKET ? WSAGetLastError() : 0);
    return ret;
}

extern "C" int bridgeRecv(SOCKET s, char *buf, int len, int flags)
{
    log_info(">>> recv ENTRY: socket=%lld, len=%d (THIS MAY BLOCK!)", (long long)s, len);

    if (flags & 0x40)
    {
        flags &= ~0x40;
    }

    int ret = recv(s, buf, len, flags);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> recv EXIT: returning %d", ret);
    return ret;
}

extern "C" int bridgeSend(SOCKET s, const char *buf, int len, int flags)
{
    log_info(">>> send called: socket=%lld, len=%d", (long long)s, len);

    flags &= ~(0x4000 | 0x40);

    int ret = send(s, buf, len, flags);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> send EXIT: returning %d", ret);
    return ret;
}

extern "C" int bridgeRecvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
    log_info(">>> recvfrom ENTRY: socket=%lld, len=%d (THIS MAY BLOCK!)", (long long)s, len);
    if (flags & 0x40)
        flags &= ~0x40;
    int ret = recvfrom(s, buf, len, flags, from, fromlen);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> recvfrom EXIT: returning %d from %s:%u", ret,
             ret >= 0 && from && from->sa_family == AF_INET ? inet_ntoa(((struct sockaddr_in *)from)->sin_addr) : "?",
             ret >= 0 && from && from->sa_family == AF_INET ? ntohs(((struct sockaddr_in *)from)->sin_port) : 0);
    return ret;
}

extern "C" int bridgeSendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
    const struct sockaddr_in *dst = (const struct sockaddr_in *)to;
    log_info(">>> sendto called: socket=%lld, len=%d to %s:%u", (long long)s, len,
             to && to->sa_family == AF_INET ? inet_ntoa(dst->sin_addr) : "?",
             to && to->sa_family == AF_INET ? ntohs(dst->sin_port) : 0);
    flags &= ~(0x4000 | 0x40);
    if (netLinkActive())
    {
        linkBindLocal(s);
        if (linkIsMulticast(to))
        {
            /* LAN multicast: one copy per cabinet on the group's port, where the members listen. */
            struct sockaddr_in copy = *dst;
            const u_long subnet = ntohl(netLinkLocalAddress()) & 0xffffff00;
            int sent = SOCKET_ERROR;
            for (int cab = 1; cab <= NET_LINK_CABINETS; cab++)
            {
                copy.sin_addr.s_addr = htonl(subnet | cab);
                if (sendto(s, buf, len, flags, (const struct sockaddr *)&copy, sizeof(copy)) != SOCKET_ERROR)
                    sent = len;
            }
            if (sent == SOCKET_ERROR)
                errno = mapWSAErrorToErrno(WSAGetLastError());
            return sent;
        }
        if (linkIsBroadcast(to))
        {
            /* One copy per cabinet, this one included (Linux delivers broadcasts to the sender too). */
            struct sockaddr_in copy = *dst;
            const u_long subnet = ntohl(netLinkLocalAddress()) & 0xffffff00;
            int sent = SOCKET_ERROR;
            copy.sin_port = htons(linkShadowPort(ntohs(dst->sin_port)));
            for (int cab = 1; cab <= NET_LINK_CABINETS; cab++)
            {
                copy.sin_addr.s_addr = htonl(subnet | cab);
                if (sendto(s, buf, len, flags, (const struct sockaddr *)&copy, sizeof(copy)) != SOCKET_ERROR)
                    sent = len;
            }
            if (sent == SOCKET_ERROR)
                errno = mapWSAErrorToErrno(WSAGetLastError());
            return sent;
        }
    }
    int ret = sendto(s, buf, len, flags, to, tolen);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> sendto EXIT: returning %d", ret);
    return ret;
}

/* Linux socket option numbers are not Winsock's. Translate SOL_SOCKET and
   IPPROTO_IP options; returns false for options Windows has no equivalent
   for. SO_RCVTIMEO/SO_SNDTIMEO take a struct timeval on Linux and a DWORD
   of milliseconds on Windows (converted into *ms). */
static bool linuxSockoptToWin(int *level, int *optname, const char **optval, int *optlen, DWORD *ms)
{
    if (*level == 1) /* SOL_SOCKET */
    {
        static const int map[][2] = {{1, SO_DEBUG},      {2, SO_REUSEADDR}, {3, SO_TYPE},     {4, SO_ERROR},
                                     {5, SO_DONTROUTE},  {6, SO_BROADCAST}, {7, SO_SNDBUF},   {8, SO_RCVBUF},
                                     {9, SO_KEEPALIVE},  {10, SO_OOBINLINE}, {13, SO_LINGER}, {18, SO_RCVLOWAT},
                                     {19, SO_SNDLOWAT},  {20, SO_RCVTIMEO}, {21, SO_SNDTIMEO}};
        *level = SOL_SOCKET;
        for (auto &m : map)
            if (m[0] == *optname)
            {
                *optname = m[1];
                if ((m[1] == SO_RCVTIMEO || m[1] == SO_SNDTIMEO) && *optval && *optlen >= 8)
                {
                    const long *tv = (const long *)*optval; /* tv_sec, tv_usec */
                    *ms = (DWORD)(tv[0] * 1000 + tv[1] / 1000);
                    *optval = (const char *)ms;
                    *optlen = sizeof(DWORD);
                }
                return true;
            }
        return false;
    }
    if (*level == 0) /* IPPROTO_IP */
    {
        static const int map[][2] = {{1, IP_TOS},          {2, IP_TTL},             {32, IP_MULTICAST_IF},
                                     {33, IP_MULTICAST_TTL}, {34, IP_MULTICAST_LOOP}, {35, IP_ADD_MEMBERSHIP},
                                     {36, IP_DROP_MEMBERSHIP}};
        for (auto &m : map)
            if (m[0] == *optname)
            {
                *optname = m[1];
                return true;
            }
        return false;
    }
    return true; /* IPPROTO_TCP (TCP_NODELAY = 1 on both) and others */
}

extern "C" int bridgeSetsockopt(SOCKET s, int level, int optname, const char *optval, int optlen)
{
    log_info(">>> setsockopt called: socket=%lld, level=%d, optname=%d", (long long)s, level, optname);

    DWORD ms;
    if (!linuxSockoptToWin(&level, &optname, &optval, &optlen, &ms))
    {
        log_info(">>> setsockopt: no Windows equivalent, ignored");
        return 0;
    }

    int ret = setsockopt(s, level, optname, optval, optlen);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> setsockopt EXIT: returning %d", ret);
    return ret;
}

extern "C" int bridgeGetsockname(SOCKET s, struct sockaddr *name, int *namelen)
{
    int ret = getsockname(s, name, namelen);
    if (ret == SOCKET_ERROR)
        errno = mapWSAErrorToErrno(WSAGetLastError());
    return ret;
}

/* Winsock has no sendmsg/recvmsg: they're built on sendto/recvfrom. Sega Race TV matches linked
   cabinets with them. */
/* struct msghdr and struct iovec as i386 Linux lays them out. */
struct LinuxIovec
{
    void *base;
    uint32_t len;
};
struct LinuxMsghdr
{
    void *name;
    uint32_t namelen;
    LinuxIovec *iov;
    uint32_t iovlen;
    void *control;
    uint32_t controllen;
    int flags;
};

/* sendmsg: the iovecs gathered into one datagram for sendto (which also applies link play). */
extern "C" int bridgeSendmsg(SOCKET s, const void *msg, int flags)
{
    const LinuxMsghdr *m = (const LinuxMsghdr *)msg;
    size_t total = 0;
    for (uint32_t i = 0; i < m->iovlen; i++)
        total += m->iov[i].len;
    char *buf = (char *)malloc(total ? total : 1);
    if (!buf)
    {
        errno = ENOMEM;
        return -1;
    }
    size_t at = 0;
    for (uint32_t i = 0; i < m->iovlen; i++)
    {
        memcpy(buf + at, m->iov[i].base, m->iov[i].len);
        at += m->iov[i].len;
    }
    log_info(">>> sendmsg: %u bytes", (unsigned)total);
    int ret = bridgeSendto(s, buf, (int)total, flags, (const struct sockaddr *)m->name, (int)m->namelen);
    free(buf);
    return ret;
}

/* recvmsg: one datagram scattered into the iovecs; no ancillary data. */
extern "C" int bridgeRecvmsg(SOCKET s, void *msg, int flags)
{
    LinuxMsghdr *m = (LinuxMsghdr *)msg;
    size_t total = 0;
    for (uint32_t i = 0; i < m->iovlen; i++)
        total += m->iov[i].len;
    char *buf = (char *)malloc(total ? total : 1);
    if (!buf)
    {
        errno = ENOMEM;
        return -1;
    }
    int fromlen = (int)m->namelen;
    int ret = bridgeRecvfrom(s, buf, (int)total, flags, (struct sockaddr *)m->name, m->name ? &fromlen : NULL);
    /* Linux hands over the start of a datagram too big for the buffers and sets MSG_TRUNC; Winsock
       fills the buffer the same way but fails with WSAEMSGSIZE (errno 90). */
    const bool truncated = ret < 0 && errno == 90;
    if (truncated)
        ret = (int)total;
    if (ret >= 0)
    {
        size_t at = 0;
        for (uint32_t i = 0; i < m->iovlen && at < (size_t)ret; i++)
        {
            const size_t n = ret - at < m->iov[i].len ? ret - at : m->iov[i].len;
            memcpy(m->iov[i].base, buf + at, n);
            at += n;
        }
        if (m->name)
            m->namelen = (uint32_t)fromlen;
        m->controllen = 0;
        m->flags = truncated ? 0x20 /* MSG_TRUNC */ : 0;
    }
    free(buf);
    return ret;
}

extern "C" int bridgeGetsockopt(SOCKET s, int level, int optname, char *optval, int *optlen)
{
    log_info(">>> getsockopt called: socket=%lld, level=%d, optname=%d", (long long)s, level, optname);

    {
        const char *dummy = optval;
        int len = optlen ? *optlen : 0;
        DWORD ms;
        if (!linuxSockoptToWin(&level, &optname, &dummy, &len, &ms))
        {
            if (optval && optlen && *optlen >= 4)
                memset(optval, 0, 4);
            return 0;
        }
    }

    int ret = getsockopt(s, level, optname, optval, optlen);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> getsockopt EXIT: returning %d", ret);
    return ret;
}

int NetworkBridge::bridgeGethostbyname_r(const char *name, void *ret, char *buf, size_t buflen, void **result, int *h_errnop)
{
    std::lock_guard<std::mutex> lock(g_net_mutex);
    struct hostent *he = gethostbyname(name);
    if (!he)
    {
        if (h_errnop)
            *h_errnop = WSAGetLastError();
        if (result)
            *result = nullptr;
        return -1;
    }
    memcpy(ret, he, sizeof(struct hostent));
    if (result)
        *result = ret;
    return 0;
}

int NetworkBridge::bridgeGethostbyaddr_r(const void *addr, int len, int type, void *ret, char *buf, size_t buflen, void **result,
                                         int *h_errnop)
{
    std::lock_guard<std::mutex> lock(g_net_mutex);
    struct hostent *he = gethostbyaddr((const char *)addr, len, type);
    if (!he)
    {
        if (h_errnop)
            *h_errnop = WSAGetLastError();
        if (result)
            *result = nullptr;
        return -1;
    }
    memcpy(ret, he, sizeof(struct hostent));
    if (result)
        *result = ret;
    return 0;
}

extern "C" int bridgeGethostname(char *name, size_t namelen)
{
    // fallback to localhost if gethostname fails
    if (name == nullptr || namelen == 0)
    {
        return -1;
    }
    strncpy(name, "localhost", namelen - 1);
    name[namelen - 1] = '\0';
    return 0;
}

#endif