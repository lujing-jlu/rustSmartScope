#include "app/ui/screen_recorder_overlay.h"
#include "infrastructure/config/config_manager.h"
#include <QHBoxLayout>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>

ScreenRecorderOverlay::ScreenRecorderOverlay(QWidget* parent)
	: QWidget(parent)
{
	setAttribute(Qt::WA_TranslucentBackground, true);
	setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
	setAutoFillBackground(false);

	m_startButton = new QPushButton("开始录制", this);
	m_stopButton = new QPushButton("结束录制", this);
	m_timeLabel = new QLabel("00:00", this);

	m_stopButton->setEnabled(false);

	auto layout = new QHBoxLayout(this);
	layout->setContentsMargins(10, 10, 10, 10);
	layout->setSpacing(8);
	layout->addWidget(m_startButton);
	layout->addWidget(m_stopButton);
	layout->addWidget(m_timeLabel);

	m_timer = new QTimer(this);
	connect(m_timer, &QTimer::timeout, this, &ScreenRecorderOverlay::tick);
	m_timer->setInterval(1000);

	connect(m_startButton, &QPushButton::clicked, this, &ScreenRecorderOverlay::startRecording);
	connect(m_stopButton, &QPushButton::clicked, this, &ScreenRecorderOverlay::stopRecording);

	// 默认隐藏UI，只作为后台控制器使用
	hide();
	m_startButton->hide();
	m_stopButton->hide();
	m_timeLabel->hide();
}

ScreenRecorderOverlay::~ScreenRecorderOverlay() {
	if (m_proc) {
		m_proc->kill();
		m_proc->deleteLater();
	}
}

void ScreenRecorderOverlay::updatePosition(int parentWidth, int parentHeight) {
	const int margin = 20;
	adjustSize();
	int w = width();
	int h = height();
	setGeometry(parentWidth - w - margin, parentHeight - h - margin, w, h);
	raise();
    // 默认不显示
}

void ScreenRecorderOverlay::tick() {
	qint64 secs = m_elapsed.elapsed() / 1000;
	int mm = static_cast<int>(secs / 60);
	int ss = static_cast<int>(secs % 60);
	QString t = QString::asprintf("%02d:%02d", mm, ss);
	m_timeLabel->setText(t);
	emit elapsedUpdated(t);
}

QString ScreenRecorderOverlay::ensureOutputDirectory() const {
	// 统一到配置的根目录下: ~/data/Recordings
	QString rootDirectory = SmartScope::Infrastructure::ConfigManager::instance().getValue("app/root_directory", QDir::homePath() + "/data").toString();
	QString sub = rootDirectory + "/Videos";
	QDir subdir(sub);
	if (!subdir.exists()) subdir.mkpath(".");
	return sub;
}

QString ScreenRecorderOverlay::buildOutputFilePath() const {
	QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
	return ensureOutputDirectory() + QString("/record_%1.mp4").arg(ts);
}

bool ScreenRecorderOverlay::haveExecutable(const QString& name) const {
	QStringList paths = QString(::getenv("PATH")).split(":");
	for (const QString& p : paths) {
		QFileInfo fi(QDir(p), name);
		if (fi.exists() && fi.isExecutable()) return true;
	}
	return false;
}

QStringList ScreenRecorderOverlay::buildFfmpegArgs(const QString& filePath) const {
	// 尝试X11录屏，若是Wayland可退回wf-recorder
	// -f x11grab -s WxH -i :0.0 -r 30 -codec:v libx264 -preset ultrafast -pix_fmt yuv420p file
	QSize sz = parentWidget() ? parentWidget()->size() : QSize(1280, 720);
	QString sizeArg = QString::asprintf("%dx%d", sz.width(), sz.height());
	QString display = ::getenv("DISPLAY") ? QString::fromLocal8Bit(::getenv("DISPLAY")) : ":0.0";
	QStringList args;
	args << "-y" << "-f" << "x11grab" << "-video_size" << sizeArg << "-i" << display
		 << "-r" << "30" << "-codec:v" << "libx264" << "-preset" << "ultrafast" << "-pix_fmt" << "yuv420p" << filePath;
	return args;
}

QStringList ScreenRecorderOverlay::buildWfRecorderArgs(const QString& filePath) const {
	// Wayland: wf-recorder -f file -r 30
	QStringList args;
	args << "-f" << filePath << "-r" << "30";
	return args;
}

bool ScreenRecorderOverlay::launchRecorder(const QString& filePath) {
	if (m_proc) {
		m_proc->deleteLater();
		m_proc = nullptr;
	}
	m_proc = new QProcess(this);
	connect(m_proc, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, &ScreenRecorderOverlay::handleProcessFinished);
	connect(m_proc, &QProcess::errorOccurred, this, &ScreenRecorderOverlay::handleProcessError);

	QString program;
	QStringList args;
	if (haveExecutable("wf-recorder")) {
		program = "wf-recorder";
		args = buildWfRecorderArgs(filePath);
	} else if (haveExecutable("ffmpeg")) {
		program = "ffmpeg";
		args = buildFfmpegArgs(filePath);
	} else {
		return false;
	}

	m_proc->start(program, args);
	return m_proc->waitForStarted(2000);
}

void ScreenRecorderOverlay::updateUIState() {
	m_startButton->setEnabled(!m_isRecording);
	m_stopButton->setEnabled(m_isRecording);
	m_timeLabel->setVisible(m_isRecording);
}

void ScreenRecorderOverlay::startRecording() {
	if (m_isRecording) return;
	QString file = buildOutputFilePath();
	if (!launchRecorder(file)) {
		m_timeLabel->setText("启动录制失败");
		return;
	}
	m_outputPath = file;
	m_isRecording = true;
	m_elapsed.start();
	m_timer->start();
	updateUIState();
	emit recordingStarted(file);
}

void ScreenRecorderOverlay::stopRecording() {
	if (!m_isRecording) return;
	m_isRecording = false;
	m_timer->stop();
	updateUIState();
	if (m_proc) {
		m_proc->terminate();
		m_proc->waitForFinished(2000);
	}
	emit recordingStopped(m_outputPath, true);
}

void ScreenRecorderOverlay::handleProcessFinished(int, QProcess::ExitStatus) {
	// 进程结束
}

void ScreenRecorderOverlay::handleProcessError(QProcess::ProcessError) {
	// 记录错误
} 