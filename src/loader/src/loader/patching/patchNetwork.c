#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#include <unistd.h>

#include "../config/config.h"
#include "../log/log.h"
#include "../patching/flowControl.h"

#ifdef __linux__
#include <dlfcn.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#else
#include <ws2tcpip.h>
#endif

extern uint32_t gId;
char *rtunedIPs[4];
char *rtunedSortedIPs[4];
uint8_t rtunedLinkId = 0;
uint8_t rtunedCabs = 0;

typedef struct sockaddr_in Sockaddr;

void hummerSetIPs(size_t firstAddress)
{
    if (!getConfig()->enableNetworkPatches)
        return;

    if (strcmp(getConfig()->IpCab1, "") != 0)
        setVariable(firstAddress, (size_t)(void *)getConfig()->IpCab1);
    if (strcmp(getConfig()->IpCab2, "") != 0)
        setVariable(firstAddress + 4, (size_t)(void *)getConfig()->IpCab2);
    if (strcmp(getConfig()->IpCab3, "") != 0)
        setVariable(firstAddress + 8, (size_t)(void *)getConfig()->IpCab3);
    if (strcmp(getConfig()->IpCab4, "") != 0)
        setVariable(firstAddress + 12, (size_t)(void *)getConfig()->IpCab4);
}

int rtunedGetCabs()
{
    return rtunedCabs;
}

void rtunedGetLinkIdandCabs()
{
    FILE *f = fopen(getConfig()->sramPath, "rb");
    if (f == NULL)
        return;
    fseek(f, 0xc011, SEEK_SET);
    fread(&rtunedLinkId, 1, 1, f);
    fseek(f, 0xc019, SEEK_SET);
    fread(&rtunedCabs, 1, 1, f);
    fclose(f);
}

void rtunedRotateIps(char *ori[], char *dst[], int total, int pos)
{
    for (int i = 0; i < total; i++)
    {
        dst[i] = ori[(pos + i) % total];
    }
    dst[total - 1] = ori[pos - 1];
}

void rtunedSetIPs()
{
    if (!getConfig()->enableNetworkPatches)
        return;

    for (int x = 0; x < 4; x++)
    {
        rtunedIPs[x] = "0.0.0.0";
        rtunedSortedIPs[x] = "0.0.0.0";
    }

    uint8_t setIps = 0;
    if (strcmp(getConfig()->IpCab1, "") != 0)
    {
        rtunedIPs[0] = getConfig()->IpCab1;
        setIps++;
    }
    if (strcmp(getConfig()->IpCab2, "") != 0)
    {
        rtunedIPs[1] = getConfig()->IpCab2;
        setIps++;
    }
    if (strcmp(getConfig()->IpCab3, "") != 0)
    {
        rtunedIPs[2] = getConfig()->IpCab3;
        setIps++;
    }
    if (strcmp(getConfig()->IpCab4, "") != 0)
    {
        rtunedIPs[3] = getConfig()->IpCab4;
        setIps++;
    }
    if (setIps != rtunedCabs)
        log_warn("Warning: You have %d IPs set in the config file and %d Cabinets set in the TestMenu.", setIps, rtunedCabs);

    rtunedRotateIps(rtunedIPs, rtunedSortedIPs, rtunedCabs, rtunedLinkId);
}

int rtunedBind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (rtunedCabs > 1)
    {
        int idx1 = 0;
        int idx2 = 0;
        for (int i = 0; i < rtunedCabs; i++)
        {
            setVariable(0x0851e890 + idx1, inet_addr(rtunedSortedIPs[i]));
            setVariable(0x0851d7c0 + idx2, inet_addr(rtunedIPs[i]));
            idx1 += 16;
            idx2 += 4;
        }
        uint32_t ip = inet_addr(rtunedIPs[rtunedLinkId - 1]);
        uint32_t invIp = ((ip & 0xFF) << 24) | ((ip & 0xFF00) << 8) | ((ip & 0xFF0000) >> 8) | ((ip & 0xFF000000) >> 24);
        uint32_t invIpBcast = ((ip & 0xFF) << 24) | ((ip & 0xFF00) << 8) | ((ip & 0xFF0000) >> 8) | 0x000000FF;
        setVariable(0x0851bf90, invIp);
        setVariable(0x0851bf98, invIpBcast);
    }
    return bind(sockfd, addr, addrlen);
}

/* Address remapping shared by the Linux libc interposers below and the
   Windows network bridge (elfLoader/networkBridge.cpp), so the patches
   apply on both. Return the address the game should really use. */
