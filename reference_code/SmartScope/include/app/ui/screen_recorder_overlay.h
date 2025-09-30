#ifndef SCREEN_RECORDER_OVERLAY_H
#define SCREEN_RECORDER_OVERLAY_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
#include <QProcess>

class ScreenRecorderOverlay : public QWidget {
	Q_OBJECT
public:
	explicit ScreenRecorderOverlay(QWidget* parent = nullptr);
	~ScreenRecorderOverlay() override;

	// 右下角定位
	void updatePosition(int parentWidth, int parentHeight);

	bool isRecording() const { return m_isRecording; }
	QString outputPath() const { return m_outputPath; }

signals:
	void recordingStarted(const QString& path);
	void recordingStopped(const QString& path, bool success);
	void elapsedUpdated(const QString& text);

public slots:
	void startRecording();
	void stopRecording();

private slots:
	void tick();
	void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
	void handleProcessError(QProcess::ProcessError error);

private:
	void updateUIState();
	QString ensureOutputDirectory() const;
	QString buildOutputFilePath() const;
	bool launchRecorder(const QString& filePath);
	QStringList buildFfmpegArgs(const QString& filePath) const;
	QStringList buildWfRecorderArgs(const QString& filePath) const;
	bool haveExecutable(const QString& name) const;

private:
	QPushButton* m_startButton = nullptr;
	QPushButton* m_stopButton = nullptr;
	QLabel* m_timeLabel = nullptr;
	QTimer* m_timer = nullptr;
	QElapsedTimer m_elapsed;
	QProcess* m_proc = nullptr;
	bool m_isRecording = false;
	QString m_outputPath;
};

#endif // SCREEN_RECORDER_OVERLAY_H 