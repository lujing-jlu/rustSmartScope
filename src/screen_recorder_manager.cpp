#include "screen_recorder_manager.h"
#include "logger.h"

extern "C" {
    int smartscope_screenrec_start(const char* output_path, unsigned int fps, unsigned long long bitrate);
    int smartscope_screenrec_stop();
    int smartscope_recorder_is_recording();
}

ScreenRecorderManager::ScreenRecorderManager(QObject* parent)
    : QObject(parent) {}

bool ScreenRecorderManager::startScreenRecording(const QString& outputPath, int fps, qint64 bitrate) {
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
    int rc = smartscope_screenrec_stop();
    if (rc != 0) {
        LOG_ERROR("ScreenRecorder", "stop failed rc=", rc);
        return false;
    }
    LOG_INFO("ScreenRecorder", "stopped");
    return true;
}

bool ScreenRecorderManager::isRecording() const {
    return smartscope_recorder_is_recording() != 0;
}

