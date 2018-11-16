#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtCore/QLoggingCategory>

#include "devicefinder.h"
#include "devicehandler.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    DeviceHandler deviceHandler;
    DeviceFinder deviceFinder(&deviceHandler);

    qmlRegisterUncreatableType<DeviceHandler>("Shared", 1, 0, "AddressType", "Enum is not a type");

    QQmlApplicationEngine engine;
//    engine.rootContext()->setContextProperty("connectionHandler", &connectionHandler);
    engine.rootContext()->setContextProperty("deviceFinder", &deviceFinder);
    engine.rootContext()->setContextProperty("deviceHandler", &deviceHandler);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
