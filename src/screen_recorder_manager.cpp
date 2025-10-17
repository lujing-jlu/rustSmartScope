#include "screen_recorder_manager.h"
#include "logger.h"
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>

ScreenRecorderManager::ScreenRecorderManager(QObject* parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ScreenRecorderManager::updateElapsedTime);
    m_timer->setInterval(1000);
}

ScreenRecorderManager::~ScreenRecorderManager() {
    if (m_proc) {
        m_proc->kill();
        m_proc->deleteLater();
    }
}

void ScreenRecorderManager::updateElapsedTime() {
    qint64 secs = m_elapsed.elapsed() / 1000;
    int mm = static_cast<int>(secs / 60);
    int ss = static_cast<int>(secs % 60);
    m_elapsedTimeStr = QString::asprintf("%02d:%02d", mm, ss);
    emit elapsedTimeChanged();
}

QString ScreenRecorderManager::ensureOutputDirectory(const QString& rootDir) const {
    QString root = rootDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation) : rootDir;
    QString videosDir = root + "/Videos";
    QDir dir(videosDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return videosDir;
}

QString ScreenRecorderManager::buildOutputFilePath(const QString& rootDir) const {
    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return ensureOutputDirectory(rootDir) + QString("/record_%1.mp4").arg(ts);
}

bool ScreenRecorderManager::haveExecutable(const QString& name) const {
    QStringList paths = QString(::getenv("PATH")).split(":");
    for (const QString& p : paths) {
        QFileInfo fi(QDir(p), name);
        if (fi.exists() && fi.isExecutable()) return true;
    }
    return false;
}

QStringList ScreenRecorderManager::buildFfmpegArgs(const QString& filePath) const {
    QString sizeArg = "1920x1200";
    QString display = ::getenv("DISPLAY") ? QString::fromLocal8Bit(::getenv("DISPLAY")) : ":0";

    QStringList args;
    args << "-y" << "-f" << "x11grab"
         << "-video_size" << sizeArg
         << "-framerate" << "30"
         << "-i" << display
         << "-c:v" << "libx264"
         << "-preset" << "ultrafast"
         << "-pix_fmt" << "yuv420p"
         << filePath;
    return args;
}

QStringList ScreenRecorderManager::buildWfRecorderArgs(const QString& filePath) const {
    QStringList args;
    args << "-f" << filePath << "-r" << "30";
    return args;
}

bool ScreenRecorderManager::launchRecorder(const QString& filePath) {
    if (m_proc) {
        m_proc->deleteLater();
        m_proc = nullptr;
    }

    m_proc = new QProcess(this);
    connect(m_proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &ScreenRecorderManager::handleProcessFinished);
    connect(m_proc, &QProcess::errorOccurred,
            this, &ScreenRecorderManager::handleProcessError);

    QString program;
    QStringList args;

    if (haveExecutable("wf-recorder")) {
        program = "wf-recorder";
        args = buildWfRecorderArgs(filePath);
        LOG_INFO("ScreenRecorder", "Using wf-recorder for Wayland");
    } else if (haveExecutable("ffmpeg")) {
        program = "ffmpeg";
        args = buildFfmpegArgs(filePath);
        LOG_INFO("ScreenRecorder", "Using ffmpeg for X11");
    } else {
        LOG_ERROR("ScreenRecorder", "No recording backend available (wf-recorder or ffmpeg)");
        return false;
    }

    LOG_INFO("ScreenRecorder", "Starting:", program.toStdString(), args.join(" ").toStdString());
    m_proc->start(program, args);
    return m_proc->waitForStarted(2000);
}

bool ScreenRecorderManager::startScreenRecording(const QString& rootDir) {
    if (m_isRecording) {
        LOG_WARN("ScreenRecorder", "Already recording");
        return false;
    }

    QString filePath = buildOutputFilePath(rootDir);
    if (!launchRecorder(filePath)) {
        LOG_ERROR("ScreenRecorder", "Failed to start recording process");
        return false;
    }

    m_outputPath = filePath;
    m_isRecording = true;
    m_elapsed.start();
    m_timer->start();

    emit recordingChanged();
    emit recordingStarted(filePath);

    LOG_INFO("ScreenRecorder", "Recording started:", filePath.toStdString());
    return true;
}

void ScreenRecorderManager::stopScreenRecording() {
    if (!m_isRecording) {
        return;
    }

    m_isRecording = false;
    m_timer->stop();
    emit recordingChanged();

    if (m_proc) {
        LOG_INFO("ScreenRecorder", "Stopping recording process...");
        m_proc->terminate();  // Send SIGTERM
        m_proc->waitForFinished(3000);  // Wait up to 3 seconds

        if (m_proc->state() == QProcess::Running) {
            LOG_WARN("ScreenRecorder", "Process did not terminate, killing...");
            m_proc->kill();
        }
    }

    emit recordingStopped(m_outputPath, true);
    LOG_INFO("ScreenRecorder", "Recording stopped:", m_outputPath.toStdString());
}

void ScreenRecorderManager::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    LOG_INFO("ScreenRecorder", "Process finished, exitCode=", exitCode,
             "exitStatus=", (exitStatus == QProcess::NormalExit ? "Normal" : "Crashed"));

    if (m_isRecording) {
        m_isRecording = false;
        m_timer->stop();
        emit recordingChanged();
        emit recordingStopped(m_outputPath, exitStatus == QProcess::NormalExit);
    }
}

void ScreenRecorderManager::handleProcessError(QProcess::ProcessError error) {
    QString errorStr;
    switch (error) {
        case QProcess::FailedToStart: errorStr = "FailedToStart"; break;
        case QProcess::Crashed: errorStr = "Crashed"; break;
        case QProcess::Timedout: errorStr = "Timedout"; break;
        case QProcess::WriteError: errorStr = "WriteError"; break;
        case QProcess::ReadError: errorStr = "ReadError"; break;
        default: errorStr = "UnknownError"; break;
    }

    LOG_ERROR("ScreenRecorder", "Process error:", errorStr.toStdString());

    if (m_isRecording) {
        m_isRecording = false;
        m_timer->stop();
        emit recordingChanged();
        emit recordingStopped(m_outputPath, false);
    }
}
