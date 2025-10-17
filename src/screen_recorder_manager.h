#ifndef SCREEN_RECORDER_MANAGER_H
#define SCREEN_RECORDER_MANAGER_H

#include <QObject>
#include <QString>

class ScreenRecorderManager : public QObject {
    Q_OBJECT
public:
    explicit ScreenRecorderManager(QObject* parent = nullptr);

    Q_INVOKABLE bool startScreenRecording(const QString& outputPath, int fps = 30, qint64 bitrate = 4ll * 1000 * 1000);
    Q_INVOKABLE bool stopScreenRecording();
    Q_INVOKABLE bool isRecording() const;
};

#endif // SCREEN_RECORDER_MANAGER_H

