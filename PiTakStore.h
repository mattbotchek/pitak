#pragma once

#include <QSqlDatabase>
#include <QVariantList>

class PiTakStore {
public:
    PiTakStore();
    ~PiTakStore();

    bool open(const QString &path, QString *error);
    quint64 nextOutboundNonce();
    bool acceptInboundNonce(const QString &source, quint64 nonce);
    qint64 startSession();
    bool finishLiveSessions();
    bool saveObservation(qint64 sessionId, const QString &source, const QString &payload);
    QVariantList completedSessions() const;
    QVariantList observations(qint64 sessionId) const;

private:
    QString m_connectionName;
    QSqlDatabase m_db;
    bool execSchema(QString *error);
};
