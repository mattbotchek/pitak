#include "sx126x.h"
#if defined(__linux__)
#include <pigpio.h>
#else
// Build-time fallback for non-Linux hosts (macOS, Windows) so tests and tools
// can be built without pigpio installed. At runtime on deployed Pis, Linux
// builds will use the real pigpio implementation above.
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#if defined(__linux__)
SX126X::SX126X(const std::string &serialDev, int baud)
    : fd(-1), pigpioReady(false), m0_gpio_(22), m1_gpio_(27), aux_gpio_(17), aux_active_high_(true)
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
    if (gpioWrite(m0_gpio_, 0) < 0 || gpioWrite(m1_gpio_, 0) < 0 ||
        gpioSetMode(m0_gpio_, PI_OUTPUT) < 0 || gpioSetMode(m1_gpio_, PI_OUTPUT) < 0 ||
        gpioSetMode(aux_gpio_, PI_INPUT) < 0 || gpioSetPullUpDown(aux_gpio_, PI_PUD_UP) < 0) {
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

SX126X::SX126X(const std::string &serialDev, int baud,
               int m0_gpio, int m1_gpio, int aux_gpio, bool aux_active_high)
    : fd(-1), pigpioReady(false), m0_gpio_(m0_gpio), m1_gpio_(m1_gpio), aux_gpio_(aux_gpio), aux_active_high_(aux_active_high)
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
    if (gpioWrite(m0_gpio_, 0) < 0 || gpioWrite(m1_gpio_, 0) < 0 ||
        gpioSetMode(m0_gpio_, PI_OUTPUT) < 0 || gpioSetMode(m1_gpio_, PI_OUTPUT) < 0 ||
        gpioSetMode(aux_gpio_, PI_INPUT) < 0 || gpioSetPullUpDown(aux_gpio_, PI_PUD_UP) < 0) {
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
    if (fd < 0 || data.empty() || data.size() > 255)
        return false;
    const int aux_state = gpioRead(aux_gpio_);
    const int expected = aux_active_high_ ? 1 : 0;
    if (aux_state != expected) return false;

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
    const int expected = aux_active_high_ ? 1 : 0;
    while (std::chrono::steady_clock::now() < deadline) {
        if (gpioRead(aux_gpio_) == expected)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return gpioRead(aux_gpio_) == expected;
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

#else // non-Linux stubs

SX126X::SX126X(const std::string &serialDev, int baud)
    : fd(-1), pigpioReady(false)
{
    (void)serialDev; (void)baud;
    std::cerr << "pigpio HAT control is supported on Linux only; running in stub mode\n";
}

SX126X::~SX126X() {}

bool SX126X::send(const std::string &data) {
    (void)data; return false;
}

bool SX126X::available() { return false; }

bool SX126X::isReady() const { return false; }

bool SX126X::waitForAux(int timeoutMs) const { (void)timeoutMs; return false; }

int SX126X::receive(uint8_t *buf, int maxLen) {
    (void)buf; (void)maxLen; return -1;
}

#endif
