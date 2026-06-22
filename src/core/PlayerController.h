#ifndef PLAYERCONTROLLER_H
#define PLAYERCONTROLLER_H

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include "VideoDisplay.h"
#include "MediaCore.h"

class PlayerController : public QObject {
    Q_OBJECT
    QML_ELEMENT

    // 这两个属性暴露给 QML，让前端的进度条能随着播放自动更新
    // READ position: 通过position()来读取position变量
    // NOTIFY positionChanged: 如果positionChanged信号发射，提醒所有使用了position变量的控件
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)

public:
    explicit PlayerController(QObject* parent = nullptr);
    ~PlayerController();

    // 暴露给 QML 的控制函数（加上 Q_INVOKABLE 宏，QML 中的按钮就能直接调用它）
    Q_INVOKABLE void setVideoDisplay(VideoDisplay* display);
    Q_INVOKABLE void openFile(const QUrl& fileUrl);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void setVolume(double volume);

    double position() const;
    double duration() const;

signals:
    void positionChanged(double pos);
    void durationChanged(double dur);

private:
    std::unique_ptr<MediaCore> m_core{nullptr};
    QTimer* m_timer{nullptr};
    double m_duration{0.0};
};

#endif