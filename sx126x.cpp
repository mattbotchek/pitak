#include "sx126x.h"
#include <pigpio.h>
#include <algorithm>
#include <iostream>
#include <cstring>

#define M0 22
#define M1 27

SX126X::SX126X(const std::string &serialDev, int baud)
    : fd(-1)
{
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio init failed\n";
        fd = -1;
        return;
    }

    gpioSetMode(M0, PI_OUTPUT);
    gpioSetMode(M1, PI_OUTPUT);

    gpioWrite(M0, 0);
    gpioWrite(M1, 0);

    fd = serOpen(const_cast<char*>(serialDev.c_str()), baud, 0);
    if (fd < 0) {
        std::cerr << "UART open failed\n";
        gpioTerminate();
        fd = -1;
    }
}

SX126X::~SX126X() {
    if (fd >= 0)
        serClose(fd);
    gpioTerminate();
}

void SX126X::send(const std::string &data) {
    if (fd < 0) return;

    int len = static_cast<int>(std::min<size_t>(data.size(), 255));
    char tmp[256];
    memcpy(tmp, data.c_str(), len);
    serWrite(fd, tmp, len);
}

bool SX126X::available() {
    if (fd < 0) return false;
    return serDataAvailable(fd) > 0;
}

int SX126X::receive(uint8_t *buf, int maxLen) {
    if (fd < 0 || maxLen <= 0)
        return -1;

    int avail = serDataAvailable(fd);
    if (avail <= 0)
        return 0;

    int toRead = std::min(avail, maxLen - 1);
    int n = serRead(fd, reinterpret_cast<char*>(buf), toRead);
    if (n > 0) {
        buf[n] = '\0';
    }
    return n;
}
