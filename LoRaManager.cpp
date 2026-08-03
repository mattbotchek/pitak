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
            m_rxBuffer.append(reinterpret_cast<const char*>(buf), n);

            while (m_rxBuffer.contains('\n')) {
                int newlinePos = m_rxBuffer.indexOf('\n');
                QByteArray line = m_rxBuffer.left(newlinePos);
                m_rxBuffer.remove(0, newlinePos + 1);

                if (line.isEmpty())
                    continue;

                QString msg = QString::fromUtf8(line).trimmed();
                if (!msg.isEmpty()) {
                    appendMessage("RX: " + msg);
                    qDebug() << "RX:" << msg;
                }
            }

            if (m_rxBuffer.size() > 1024) {
                m_rxBuffer.clear();
            }
        }
    });

    rxTimer->start();
}

void LoRaManager::sendMessage(const QString &msg) {
    if (msg.isEmpty())
        return;

    std::string payload = msg.toStdString();
    if (!payload.empty() && payload.back() != '\n') {
        payload.push_back('\n');
    }

    radio->send(payload);
    appendMessage("TX: " + msg);
}

void LoRaManager::appendMessage(const QString &msg) {
    m_messages.append(msg);
    emit messagesChanged();
}

QVariantList LoRaManager::messages() const {
    return m_messages;
}
