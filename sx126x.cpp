#include "sx126x.h"
#include <pigpio.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#define M0 22
#define M1 27
#define AUX 17

SX126X::SX126X(const std::string &serialDev, int baud)
    : fd(-1), pigpioReady(false)
{
    const uint32_t internalFlags = gpioCfgGetInternals();
    if (gpioCfgSetInternals(internalFlags | PI_CFG_NOSIGHANDLER) < 0 ||
        gpioCfgInterfaces(PI_DISABLE_FIFO_IF | PI_DISABLE_SOCK_IF) < 0) {
        std::cerr << "pigpio configuration failed\n";
        return;
    }
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio init failed\n";
        return;
    }
    pigpioReady = true;

    // Set output latches low before enabling output mode to avoid a mode-select glitch.
    if (gpioWrite(M0, 0) < 0 || gpioWrite(M1, 0) < 0 ||
        gpioSetMode(M0, PI_OUTPUT) < 0 || gpioSetMode(M1, PI_OUTPUT) < 0 ||
        gpioSetMode(AUX, PI_INPUT) < 0 || gpioSetPullUpDown(AUX, PI_PUD_UP) < 0) {
        std::cerr << "LoRa GPIO setup failed\n";
        gpioTerminate();
        pigpioReady = false;
        return;
    }
    if (!waitForAux(1000)) {
        std::cerr << "LoRa AUX not ready\n";
        gpioTerminate();
        pigpioReady = false;
        return;
    }

    fd = serOpen(const_cast<char*>(serialDev.c_str()), baud, 0);
    if (fd < 0) {
        std::cerr << "UART open failed\n";
        gpioTerminate();
        pigpioReady = false;
        fd = -1;
    }
}

SX126X::~SX126X() {
    if (fd >= 0)
        serClose(fd);
    if (pigpioReady)
        gpioTerminate();
}

bool SX126X::send(const std::string &data) {
    if (fd < 0 || data.empty() || data.size() > 255 || gpioRead(AUX) != 1)
        return false;

    const unsigned len = static_cast<unsigned>(data.size());
    const int result = serWrite(fd, const_cast<char *>(data.data()), len);
    return result == 0;
}

bool SX126X::available() {
    if (fd < 0) return false;
    return serDataAvailable(fd) > 0;
}

bool SX126X::isReady() const {
    return fd >= 0 && pigpioReady;
}

bool SX126X::waitForAux(int timeoutMs) const {
    if (!pigpioReady)
        return false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (gpioRead(AUX) == 1)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return gpioRead(AUX) == 1;
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
