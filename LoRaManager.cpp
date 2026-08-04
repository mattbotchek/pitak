#include "LoRaManager.h"
#include "PiSatPage.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <algorithm>

QString LoRaManager::environment(const char *name, const QString &fallback)
{
    const QString value = QString::fromUtf8(qgetenv(name)).trimmed();
    return value.isEmpty() ? fallback : value;
}

bool LoRaManager::isUniqueNodeId(const QString &nodeId)
{
    static const QStringList genericIds = {
        QStringLiteral("pitak"), QStringLiteral("pisat"), QStringLiteral("node"),
        QStringLiteral("device"), QStringLiteral("default"), QStringLiteral("test"),
        QStringLiteral("localhost"), QStringLiteral("unknown")
    };
    return SecureProtocol::validIdentifier(nodeId) &&
           !genericIds.contains(nodeId, Qt::CaseInsensitive);
}

LoRaManager::LoRaManager(QObject *parent)
    : QObject(parent),
      m_networkProtocol(qgetenv("PITAK_NETWORK_KEY")),
      m_pisatProtocol(qgetenv("PITAK_PISAT_KEY")),
      m_nodeId(environment("PITAK_NODE_ID")),
      m_name(environment("PITAK_NAME", QStringLiteral("PiTAK"))),
      m_pisatNode(environment("PITAK_PISAT_NODE")),
      m_frequency(environment("PITAK_LORA_FREQUENCY", QStringLiteral("915000000"))),
      m_channel(environment("PITAK_LORA_CHANNEL", QStringLiteral("0"))),
      m_network(environment("PITAK_LORA_NETWORK", QStringLiteral("default"))),
      m_address(environment("PITAK_LORA_ADDRESS", m_nodeId))
{
    const QByteArray networkKey = qgetenv("PITAK_NETWORK_KEY");
    const QByteArray pisatKey = qgetenv("PITAK_PISAT_KEY");
    m_keysSeparated = networkKey.isEmpty() || pisatKey.isEmpty() || networkKey != pisatKey;
    m_identityReady = isUniqueNodeId(m_nodeId) && isUniqueNodeId(m_pisatNode) &&
                      m_nodeId.compare(m_pisatNode, Qt::CaseInsensitive) != 0;
    if (!m_identityReady) {
        m_securityStatus =
            QStringLiteral("PITAK_NODE_ID and PITAK_PISAT_NODE are required, distinct, non-generic IDs. "
                           "PITAK_NODE_ID must be globally unique among every device sharing either configured key.");
    } else if (!m_keysSeparated) {
        m_securityStatus =
            QStringLiteral("PITAK_NETWORK_KEY and PITAK_PISAT_KEY must be different");
    } else if (!m_networkProtocol.isConfigured() && !m_pisatProtocol.isConfigured()) {
        m_securityStatus =
            QStringLiteral("Peer messaging disabled: PITAK_NETWORK_KEY is required; "
                           "PiSAT control disabled: PITAK_PISAT_KEY is required");
    } else if (!m_networkProtocol.isConfigured()) {
        m_securityStatus =
            QStringLiteral("Peer messaging disabled: PITAK_NETWORK_KEY is required; PiSAT control key configured");
    } else if (!m_pisatProtocol.isConfigured()) {
        m_securityStatus =
            QStringLiteral("Peer messaging key configured; PiSAT control disabled: PITAK_PISAT_KEY is required");
    } else {
        m_securityStatus = QStringLiteral("Peer messaging and PiSAT control encryption enabled with separate keys");
    }

    QString dataPath = environment("PITAK_DATA_PATH");
    if (dataPath.isEmpty())
        dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFileInfo dataInfo(dataPath);
    const QString databasePath = (dataInfo.suffix().compare(QStringLiteral("sqlite"), Qt::CaseInsensitive) == 0 ||
                                  dataInfo.suffix().compare(QStringLiteral("db"), Qt::CaseInsensitive) == 0)
        ? dataPath : QDir(dataPath).filePath(QStringLiteral("pitak.sqlite"));
    QString databaseError;
    m_storeReady = m_store.open(databasePath, &databaseError);
    if (!m_storeReady)
        m_securityStatus += QStringLiteral("; SQLite error: ") + databaseError;
    else if (!m_store.finishLiveSessions()) {
        m_storeReady = false;
        m_securityStatus += QStringLiteral("; SQLite error: unable to close prior live sessions");
    }

    m_radio = new SX126X("/dev/ttyS0", 9600);
    if (!m_radio->isReady())
        m_securityStatus += QStringLiteral("; LoRa radio unavailable");

    m_rxTimer.setInterval(20);
    connect(&m_rxTimer, &QTimer::timeout, this, &LoRaManager::pollRadio);
    m_rxTimer.start();

    m_heartbeatTimer.setInterval(10000);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &LoRaManager::sendHeartbeat);
    m_heartbeatTimer.start();

    m_peerTimer.setInterval(5000);
    connect(&m_peerTimer, &QTimer::timeout, this, [this]() {
        const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - 35000;
        bool changed = false;
        for (auto it = m_peerMap.begin(); it != m_peerMap.end();) {
            if (it->lastSeenMs < cutoff) {
                if (m_selectedPeer == it.key())
                    setSelectedPeer(QString());
                it = m_peerMap.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
        if (changed)
            rebuildPeers();
    });
    m_peerTimer.start();

    m_telemetryInactivityTimer.setInterval(75000);
    m_telemetryInactivityTimer.setSingleShot(true);
    connect(&m_telemetryInactivityTimer, &QTimer::timeout,
            this, &LoRaManager::finishTelemetrySession);

    refreshSessions();
    sendHeartbeat();
}

LoRaManager::~LoRaManager()
{
    finishTelemetrySession();
    delete m_radio;
}

QString LoRaManager::securityStatus() const { return m_securityStatus; }
QString LoRaManager::pisatStatus() const { return m_pisatStatus; }
QVariantList LoRaManager::messages() const { return m_messages; }
QVariantList LoRaManager::telemetryRecords() const { return m_telemetryRecords; }
QVariantList LoRaManager::sessions() const { return m_sessions; }
QVariantList LoRaManager::sessionObservations() const { return m_sessionObservations; }
QString LoRaManager::selectedPeer() const { return m_selectedPeer; }

QVariantList LoRaManager::peers() const
{
    QVariantList result;
    QStringList peerIds = m_peerMap.keys();
    std::sort(peerIds.begin(), peerIds.end());
    for (const QString &peerId : peerIds)
        result.append(m_peerMap.value(peerId).values);
    return result;
}

void LoRaManager::pollRadio()
{
    if (!m_radio || !m_radio->available())
        return;
    uint8_t buffer[512];
    const int count = m_radio->receive(buffer, sizeof(buffer));
    if (count <= 0)
        return;
    m_rxBuffer.append(reinterpret_cast<const char *>(buffer), count);
    int newline = -1;
    while ((newline = m_rxBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_rxBuffer.left(newline);
        m_rxBuffer.remove(0, newline + 1);
        if (!line.isEmpty())
            processLine(line);
    }
    if (m_rxBuffer.size() > 4096)
        m_rxBuffer.clear();
}

void LoRaManager::processLine(const QByteArray &line)
{
    if (!m_identityReady || !m_keysSeparated) {
        setStatusError(!m_identityReady
                       ? QStringLiteral("Rejected frame: globally unique PITAK_NODE_ID and explicit PITAK_PISAT_NODE are required")
                       : QStringLiteral("Rejected frame: PITAK_NETWORK_KEY and PITAK_PISAT_KEY must be different"));
        return;
    }
    if (line.size() + 1 > 255) {
        setStatusError(QStringLiteral("Rejected frame exceeding the 255-byte LoRa limit"));
        return;
    }
    SecureFrame frame;
    QString error;
    const QByteArrayList header = line.split('|');
    const bool claimsPiSatSource =
        header.size() == 7 && QString::fromUtf8(header[1]) == m_pisatNode;
    const SecureProtocol &protocol = claimsPiSatSource ? m_pisatProtocol : m_networkProtocol;
    if (!protocol.decode(line, &frame, &error)) {
        setStatusError(claimsPiSatSource
                       ? QStringLiteral("Rejected PiSAT frame: ") + error +
                             QStringLiteral(" (PITAK_PISAT_KEY)")
                       : QStringLiteral("Rejected peer frame: ") + error +
                             QStringLiteral(" (PITAK_NETWORK_KEY)"));
        return;
    }
    if (frame.dst != m_nodeId && frame.dst != QStringLiteral("*"))
        return;
    if (!m_storeReady) {
        setStatusError(QStringLiteral("Rejected frame: SQLite nonce store is unavailable"));
        return;
    }
    if (!m_store.acceptInboundNonce(frame.src, frame.nonce)) {
        setStatusError(QStringLiteral("Rejected replay from ") + frame.src);
        return;
    }
    handleFrame(frame);
}

void LoRaManager::handleFrame(const SecureFrame &frame)
{
    if (frame.type == QStringLiteral("HEARTBEAT") && frame.dst == QStringLiteral("*")) {
        handleHeartbeat(frame);
    } else if (frame.type == QStringLiteral("MESSAGE") && frame.dst == m_nodeId) {
        const QString text = SecureProtocol::sanitizeMessage(QString::fromUtf8(frame.payload));
        if (!text.isEmpty()) {
            const QString peerName = m_peerMap.contains(frame.src)
                ? m_peerMap.value(frame.src).values.value(QStringLiteral("name")).toString() : frame.src;
            m_messages.append(QVariantMap{{QStringLiteral("direction"), QStringLiteral("received")},
                                          {QStringLiteral("peerId"), frame.src},
                                          {QStringLiteral("name"), peerName},
                                          {QStringLiteral("text"), text},
                                          {QStringLiteral("time"), QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))}});
            emit messagesChanged();
        }
    } else if (frame.src == m_pisatNode &&
               (frame.type == QStringLiteral("OK") || frame.type == QStringLiteral("ERR"))) {
        handleResponse(frame);
    } else if (frame.src == m_pisatNode && frame.type == QStringLiteral("EVENT")) {
        m_pisatStatus = QStringLiteral("Authenticated EVENT: ") + QString::fromUtf8(frame.payload);
        recordTelemetry(frame.src, QStringLiteral("EVENT,") + QString::fromUtf8(frame.payload));
        emit statusChanged();
    }
}