const char *networkRemapInetAddr(const char *cp)
{
    if (!cp || !getConfig()->enableNetworkPatches)
        return cp;
    if (gId == TOO_SPICY_SBMV)
    {
        if (strcmp("10.0.0.1", cp) == 0 && strcmp(getConfig()->tooSpicyIpCab1, "") != 0)
            return getConfig()->tooSpicyIpCab1;
        if (strcmp("10.0.0.2", cp) == 0 && strcmp(getConfig()->tooSpicyIpCab2, "") != 0)
            return getConfig()->tooSpicyIpCab2;
    }
    else if (gId == SEGA_RACE_TV_SBPF)
    {
        // Standalone = 4, Main = 0, 2-Sub = 1, 3-Sub = 2 and 4-Sub = 3
        if (strcmp("192.168.49.1", cp) == 0 || strcmp("192.168.49.2", cp) == 0 || strcmp("192.168.49.3", cp) == 0 ||
            strcmp("192.168.49.4", cp) == 0)
        {
            // Link play on this PC: each cabinet keeps its own address, on loopback.
            if (getConfig()->loopbackLink)
            {
                static const char *const loopback[] = {"127.0.49.1", "127.0.49.2", "127.0.49.3", "127.0.49.4"};
                return loopback[cp[11] - '1'];
            }
            if (strcmp(getConfig()->srtvIP, "") != 0)
                return getConfig()->srtvIP;
            // Prevents the game to crash if the IP does not match the PC's IP.
            return "127.0.0.1";
        }
    }
    return cp;
}

const char *networkRemapInetAton(const char *cp)
{
    if (!cp || gId != HARLEY_DAVIDSON_SBRG)
        return cp;
    if (!getConfig()->enableNetworkPatches)
        return "127.0.0.1";
    if (strcmp(cp, "192.168.1.1") == 0 || strcmp(cp, "192.168.1.5") == 0 || strcmp(cp, "192.168.1.9") == 0)
        return strcmp(getConfig()->IpCab1, "") != 0 ? getConfig()->IpCab1 : "127.0.0.1";
    if ((strcmp(cp, "192.168.1.2") == 0 || strcmp(cp, "192.168.1.7") == 0 || strcmp(cp, "192.168.1.10") == 0) &&
        strcmp(getConfig()->IpCab2, "") != 0)
        return getConfig()->IpCab2;
    if ((strcmp(cp, "192.168.1.3") == 0 || strcmp(cp, "192.168.1.7") == 0 || strcmp(cp, "192.168.1.11") == 0) &&
        strcmp(getConfig()->IpCab3, "") != 0)
        return getConfig()->IpCab4; /* upstream maps cab 3 to IpCab4; kept as is */
    if ((strcmp(cp, "192.168.1.4") == 0 || strcmp(cp, "192.168.1.8") == 0 || strcmp(cp, "192.168.1.12") == 0) &&
        strcmp(getConfig()->IpCab4, "") != 0)
        return getConfig()->IpCab4;
    return cp;
}

#ifdef __linux__
int inet_aton(const char *cp, struct in_addr *inp)
{
    int (*_inet_aton)(const char *cp, struct in_addr *inp) = dlsym(RTLD_NEXT, "inet_aton");
    return _inet_aton(networkRemapInetAton(cp), inp);
}

in_addr_t inet_addr(const char *cp)
{
    in_addr_t (*_inet_addr)(const char *cp) = dlsym(RTLD_NEXT, "inet_addr");
    return _inet_addr(networkRemapInetAddr(cp));
}

#undef strstr
char *strstr(const char *haystack, const char *needle)
{
    char *(*_strstr)(const char *__haystack, const char *__needle) = dlsym(RTLD_NEXT, "strstr");

    if (strcmp(needle, "eth0") == 0 && getConfig()->enableNetworkPatches && strcmp(getConfig()->nicName, "") != 0)
    {
        return _strstr(haystack, getConfig()->nicName);
    }
    return _strstr(haystack, needle);
}

int gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
    int (*_gethostbyname_r)(const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop) =
        dlsym(RTLD_NEXT, "gethostbyname_r");

    void *addr = __builtin_return_address(0);
    if (gId == QUIZ_AXA_SBMS && addr == (void *)0x0832609d)
    {
        ret = 0;
        buf[0] = '\0';
        result = 0;
        h_errnop = 0;
        return -1;
    }
    return _gethostbyname_r(name, ret, buf, buflen, result, h_errnop);
}

