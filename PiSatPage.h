#pragma once

#include <QString>

struct PiSatDevicePage {
    int offset = 0;
    int total = 0;
    int nextOffset = 0;
    QString record;
};

bool parsePiSatDevicePage(const QString &response, PiSatDevicePage *page);
