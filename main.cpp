#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickItem>
#include <QQuickWindow>

#include "MediaCore.h"
#include "VideoDisplay.h"

#include <iostream>
#include <QTimer>

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle("Basic");

    QGuiApplication app(argc, argv);
    // 设置“组织名称”和“域名” 为配置文件的自动存储指明路径
    app.setOrganizationName("MediaPlayer");
    app.setOrganizationDomain("config");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );
    
    engine.loadFromModule("MediaPlayer", "Main");

    return app.exec();
}