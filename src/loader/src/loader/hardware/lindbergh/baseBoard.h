#ifdef __linux__
#include <stdio.h>
#include <sys/time.h>
#include <sys/select.h>
#else
#include <winsock2.h>
#include <windows.h> 
#endif

#ifdef __cplusplus
extern "C" {
#endif 

int initBaseboard();

ssize_t baseboardRead(int fd, void *buf, size_t count);
ssize_t baseboardWrite(int fd, const void *buf, size_t count);

int baseboardIoctl(int fd, unsigned long request, void *data);
int baseboardSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                    struct timeval *timeout);

#ifdef __cplusplus
}
#endif 