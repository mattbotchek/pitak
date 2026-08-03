#pragma once
#include <QObject>
#include <QVariantList>
#include <QByteArray>
#include <QTimer>
#include "sx126x.h"

class LoRaManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)

public:
    explicit LoRaManager(QObject *parent = nullptr);

    Q_INVOKABLE void sendMessage(const QString &msg);
    QVariantList messages() const;

signals:
    void messagesChanged();

private:
    SX126X *radio;
    QTimer *rxTimer;
    QVariantList m_messages;
    QByteArray m_rxBuffer;

    void appendMessage(const QString &msg);
};
