#include "md3.h"

#include <QApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    Md3::RunOptions opts;
    opts.organization = QStringLiteral("QML_MD3");
    opts.applicationName = QStringLiteral("Md3 Create");
    opts.applicationVersion = QStringLiteral("0.1.0");
#if defined(Q_OS_WIN)
    opts.appUserModelId = QStringLiteral("QML_MD3.Md3Create");
#endif

    Md3::applyEarly(argc, argv, opts);

    QApplication app(argc, argv);
    Md3::initialize(app, opts);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("Md3Create", "Main");
    return app.exec();
}
