#include "LoRaManager.h"
#include <QDebug>

LoRaManager::LoRaManager(QObject *parent)
    : QObject(parent)
{
    radio = new SX126X("/dev/ttyS0", 9600);

    rxTimer = new QTimer(this);
    rxTimer->setInterval(20); // 50 Hz polling

    connect(rxTimer, &QTimer::timeout, [this]() {
        if (!radio->available())
            return;

        uint8_t buf[256];
        int n = radio->receive(buf, sizeof(buf));

        if (n > 0) {
            QString msg = QString::fromUtf8((char*)buf, n);
            appendMessage("RX: " + msg);
            qDebug() << "RX RAW:" << QByteArray((char*)buf, n).toHex();
        }
    });

    rxTimer->start();
}

void LoRaManager::sendMessage(const QString &msg) {
    radio->send(msg.toStdString());
    appendMessage("TX: " + msg);
}

void LoRaManager::appendMessage(const QString &msg) {
    m_messages.append(msg);
    emit messagesChanged();
}

QVariantList LoRaManager::messages() const {
    return m_messages;
}
