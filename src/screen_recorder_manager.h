#ifndef SCREEN_RECORDER_MANAGER_H
#define SCREEN_RECORDER_MANAGER_H

#include <QObject>
#include <QString>

class ScreenRecorderManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool finalizing READ finalizing NOTIFY finalizingChanged)
public:
    explicit ScreenRecorderManager(QObject* parent = nullptr);

    Q_INVOKABLE bool startScreenRecording(const QString& outputPath, int fps = 30, qint64 bitrate = 4ll * 1000 * 1000);
    // 同步停止（会阻塞UI，不推荐在QML直接使用）
    Q_INVOKABLE bool stopScreenRecording();
    // 异步停止：接受停止时的延迟，不阻塞UI
    Q_INVOKABLE void stopScreenRecordingAsync();

    Q_INVOKABLE bool isRecording() const;

    // 硬件编码已移除，固定软件编码 720p

    bool finalizing() const { return m_finalizing; }

signals:
    void finalizingChanged();
    void stopCompleted(bool ok);

private:
    void setFinalizing(bool v);
    bool m_finalizing = false;
};

#endif // SCREEN_RECORDER_MANAGER_H
