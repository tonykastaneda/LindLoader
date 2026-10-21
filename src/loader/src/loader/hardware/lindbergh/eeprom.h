#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int initEeprom();
int eepromIoctl(int fd, unsigned int request, void *data);

#ifdef __cplusplus
}
#endif
