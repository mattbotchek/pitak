#include "sx126x.h"
#include <pigpio.h>
#include <iostream>
#include <cstring>

SX126X::SX126X(const std::string &serialDev, int baud)
{
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio init failed\n";
        fd = -1;
        return;
    }

    gpioSetMode(22, PI_OUTPUT);
    gpioSetMode(27, PI_OUTPUT);

    gpioWrite(22, 0);
    gpioWrite(27, 0);

    fd = serOpen((char*)serialDev.c_str(), baud, 0);
    if (fd < 0) {
        std::cerr << "UART open failed\n";
    }
}

SX126X::~SX126X() {
    if (fd >= 0)
        serClose(fd);
    gpioTerminate();
}

void SX126X::send(const std::string &data) {
    if (fd < 0) return;

    char buf[256];
    size_t len = std::min(data.size(), sizeof(buf)-1);

    memcpy(buf, data.c_str(), len);
    buf[len] = '\0';

    serWrite(fd, buf, len);
}

bool SX126X::available() {
    if (fd < 0) return false;
    return serDataAvailable(fd) > 0;
}

int SX126X::receive(uint8_t *buf, int maxLen) {
    if (fd < 0) return -1;

    int n = serRead(fd, (char*)buf, maxLen - 1);
    if (n > 0)
        buf[n] = '\0';

    return n;
}
