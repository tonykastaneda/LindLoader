#pragma once

#include <stdio.h>
#include <unistd.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#endif

void hummerSetIPs(size_t firstAddress);
int rtunedGetCabs();
void rtunedGetLinkIdandCabs();
void rtunedSetIPs();
int rtunedBind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int getHostByName(struct in_addr *param_1, const char *param_2);

#ifdef __cplusplus
extern "C" {
#endif
const char *networkRemapInetAddr(const char *cp);
const char *networkRemapInetAton(const char *cp);
#ifdef __cplusplus
}
#endif
