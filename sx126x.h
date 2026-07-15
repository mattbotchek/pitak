#pragma once
#include <string>
#include <cstdint>

class SX126X {
public:
    SX126X(const std::string &serialDev, int baud);
    ~SX126X();

    void send(const std::string &data);
    int receive(uint8_t *buf, int maxLen);
    bool available();

private:
    int fd;
};
