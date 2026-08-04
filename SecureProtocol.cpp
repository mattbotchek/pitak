#include "SecureProtocol.h"

#include <QCryptographicHash>
#include <QRegularExpression>
#include <QUrl>
#include <openssl/evp.h>

namespace {
void setError(QString *error, const QString &value)
{
    if (error)
        *error = value;
}
}

SecureProtocol::SecureProtocol(const QByteArray &sharedSecret)
{
    if (!sharedSecret.isEmpty())
        m_key = QCryptographicHash::hash(sharedSecret, QCryptographicHash::Sha256);
}

bool SecureProtocol::isConfigured() const
{
    return m_key.size() == 32;
}

bool SecureProtocol::validIdentifier(const QString &value, bool allowBroadcast)
{
    if (allowBroadcast && value == QStringLiteral("*"))
        return true;
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_.:-]{1,32}$"));
    return pattern.match(value).hasMatch();
}

QByteArray SecureProtocol::ivFor(const QString &src, quint64 nonce)
{
    QByteArray iv = QCryptographicHash::hash(src.toUtf8(), QCryptographicHash::Sha256).left(4);
    for (int shift = 56; shift >= 0; shift -= 8)
        iv.append(char((nonce >> shift) & 0xff));
    return iv;
}

QByteArray SecureProtocol::encode(const QString &src, const QString &dst, quint64 nonce,
                                  const QString &type, const QByteArray &payload,
                                  QString *error) const
{
    if (!isConfigured()) {
        setError(error, QStringLiteral("PITAK_NETWORK_KEY is not configured"));
        return {};
    }
    if (nonce == 0) {
        setError(error, QStringLiteral("Nonce must be nonzero"));
        return {};
    }
    if (!validIdentifier(src) || !validIdentifier(dst, true) || !validIdentifier(type)) {
        setError(error, QStringLiteral("Invalid protocol identifier"));
        return {};
    }

    const QByteArray nonceText = QByteArray::number(nonce);
    const QByteArray aad = "P2|" + src.toUtf8() + "|" + dst.toUtf8() + "|" + nonceText + "|" + type.toUtf8();
    const QByteArray iv = ivFor(src, nonce);
    QByteArray ciphertext(payload.size(), '\0');
    QByteArray tag(16, '\0');
    int outputLength = 0;
    int finalLength = 0;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        setError(error, QStringLiteral("Unable to allocate encryption context"));
        return {};
    }
    const bool ok =
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) == 1 &&
        EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char *>(m_key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData())) == 1 &&
        EVP_EncryptUpdate(ctx, nullptr, &outputLength,
                          reinterpret_cast<const unsigned char *>(aad.constData()), aad.size()) == 1 &&
        EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(ciphertext.data()), &outputLength,
                          reinterpret_cast<const unsigned char *>(payload.constData()), payload.size()) == 1 &&
        EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(ciphertext.data()) + outputLength,
                            &finalLength) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        setError(error, QStringLiteral("AES-GCM encryption failed"));
        return {};
    }
    ciphertext.resize(outputLength + finalLength);
    return aad + "|" + ciphertext.toBase64() + "|" + tag.toBase64() + "\n";
}

QByteArray SecureProtocol::encodeFittingMessage(const QString &src, const QString &dst,
                                                quint64 nonce, const QString &message,
                                                int maxFrameBytes, QString *fittedMessage,
                                                QString *error) const
{
    QString fitted = sanitizeMessage(message);
    QByteArray encoded = encode(src, dst, nonce, QStringLiteral("MESSAGE"), fitted.toUtf8(), error);
    while (encoded.size() > maxFrameBytes && !fitted.isEmpty()) {
        fitted.chop(1);
        if (!fitted.isEmpty() && fitted.back().isHighSurrogate())
            fitted.chop(1);
        encoded = encode(src, dst, nonce, QStringLiteral("MESSAGE"), fitted.toUtf8(), error);
    }
    if (fitted.isEmpty()) {
        setError(error, QStringLiteral("MESSAGE frame has no room for text"));
        return {};
    }
    if (fittedMessage)
        *fittedMessage = fitted;
    return encoded;
}

bool SecureProtocol::decode(const QByteArray &line, SecureFrame *frame, QString *error) const
{
    if (!isConfigured() || !frame) {
        setError(error, QStringLiteral("Secure protocol is not configured"));
        return false;
    }
    QByteArray clean = line;
    while (clean.endsWith('\r') || clean.endsWith('\n'))
        clean.chop(1);
    const QByteArrayList parts = clean.split('|');
    if (parts.size() != 7 || parts[0] != "P2") {
        setError(error, QStringLiteral("Malformed secure frame"));
        return false;
    }

    bool nonceOk = false;
    const quint64 nonce = parts[3].toULongLong(&nonceOk);
    const QString src = QString::fromUtf8(parts[1]);
    const QString dst = QString::fromUtf8(parts[2]);
    const QString type = QString::fromUtf8(parts[4]);
    if (!nonceOk || nonce == 0 || !validIdentifier(src) ||
        !validIdentifier(dst, true) || !validIdentifier(type)) {
        setError(error, QStringLiteral("Invalid secure frame header"));
        return false;
    }
    const QByteArray ciphertext = QByteArray::fromBase64(parts[5], QByteArray::AbortOnBase64DecodingErrors);
    const QByteArray tag = QByteArray::fromBase64(parts[6], QByteArray::AbortOnBase64DecodingErrors);
    if (tag.size() != 16 || (ciphertext.isEmpty() && !parts[5].isEmpty())) {
        setError(error, QStringLiteral("Invalid secure frame encoding"));
        return false;
    }

    const QByteArray aad = "P2|" + parts[1] + "|" + parts[2] + "|" + parts[3] + "|" + parts[4];
    const QByteArray iv = ivFor(src, nonce);
    QByteArray plaintext(ciphertext.size(), '\0');
    int outputLength = 0;
    int finalLength = 0;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return false;
    const bool ok =
        EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) == 1 &&
        EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char *>(m_key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData())) == 1 &&
        EVP_DecryptUpdate(ctx, nullptr, &outputLength,
                          reinterpret_cast<const unsigned char *>(aad.constData()), aad.size()) == 1 &&
        EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char *>(plaintext.data()), &outputLength,
                          reinterpret_cast<const unsigned char *>(ciphertext.constData()), ciphertext.size()) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(),
                            const_cast<char *>(tag.constData())) == 1 &&
        EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(plaintext.data()) + outputLength,
                            &finalLength) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        setError(error, QStringLiteral("Authentication failed"));
        return false;
    }
    plaintext.resize(outputLength + finalLength);
    *frame = {src, dst, nonce, type, plaintext};
    return true;
}

QString SecureProtocol::sanitizeMessage(const QString &message, int maxLength)
{
    QString result = message;
    result.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    result.remove(QRegularExpression(QStringLiteral("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F\\x7F-\\x9F]")));
    result = result.trimmed();
    if (result.size() > maxLength)
        result.truncate(maxLength);
    return result;
}

QString SecureProtocol::percentEncode(const QString &value)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(value, QByteArray("-_.~")));
}

QString SecureProtocol::percentDecode(const QString &value)
{
    return QUrl::fromPercentEncoding(value.toLatin1());
}
