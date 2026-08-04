#include "SecureProtocol.h"
#include "PiSatPage.h"

#include <QTest>

class SecureProtocolTests : public QObject {
    Q_OBJECT

private slots:
    void roundTrip()
    {
        SecureProtocol protocol("shared secret");
        const QByteArray encoded = protocol.encode("pitak", "pisat", 42, "STATUS", {});
        QCOMPARE(encoded, QByteArray("P2|pitak|pisat|42|STATUS||XNF8E2Iu6lIi/3AD5cUEjQ==\n"));
        QVERIFY(encoded.contains('/'));
        QVERIFY(encoded.endsWith("==\n"));

        SecureFrame frame;
        QVERIFY(protocol.decode(encoded, &frame));
        QCOMPARE(frame.src, QString("pitak"));
        QCOMPARE(frame.dst, QString("pisat"));
        QCOMPARE(frame.nonce, quint64(42));
        QCOMPARE(frame.type, QString("STATUS"));
        QVERIFY(frame.payload.isEmpty());
    }

    void rejectsTampering()
    {
        SecureProtocol protocol("shared secret");
        QByteArray encoded = protocol.encode("pitak", "pisat", 43, "MESSAGE", "hello");
        encoded.replace("hello", "other");
        encoded[encoded.size() - 3] ^= 1;
        SecureFrame frame;
        QVERIFY(!protocol.decode(encoded, &frame));
    }

    void separatesCredentialDomains()
    {
        SecureProtocol networkProtocol("peer network key");
        SecureProtocol pisatProtocol("PiSAT control key");
        const QByteArray controlFrame =
            pisatProtocol.encode("pitak-alpha", "pisat-alpha", 45, "STATUS", {});

        SecureFrame frame;
        QVERIFY(pisatProtocol.decode(controlFrame, &frame));
        QVERIFY(!networkProtocol.decode(controlFrame, &frame));

        const QByteArray peerFrame =
            networkProtocol.encode("pitak-alpha", "*", 46, "HEARTBEAT", "capabilities=MSG");
        QVERIFY(networkProtocol.decode(peerFrame, &frame));
        QVERIFY(!pisatProtocol.decode(peerFrame, &frame));
    }

    void rejectsZeroNonce()
    {
        SecureProtocol protocol("shared secret");
        QString error;
        QVERIFY(protocol.encode("pitak", "pisat", 0, "STATUS", {}, &error).isEmpty());
        QVERIFY(error.contains("nonzero"));

        SecureFrame frame;
        QVERIFY(!protocol.decode("P2|pitak|pisat|0|STATUS||AAAAAAAAAAAAAAAAAAAAAA==", &frame, &error));
    }

    void fitsMessageToRadioFrame()
    {
        SecureProtocol protocol("shared secret");
        const QString original(160, QChar('x'));
        QString fitted;
        const QByteArray encoded =
            protocol.encodeFittingMessage("pitak", "peer-with-a-long-id", 44,
                                          original, 255, &fitted);
        QVERIFY(!encoded.isEmpty());
        QVERIFY(encoded.size() <= 255);
        QVERIFY(original.startsWith(fitted));

        SecureFrame frame;
        QVERIFY(protocol.decode(encoded, &frame));
        QCOMPARE(QString::fromUtf8(frame.payload), fitted);
    }

    void sanitizesMessages()
    {
        QCOMPARE(SecureProtocol::sanitizeMessage(QString(" hello\t\nworld\1 ")),
                 QString("hello world"));
        QCOMPARE(SecureProtocol::sanitizeMessage(QString(200, QChar('x'))).size(), 160);
    }

    void validatesMetadata()
    {
        QVERIFY(SecureProtocol::validIdentifier("node-1.example"));
        QVERIFY(!SecureProtocol::validIdentifier("bad|node"));
        const QString value = QString("field=value;line\n2");
        QCOMPARE(SecureProtocol::percentDecode(SecureProtocol::percentEncode(value)), value);
    }

    void parsesDevicePages()
    {
        PiSatDevicePage page;
        QVERIFY(parsePiSatDevicePage("0,2;A,AA:BB,-45,6,mesh", &page));
        QCOMPARE(page.offset, 0);
        QCOMPARE(page.total, 2);
        QCOMPARE(page.nextOffset, 1);
        QCOMPARE(page.record, QString("A,AA:BB,-45,6,mesh"));

        QVERIFY(parsePiSatDevicePage("1,2", &page));
        QCOMPARE(page.nextOffset, 0);
        QVERIFY(page.record.isEmpty());
        QVERIFY(!parsePiSatDevicePage("bad,page", &page));
    }
};

QTEST_MAIN(SecureProtocolTests)
#include "SecureProtocolTests.moc"
