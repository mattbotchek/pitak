#pragma once

#include <QByteArray>
#include <QString>

struct SecureFrame {
    QString src;
    QString dst;
    quint64 nonce = 0;
    QString type;
    QByteArray payload;
};

class SecureProtocol {
public:
    // Wire ciphertext and tag use standard padded RFC 4648 Base64, never Base64URL.
    explicit SecureProtocol(const QByteArray &sharedSecret = {});

    bool isConfigured() const;
    QByteArray encode(const QString &src, const QString &dst, quint64 nonce,
                      const QString &type, const QByteArray &payload,
                      QString *error = nullptr) const;
    QByteArray encodeFittingMessage(const QString &src, const QString &dst, quint64 nonce,
                                    const QString &message, int maxFrameBytes,
                                    QString *fittedMessage = nullptr,
                                    QString *error = nullptr) const;
    bool decode(const QByteArray &line, SecureFrame *frame, QString *error = nullptr) const;

    static bool validIdentifier(const QString &value, bool allowBroadcast = false);
    static QString sanitizeMessage(const QString &message, int maxLength = 160);
    static QString percentEncode(const QString &value);
    static QString percentDecode(const QString &value);

private:
    QByteArray m_key;
    static QByteArray ivFor(const QString &src, quint64 nonce);
};
