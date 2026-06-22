#include "PlayerController.h"

PlayerController::PlayerController(QObject* parent)
    : QObject(parent) {
    m_core = std::make_unique<MediaCore>();
    m_timer = new QTimer(nullptr);
    connect(m_timer, &QTimer::timeout, this, [this]() { emit positionChanged(m_core->GetPosition()); });
    m_timer->start(100);
}
PlayerController::~PlayerController() {
    m_core->Stop();
}

void PlayerController::setVideoDisplay(VideoDisplay* display) {
    m_core->SetVideoRenderer(display);
}

void PlayerController::openFile(const QUrl& fileUrl) {
    // QML 传来的文件路径通常带 file:// 前缀，需要处理一下
    QString localPath = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();

    m_core->Stop();
    m_core->Play(localPath.toStdString());

    m_duration = m_core->GetDuration();
    emit durationChanged(m_duration);
}

void PlayerController::pause() {
    m_core->SetPause(true);
}
void PlayerController::resume() {
    m_core->SetPause(false);
}

void PlayerController::seek(double seconds) {
    m_core->Seek(seconds);
}

void PlayerController::setVolume(double volume) {
    if (m_core->audioEngine) {
        m_core->audioEngine->SetVolume(volume);
    }
}

double PlayerController::position() const { return m_core->GetPosition(); }
double PlayerController::duration() const { return m_core->GetDuration(); }
