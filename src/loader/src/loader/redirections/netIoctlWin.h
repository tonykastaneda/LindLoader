#pragma once

/* Linux network ioctls (SIOCGIFCONF, SIOCGIF*, FIONBIO, FIONREAD) on Windows.
   Sets *handled to 0 for requests it does not know. */
int netIoctlWin(int fd, unsigned long request, void *argp, int *handled);
