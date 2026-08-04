#pragma once

#include "PiTakStore.h"
#include "SecureProtocol.h"
#include "sx126x.h"

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class LoRaManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString securityStatus READ securityStatus NOTIFY statusChanged)
    Q_PROPERTY(QString pisatStatus READ pisatStatus NOTIFY statusChanged)
    Q_PROPERTY(QVariantList peers READ peers NOTIFY peersChanged)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(QVariantList telemetryRecords READ telemetryRecords NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY sessionsChanged)
    Q_PROPERTY(QVariantList sessionObservations READ sessionObservations NOTIFY sessionObservationsChanged)
    Q_PROPERTY(QString selectedPeer READ selectedPeer WRITE setSelectedPeer NOTIFY selectedPeerChanged)

public:
    explicit LoRaManager(QObject *parent = nullptr);
    ~LoRaManager() override;

    QString securityStatus() const;
    QString pisatStatus() const;
    QVariantList peers() const;
    QVariantList messages() const;
    QVariantList telemetryRecords() const;
    QVariantList sessions() const;
    QVariantList sessionObservations() const;
    QString selectedPeer() const;

    Q_INVOKABLE void requestStatus();
    Q_INVOKABLE void requestDevices(bool restart = false);
    Q_INVOKABLE void setMonitor(bool enabled, const QString &channel);
    Q_INVOKABLE void startCapture(const QString &tool);
    Q_INVOKABLE void stopCapture();
    Q_INVOKABLE void sendPeerMessage(const QString &message);
    Q_INVOKABLE void refreshSessions();
    Q_INVOKABLE void selectSession(qint64 sessionId);
    Q_INVOKABLE void setSelectedPeer(const QString &peerId);

signals:
    void statusChanged();
    void peersChanged();
    void messagesChanged();
    void telemetryChanged();
    void sessionsChanged();
    void sessionObservationsChanged();
    void selectedPeerChanged();

private:
    struct Peer {
        QVariantMap values;
        qint64 lastSeenMs = 0;
    };

    SX126X *m_radio = nullptr;
    QTimer m_rxTimer;
    QTimer m_heartbeatTimer;
    QTimer m_peerTimer;
    QTimer m_telemetryInactivityTimer;
    QByteArray m_rxBuffer;
    SecureProtocol m_networkProtocol;
    SecureProtocol m_pisatProtocol;
    PiTakStore m_store;
    QString m_nodeId;
    QString m_name;
    QString m_pisatNode;
    QString m_frequency;
    QString m_channel;
    QString m_network;
    QString m_address;
    QString m_securityStatus;
    QString m_pisatStatus = QStringLiteral("No authenticated PiSAT status received");
    QString m_selectedPeer;
    QHash<QString, Peer> m_peerMap;
    QHash<quint64, QString> m_pendingRequests;
    QVariantList m_messages;
    QVariantList m_telemetryRecords;
    QVariantList m_sessions;
    QVariantList m_sessionObservations;
    bool m_identityReady = false;
    bool m_keysSeparated = false;
    bool m_storeReady = false;
    bool m_devicesPending = false;
    bool m_restartDevicesPending = false;
    qint64 m_liveSessionId = 0;
    int m_deviceOffset = 0;

    void pollRadio();
    void processLine(const QByteArray &line);
    void handleFrame(const SecureFrame &frame);
    void handleHeartbeat(const SecureFrame &frame);
    void handleResponse(const SecureFrame &frame);
    void sendHeartbeat();
    bool sendFrame(const QString &dst, const QString &type, const QByteArray &payload,
                   quint64 *sentNonce = nullptr, QByteArray *sentPayload = nullptr);
    void sendRequest(const QString &type, const QString &payload);
    void setStatusError(const QString &message);
    QString heartbeatPayload() const;
    bool peerCompatible(const QVariantMap &peer) const;
    void rebuildPeers();
    void recordTelemetry(const QString &source, const QString &payload);
    void finishTelemetrySession();
    static bool isUniqueNodeId(const QString &nodeId);
    static QString environment(const char *name, const QString &fallback = {});
};
