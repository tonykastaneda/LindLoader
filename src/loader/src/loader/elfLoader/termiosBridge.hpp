#pragma once

#include <stddef.h>
#include <stdint.h>

struct linux_termios
{
    unsigned int c_iflag;   // input mode flags
    unsigned int c_oflag;   // output mode flags
    unsigned int c_cflag;   // control mode flags
    unsigned int c_lflag;   // local mode flags
    unsigned char c_line;   // line discipline
    unsigned char c_cc[32]; // control characters
    unsigned int c_ispeed;  // input speed
    unsigned int c_ospeed;  // output speed
};

namespace TermiosBridge
{
    void initBridges();

    int bridgeTcgetattr(int fd, linux_termios *termios_p);
    int bridgeTcsetattr(int fd, int optional_actions, const linux_termios *termios_p);
    int bridgeTcflush(int fd, int queue_selector);
    int bridgeTcdrain(int fd);
    int bridgeCfsetispeed(linux_termios *termios_p, unsigned int speed);
    int bridgeCfsetospeed(linux_termios *termios_p, unsigned int speed);
} // namespace TermiosBridge
