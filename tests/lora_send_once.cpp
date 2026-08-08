#include <iostream>
#include <pigpio.h>
#include <cstring>

const char* UART_DEV = "/dev/ttyS0";
#define BAUD 9600

int main() {
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio init failed\n";
        return 1;
    }
    int fd = serOpen((char*)UART_DEV, BAUD, 0);
    if (fd < 0) {
        std::cerr << "UART open failed\n";
        gpioTerminate();
        return 2;
    }

    for (int i = 0; i < 6; ++i) {
        char msg[] = "PING\n";
        serWrite(fd, msg, strlen(msg));
        std::cout << "TX: " << msg;
        std::cout.flush();
        gpioDelay(1000000);
    }

    serClose(fd);
    gpioTerminate();
    return 0;
}
