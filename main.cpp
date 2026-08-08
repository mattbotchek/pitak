#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDateTime>
#include <QTimer>
#include <QDebug>

#include <cstdio>

#include "LoRaManager.h"

// PiTAK can run two ways:
//
//   pitak            -> Qt/QML GUI (unchanged behaviour, needs a display)
//   pitak serve      -> headless daemon, no GUI, for systemd
//
// The headless path exists because LoRaManager is a plain QObject driven by
// QTimers and has no GUI dependency: it only needs a running event loop. The
// systemd unit has always invoked `pitak serve`, but main() previously ignored
// argv and started the QML engine, which aborts under systemd with
// "could not connect to display" (SIGABRT, exit 134) in a restart loop -- so
// the base station never actually opened the radio.

static void logLine(const QString &text)
{
    // stdout, line-buffered and flushed: the unit redirects it into
    // /var/log/pitak/pitak.log and we want entries to appear immediately.
    const QString stamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    std::fprintf(stdout, "%s %s\n", qPrintable(stamp), qPrintable(text));
    std::fflush(stdout);
}

static int runHeadless(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("PiTAK"));
    QCoreApplication::setApplicationName(QStringLiteral("pitak"));

    logLine(QStringLiteral("PiTAK LoRa service starting (headless)"));

    LoRaManager lora;

    logLine(QStringLiteral("security: ") + lora.securityStatus());
    logLine(QStringLiteral("pisat: ") + lora.pisatStatus());

    // Surface state changes so the service log is actually diagnosable.
    QObject::connect(&lora, &LoRaManager::statusChanged, &lora, [&lora]() {
        logLine(QStringLiteral("security: ") + lora.securityStatus());
        logLine(QStringLiteral("pisat: ") + lora.pisatStatus());
    });
    QObject::connect(&lora, &LoRaManager::peersChanged, &lora, [&lora]() {
        logLine(QStringLiteral("peers: %1 visible").arg(lora.peers().size()));
    });
    QObject::connect(&lora, &LoRaManager::telemetryChanged, &lora, [&lora]() {
        logLine(QStringLiteral("telemetry records: %1").arg(lora.telemetryRecords().size()));
    });

    // In the GUI the operator drives PiSAT with buttons. Headless there is no
    // operator, so poll PiSAT on a timer -- otherwise the base station only ever
    // listens and never exercises the uplink. Interval is configurable;
    // 0 disables polling.
    const int pollSeconds =
        qEnvironmentVariableIsSet("PITAK_STATUS_POLL_SECONDS")
            ? qEnvironmentVariableIntValue("PITAK_STATUS_POLL_SECONDS")
            : 30;
    if (pollSeconds > 0) {
        auto *poll = new QTimer(&app);
        poll->setInterval(pollSeconds * 1000);
        QObject::connect(poll, &QTimer::timeout, &app, [&lora]() {
            logLine(QStringLiteral("-> requesting PiSAT status"));
            lora.requestStatus();
        });
        poll->start();
        // don't wait a full interval for the first probe
        QTimer::singleShot(3000, &app, [&lora]() {
            logLine(QStringLiteral("-> requesting PiSAT status (initial)"));
            lora.requestStatus();
        });
        logLine(QStringLiteral("PiSAT status polling every %1s").arg(pollSeconds));
    }

    // Periodic heartbeat line so an operator can tell the daemon is alive even
    // when the link is quiet.
    auto *health = new QTimer(&app);
    health->setInterval(60000);
    QObject::connect(health, &QTimer::timeout, &app, [&lora]() {
        logLine(QStringLiteral("alive | peers=%1 | %2")
                    .arg(lora.peers().size())
                    .arg(lora.pisatStatus()));
    });
    health->start();

    return app.exec();
}

static int runGui(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    LoRaManager lora;
    engine.rootContext()->setContextProperty("lora", &lora);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}

int main(int argc, char *argv[])
{
    bool headless = qEnvironmentVariableIsSet("PITAK_HEADLESS");
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromUtf8(argv[i]);
        if (arg == QLatin1String("serve") || arg == QLatin1String("--headless") ||
            arg == QLatin1String("--serve"))
            headless = true;
    }

    return headless ? runHeadless(argc, argv) : runGui(argc, argv);
}
