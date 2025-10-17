#ifndef SCREEN_RECORDER_MANAGER_H
#define SCREEN_RECORDER_MANAGER_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QElapsedTimer>
#include <QTimer>

class ScreenRecorderManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY recordingChanged)
    Q_PROPERTY(QString elapsedTime READ elapsedTime NOTIFY elapsedTimeChanged)

public:
    explicit ScreenRecorderManager(QObject* parent = nullptr);
    ~ScreenRecorderManager();

    Q_INVOKABLE bool startScreenRecording(const QString& rootDir = QString());
    Q_INVOKABLE void stopScreenRecording();

    bool isRecording() const { return m_isRecording; }
    QString elapsedTime() const { return m_elapsedTimeStr; }

signals:
    void recordingChanged();
    void elapsedTimeChanged();
    void recordingStarted(const QString& filePath);
    void recordingStopped(const QString& filePath, bool success);

private slots:
    void updateElapsedTime();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleProcessError(QProcess::ProcessError error);

private:
    QString ensureOutputDirectory(const QString& rootDir) const;
    QString buildOutputFilePath(const QString& rootDir) const;
    bool haveExecutable(const QString& name) const;
    QStringList buildFfmpegArgs(const QString& filePath) const;
    QStringList buildWfRecorderArgs(const QString& filePath) const;
    bool launchRecorder(const QString& filePath);

    QProcess* m_proc = nullptr;
    QElapsedTimer m_elapsed;
    QTimer* m_timer = nullptr;
    bool m_isRecording = false;
    QString m_outputPath;
    QString m_elapsedTimeStr = "00:00";
};

#endif // SCREEN_RECORDER_MANAGER_H
