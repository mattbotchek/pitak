#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "LoRaManager.h"

int main(int argc, char *argv[])
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