int ioctl_intf(unsigned int *param_1, long unsigned int command)
{
    printf("Called ioctl_intf.\n");

    int fd;
    int result;
    struct ifreq ifr;
    char *intfname;
    
    if (strcmp(getConfig()->nicName, "") != 0)
        intfname = getConfig()->nicName;
    else
        intfname = "eth0"; 

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    // I want to get an IPv4 IP address 
    ifr.ifr_addr.sa_family = AF_INET;

    // I want IP address attached to "eth0" 
    strncpy(ifr.ifr_name, intfname, IFNAMSIZ - 1);
    result = ioctl(fd, command, &ifr);

    if (result == 0)
    {
        *param_1 = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
    }

    close(fd);

    return result;
}

int set_gateway(unsigned int *param_1)
{
    struct sockaddr_in gateway;
    inet_pton(AF_INET, "192.168.0.1", &(gateway.sin_addr));
    // printf("%s\n", inet_ntoa(((struct sockaddr_in *)&gateway)->sin_addr));
    *param_1 = ((struct sockaddr_in *)&gateway)->sin_addr.s_addr;
    return 0;
}

int amOsinfoGetNetworkProperty(uintptr_t param_1)
{
    printf("Got amOsinfoGetNetworkProperty\n");
    if (param_1 == 0)
    {
        return -2; // Original returned 0xfffffffe
    }

    // Set DHCP status to always "yes"
    *(uint *)(param_1 + 8) = 0xfffffffe;

    // GetIP
    unsigned int ip;
    ioctl_intf(&ip, SIOCGIFADDR);
    *(unsigned int *)(param_1 + 0xc) = ip;

    // GetSubnet
    unsigned int subnet;
    ioctl_intf(&subnet, SIOCGIFNETMASK);
    *(unsigned int *)(param_1 + 0x10) = subnet;

    // default route
    unsigned int gateway_value;
    set_gateway(&gateway_value);
    *(unsigned int *)(param_1 + 0x14) = gateway_value;

    // dns1
    struct sockaddr_in dns;
    inet_pton(AF_INET, "1.1.1.1", &(dns.sin_addr));
    // printf("%s\n", inet_ntoa(((struct sockaddr_in *)&dns)->sin_addr));
    *(in_addr_t *)(param_1 + 0x18) = dns.sin_addr.s_addr;

    // dns1
    struct sockaddr_in dns2;
    inet_pton(AF_INET, "1.0.0.1", &(dns2.sin_addr));
    // printf("%s\n", inet_ntoa(((struct sockaddr_in *)&dns2)->sin_addr));
    *(in_addr_t *)(param_1 + 0x18) = dns2.sin_addr.s_addr;

    return 0; // Indicate success
}

uint amOsinfoGetDhcpStatusEth0Ex(unsigned int *param_1)
{
    return 0;
}

bool getIPAddress(char *ifname, Sockaddr *saddr)
{
    return 1;
}

int getHostByName(struct in_addr *param_1, const char *param_2)
{
    printf("Called getHostByName: %s \n", param_2);
    if (strcmp(param_2, "tenporouter.loc") == 0 || strcmp(param_2, "bbrouter.loc") == 0)
    {
        struct in_addr fake_addr;
        fake_addr.s_addr = htonl(0x0A0000FE);
        printf("Returning fake addr: %s.\n", inet_ntoa(fake_addr));
        param_1->s_addr = fake_addr.s_addr;
        return 1;
    }

    if (strcmp(param_2, "naominet.jp") == 0)
    {
        struct in_addr fake_addr;
        fake_addr.s_addr = inet_addr("192.168.1.108");
        printf("Returning fake addr: %s.\n", inet_ntoa(fake_addr));
        param_1->s_addr = fake_addr.s_addr;
        return 1;
    }

    char *buf = (char *)malloc(0x2000);
    if (buf == NULL)
    {
        param_1->s_addr = 0;
        return 0;
    }

    int retCode = 0;
    int herrno = 0;
    struct hostent hostbuf;
    struct hostent *result = NULL;

    param_1->s_addr = 0;

    retCode = gethostbyname_r(param_2, &hostbuf, buf, 0x2000, &result, &herrno);

    if (retCode == 0 && result != NULL)
    {
        if (hostbuf.h_addrtype == AF_INET && hostbuf.h_addr_list[0] != NULL)
        {
            param_1->s_addr = *(in_addr_t *)hostbuf.h_addr_list[0];
            free(buf);
            return 1;
        }
    }

    free(buf);
    param_1->s_addr = 0;
    return 0;
}

#endif