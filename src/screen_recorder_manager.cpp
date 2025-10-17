#include "screen_recorder_manager.h"
#include "logger.h"
#include <thread>

extern "C" {
    int smartscope_screenrec_start(const char* output_path, unsigned int fps, unsigned long long bitrate);
    int smartscope_screenrec_stop();
    int smartscope_recorder_is_recording();
}

ScreenRecorderManager::ScreenRecorderManager(QObject* parent)
    : QObject(parent) {}

bool ScreenRecorderManager::startScreenRecording(const QString& outputPath, int fps, qint64 bitrate) {
    if (m_finalizing) return false; // 正在收尾
    if (outputPath.isEmpty()) return false;
    QByteArray p = outputPath.toUtf8();
    int rc = smartscope_screenrec_start(p.constData(), (unsigned int)fps, (unsigned long long)bitrate);
    if (rc != 0) {
        LOG_ERROR("ScreenRecorder", "start failed rc=", rc, ", path=", outputPath.toStdString());
        return false;
    }
    LOG_INFO("ScreenRecorder", "started path=", outputPath.toStdString(), ", fps=", fps, ", bitrate=", (long long)bitrate);
    return true;
}

bool ScreenRecorderManager::stopScreenRecording() {
    setFinalizing(true);
    int rc = smartscope_screenrec_stop();
    setFinalizing(false);
    if (rc != 0) {
        LOG_ERROR("ScreenRecorder", "stop failed rc=", rc);
        return false;
    }
    LOG_INFO("ScreenRecorder", "stopped");
    return true;
}

void ScreenRecorderManager::stopScreenRecordingAsync() {
    if (m_finalizing) return;
    setFinalizing(true);
    std::thread([this]() {
        int rc = smartscope_screenrec_stop();
        QMetaObject::invokeMethod(this, [this, rc]() {
            setFinalizing(false);
            emit stopCompleted(rc == 0);
            if (rc != 0) {
                LOG_ERROR("ScreenRecorder", "stop async failed rc=", rc);
            } else {
                LOG_INFO("ScreenRecorder", "stopped (async)");
            }
        });
    }).detach();
}

bool ScreenRecorderManager::isRecording() const {
    return smartscope_recorder_is_recording() != 0;
}

void ScreenRecorderManager::setFinalizing(bool v) {
    if (m_finalizing == v) return;
    m_finalizing = v;
    emit finalizingChanged();
}

// 硬件编码相关接口已移除
