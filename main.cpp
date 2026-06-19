#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickItem>
#include <QQuickWindow>

#include "MediaCore.h"
#include "VideoDisplay.h"

#include <iostream>

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle("Basic");

    QGuiApplication app(argc, argv);
    // 设置“组织名称”和“域名” 为配置文件的自动存储指明路径
    app.setOrganizationName("MediaPlayer");
    app.setOrganizationDomain("config");

    QQmlApplicationEngine engine;
    MediaCore core;

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [&](QObject* obj, const QUrl& url) {
            VideoDisplay* display = obj->findChild<VideoDisplay*>("myVideo");
            
            if (display) {
                // 把界面上的画板交给 FFmpeg 引擎
                core.SetVideoRenderer(display);
                
                // 开始播放！
                core.Play("/home/virtual_self/Projects/MediaPlayer/test.mp4");
            }
        },  
        Qt::QueuedConnection
    );

    engine.loadFromModule("MediaPlayer", "Main");

    return app.exec();
}