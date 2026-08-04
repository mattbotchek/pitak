#include "PiSatPage.h"

#include <QStringList>

bool parsePiSatDevicePage(const QString &response, PiSatDevicePage *page)
{
    if (!page)
        return false;
    const int recordSeparator = response.indexOf(';');
    const QString header = recordSeparator < 0 ? response : response.left(recordSeparator);
    const QStringList values = header.split(',');
    if (values.size() != 2)
        return false;

    bool offsetOk = false;
    bool totalOk = false;
    const int offset = values[0].toInt(&offsetOk);
    const int total = values[1].toInt(&totalOk);
    if (!offsetOk || !totalOk || offset < 0 || total < 0)
        return false;

    const qint64 followingOffset = static_cast<qint64>(offset) + 1;
    page->offset = offset;
    page->total = total;
    page->nextOffset = (total == 0 || followingOffset >= total) ? 0 : offset + 1;
    page->record = recordSeparator < 0 ? QString() : response.mid(recordSeparator + 1).trimmed();
    return true;
}
