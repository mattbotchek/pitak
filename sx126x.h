#pragma once
#include <string>
#include <cstdint>

class SX126X {
public:
    // Basic constructor used by tools/hosts that assume default GPIO pins
    SX126X(const std::string &serialDev, int baud);
    // Full constructor allowing custom GPIO pins and AUX polarity
    SX126X(const std::string &serialDev, int baud,
           int m0_gpio, int m1_gpio, int aux_gpio, bool aux_active_high);
    ~SX126X();

    bool send(const std::string &data);
    int receive(uint8_t *buf, int maxLen);
    bool available();
    bool isReady() const;

private:
    int fd;
    bool pigpioReady;
    int m0_gpio_ = 22;
    int m1_gpio_ = 27;
    int aux_gpio_ = 17;
    bool aux_active_high_ = true;
    bool waitForAux(int timeoutMs) const;
};