void LoRaManager::handleHeartbeat(const SecureFrame &frame)
{
    if (frame.src == m_nodeId)
        return;
    QVariantMap values;
    values.insert(QStringLiteral("id"), frame.src);
    static const QStringList allowedFields = {
        QStringLiteral("name"), QStringLiteral("freq"), QStringLiteral("channel"),
        QStringLiteral("network"), QStringLiteral("address"), QStringLiteral("connect"),
        QStringLiteral("capabilities")
    };
    const QStringList fields = QString::fromUtf8(frame.payload).split(';', Qt::SkipEmptyParts);
    for (const QString &field : fields) {
        const int separator = field.indexOf('=');
        const QString key = separator > 0 ? field.left(separator) : QString();
        if (allowedFields.contains(key))
            values.insert(key, SecureProtocol::percentDecode(field.mid(separator + 1)));
    }
    values.insert(QStringLiteral("lastSeen"), QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
    values.insert(QStringLiteral("compatible"), peerCompatible(values));
    m_peerMap.insert(frame.src, Peer{values, QDateTime::currentMSecsSinceEpoch()});
    rebuildPeers();
}

void LoRaManager::handleResponse(const SecureFrame &frame)
{
    const QString payload = QString::fromUtf8(frame.payload);
    const int comma = payload.indexOf(',');
    const QString nonceText = comma < 0 ? payload : payload.left(comma);
    bool ok = false;
    const quint64 requestNonce = nonceText.toULongLong(&ok);
    if (!ok || !m_pendingRequests.contains(requestNonce)) {
        setStatusError(QStringLiteral("PiSAT response has unknown request nonce"));
        return;
    }
    const QString requestType = m_pendingRequests.take(requestNonce);
    if (requestType == QStringLiteral("DEVICES"))
        m_devicesPending = false;
    const QString response = comma < 0 ? QString() : payload.mid(comma + 1);
    if (frame.type == QStringLiteral("ERR")) {
        m_pisatStatus = QStringLiteral("%1 error: %2").arg(requestType, response);
        emit statusChanged();
        return;
    }
    if (requestType == QStringLiteral("STATUS") || requestType == QStringLiteral("MONITOR") ||
        requestType == QStringLiteral("CAPTURE")) {
        m_pisatStatus = QStringLiteral("Authenticated %1: %2").arg(requestType, response);
        emit statusChanged();
    }
    if (requestType == QStringLiteral("DEVICES")) {
        PiSatDevicePage page;
        if (!parsePiSatDevicePage(response, &page)) {
            setStatusError(QStringLiteral("Malformed DEVICES response page"));
            return;
        }
        if (m_restartDevicesPending) {
            m_deviceOffset = 0;
            m_restartDevicesPending = false;
        } else {
            m_deviceOffset = page.nextOffset;
        }
        if (!page.record.isEmpty())
            recordTelemetry(frame.src, page.record);
    }
}

QString LoRaManager::heartbeatPayload() const
{
    const auto item = [](const QString &key, const QString &value) {
        return key + QLatin1Char('=') + SecureProtocol::percentEncode(value);
    };
    return QStringList{item(QStringLiteral("name"), m_name),
                       item(QStringLiteral("freq"), m_frequency),
                       item(QStringLiteral("channel"), m_channel),
                       item(QStringLiteral("network"), m_network),
                       item(QStringLiteral("address"), m_address),
                       item(QStringLiteral("connect"), m_network + QLatin1Char(':') + m_address),
                       item(QStringLiteral("capabilities"), QStringLiteral("MSG"))}.join(';');
}

void LoRaManager::sendHeartbeat()
{
    sendFrame(QStringLiteral("*"), QStringLiteral("HEARTBEAT"), heartbeatPayload().toUtf8());
}

bool LoRaManager::sendFrame(const QString &dst, const QString &type, const QByteArray &payload,
                            quint64 *sentNonce, QByteArray *sentPayload)
{
    const bool peerTraffic =
        type == QStringLiteral("HEARTBEAT") || type == QStringLiteral("MESSAGE");
    const bool controlTraffic =
        type == QStringLiteral("STATUS") || type == QStringLiteral("DEVICES") ||
        type == QStringLiteral("MONITOR") || type == QStringLiteral("CAPTURE");
    if (!peerTraffic && !controlTraffic) {
        setStatusError(QStringLiteral("Unsupported outbound secure frame type"));
        return false;
    }
    const SecureProtocol &protocol = peerTraffic ? m_networkProtocol : m_pisatProtocol;
    if (!m_identityReady || !m_keysSeparated || !protocol.isConfigured() || !m_storeReady ||
        !m_radio || !m_radio->isReady()) {
        setStatusError(!m_identityReady
                       ? QStringLiteral("Secure traffic disabled: configure globally unique PITAK_NODE_ID and explicit PITAK_PISAT_NODE")
                       : !m_keysSeparated
                           ? QStringLiteral("PITAK_NETWORK_KEY and PITAK_PISAT_KEY must be different")
                       : !protocol.isConfigured()
                           ? (peerTraffic
                              ? (m_pisatProtocol.isConfigured()
                                 ? QStringLiteral("Peer messaging disabled: PITAK_NETWORK_KEY is required; PiSAT control key configured")
                                 : QStringLiteral("Peer messaging disabled: PITAK_NETWORK_KEY is required; PiSAT control disabled: PITAK_PISAT_KEY is required"))
                              : (m_networkProtocol.isConfigured()
                                 ? QStringLiteral("PiSAT control disabled: PITAK_PISAT_KEY is required; peer messaging key configured")
                                 : QStringLiteral("PiSAT control disabled: PITAK_PISAT_KEY is required; peer messaging disabled: PITAK_NETWORK_KEY is required")))
                       : !m_storeReady ? QStringLiteral("SQLite nonce store is unavailable")
                                       : QStringLiteral("LoRa radio is unavailable"));
        return false;
    }
    const quint64 nonce = m_store.nextOutboundNonce();
    if (!nonce) {
        setStatusError(QStringLiteral("Unable to persist outbound nonce"));
        return false;
    }
    QString error;
    QByteArray fittedPayload = payload;
    QByteArray encoded;
    if (type == QStringLiteral("MESSAGE")) {
        QString fittedText;
        encoded = protocol.encodeFittingMessage(m_nodeId, dst, nonce,
                                                QString::fromUtf8(payload), 255,
                                                &fittedText, &error);
        fittedPayload = fittedText.toUtf8();
    } else {
        encoded = protocol.encode(m_nodeId, dst, nonce, type, fittedPayload, &error);
    }
    if (encoded.isEmpty()) {
        setStatusError(error.isEmpty() ? QStringLiteral("LoRa transmit failed") : error);
        return false;
    }
    if (encoded.size() > 255) {
        setStatusError(QStringLiteral("%1 frame exceeds the 255-byte LoRa limit").arg(type));
        return false;
    }
    if (!m_radio->send(encoded.toStdString())) {
        setStatusError(QStringLiteral("LoRa transmit failed"));
        return false;
    }
    if (sentNonce)
        *sentNonce = nonce;
    if (sentPayload)
        *sentPayload = fittedPayload;
    return true;
}

void LoRaManager::sendRequest(const QString &type, const QString &payload)
{
    quint64 nonce = 0;
    if (sendFrame(m_pisatNode, type, payload.toUtf8(), &nonce)) {
        m_pendingRequests.insert(nonce, type);
        QTimer::singleShot(type == QStringLiteral("DEVICES") ? 10000 : 30000, this, [this, nonce, type]() {
            if (m_pendingRequests.remove(nonce) && type == QStringLiteral("DEVICES"))
                m_devicesPending = false;
        });
    }
}

void LoRaManager::requestStatus() { sendRequest(QStringLiteral("STATUS"), {}); }

void LoRaManager::requestDevices(bool restart)
{
    if (restart) {
        m_deviceOffset = 0;
        if (m_devicesPending) {
            m_restartDevicesPending = true;
            return;
        }
    }
    if (m_devicesPending)
        return;
    m_devicesPending = true;
    sendRequest(QStringLiteral("DEVICES"), QStringLiteral("%1,1").arg(m_deviceOffset));
    if (!m_pendingRequests.values().contains(QStringLiteral("DEVICES")))
        m_devicesPending = false;
}

void LoRaManager::setMonitor(bool enabled, const QString &channel)
{
    if (enabled) {
        const QString cleanChannel = channel.trimmed();
        if (cleanChannel.isEmpty()) {
            setStatusError(QStringLiteral("Monitor channel is required"));
            return;
        }
        sendRequest(QStringLiteral("MONITOR"), QStringLiteral("ON,%1").arg(cleanChannel));
    } else {
        sendRequest(QStringLiteral("MONITOR"), QStringLiteral("OFF"));
        finishTelemetrySession();
    }
}

void LoRaManager::startCapture(const QString &tool)
{
    const QString normalized = tool.trimmed().toLower();
    if (normalized != QStringLiteral("tcpdump") && normalized != QStringLiteral("airodump") &&
        normalized != QStringLiteral("wireshark")) {
        setStatusError(QStringLiteral("Unsupported capture tool"));
        return;
    }
    sendRequest(QStringLiteral("CAPTURE"), QStringLiteral("START,%1").arg(normalized));
}

void LoRaManager::stopCapture()
{
    sendRequest(QStringLiteral("CAPTURE"), QStringLiteral("STOP"));
    finishTelemetrySession();
}

void LoRaManager::sendPeerMessage(const QString &message)
{
    const QString text = SecureProtocol::sanitizeMessage(message);
    if (text.isEmpty() || m_selectedPeer.isEmpty() || !m_peerMap.contains(m_selectedPeer) ||
        !m_peerMap.value(m_selectedPeer).values.value(QStringLiteral("compatible")).toBool())
        return;
    QByteArray sentPayload;
    if (sendFrame(m_selectedPeer, QStringLiteral("MESSAGE"), text.toUtf8(), nullptr, &sentPayload)) {
        const QString sentText = QString::fromUtf8(sentPayload);
        const QString peerName = m_peerMap.value(m_selectedPeer).values.value(QStringLiteral("name")).toString();
        m_messages.append(QVariantMap{{QStringLiteral("direction"), QStringLiteral("sent")},
                                      {QStringLiteral("peerId"), m_selectedPeer},
                                      {QStringLiteral("name"), peerName},
                                      {QStringLiteral("text"), sentText},
                                      {QStringLiteral("time"), QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))}});
        emit messagesChanged();
    }
}

bool LoRaManager::peerCompatible(const QVariantMap &peer) const
{
    return peer.value(QStringLiteral("capabilities")).toString().split(',').contains(QStringLiteral("MSG")) &&
           peer.value(QStringLiteral("freq")).toString() == m_frequency &&
           peer.value(QStringLiteral("channel")).toString() == m_channel &&
           peer.value(QStringLiteral("network")).toString() == m_network;
}

void LoRaManager::rebuildPeers()
{
    emit peersChanged();
}

void LoRaManager::recordTelemetry(const QString &source, const QString &payload)
{
    if (!m_storeReady || payload.isEmpty()) {
        if (!m_storeReady)
            setStatusError(QStringLiteral("Unable to persist telemetry: SQLite is unavailable"));
        return;
    }
    if (!m_liveSessionId) {
        m_liveSessionId = m_store.startSession();
        if (!m_liveSessionId) {
            setStatusError(QStringLiteral("Unable to create telemetry session"));
            return;
        }
        m_telemetryInactivityTimer.start();
    }
    if (!m_store.saveObservation(m_liveSessionId, source, payload)) {
        setStatusError(QStringLiteral("Unable to persist telemetry observation"));
        return;
    }
    m_telemetryRecords.prepend(QVariantMap{{QStringLiteral("time"), QDateTime::currentDateTime().toString(Qt::ISODate)},
                                           {QStringLiteral("source"), source},
                                           {QStringLiteral("payload"), payload}});
    while (m_telemetryRecords.size() > 200)
        m_telemetryRecords.removeLast();
    m_telemetryInactivityTimer.start();
    emit telemetryChanged();
}

void LoRaManager::setSelectedPeer(const QString &peerId)
{
    if (m_selectedPeer == peerId)
        return;
    m_selectedPeer = peerId;
    emit selectedPeerChanged();
}

void LoRaManager::refreshSessions()
{
    if (!m_storeReady)
        return;
    m_sessions = m_store.completedSessions();
    emit sessionsChanged();
}

void LoRaManager::selectSession(qint64 sessionId)
{
    if (!m_storeReady)
        return;
    m_sessionObservations = m_store.observations(sessionId);
    emit sessionObservationsChanged();
}

void LoRaManager::finishTelemetrySession()
{
    if (!m_storeReady || !m_liveSessionId)
        return;
    m_telemetryInactivityTimer.stop();
    if (!m_store.finishLiveSessions()) {
        setStatusError(QStringLiteral("Unable to finish telemetry session"));
        m_telemetryInactivityTimer.start();
        return;
    }
    m_liveSessionId = 0;
    refreshSessions();
}

void LoRaManager::setStatusError(const QString &message)
{
    m_securityStatus = message;
    emit statusChanged();
}
