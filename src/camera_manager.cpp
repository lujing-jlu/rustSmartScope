#include "camera_manager.h"
#include <QDebug>
#include <QBuffer>
#include <QImageReader>
#include <QMutexLocker>

CameraManager::CameraManager(QObject *parent)
    : QObject(parent)
    , m_updateTimer(new QTimer(this))
    , m_cameraRunning(false)
    , m_leftConnected(false)
    , m_rightConnected(false)
{
    // 设置定时器每33ms更新一次 (~30 FPS)
    m_updateTimer->setInterval(33);
    connect(m_updateTimer, &QTimer::timeout, this, &CameraManager::updateFrames);

    qDebug() << "CameraManager initialized";
}

CameraManager::~CameraManager()
{
    stopCamera();
}

bool CameraManager::startCamera()
{
    qDebug() << "Starting camera system...";

    int result = smartscope_start_camera();
    if (result == 0) { // Success
        m_cameraRunning = true;
        m_updateTimer->start();
        emit cameraRunningChanged();
        qDebug() << "Camera system started successfully";
        return true;
    } else {
        qDebug() << "Failed to start camera system, error code:" << result;
        return false;
    }
}

bool CameraManager::stopCamera()
{
    qDebug() << "Stopping camera system...";

    m_updateTimer->stop();

    int result = smartscope_stop_camera();
    if (result == 0) { // Success
        m_cameraRunning = false;
        m_leftConnected = false;
        m_rightConnected = false;

        QMutexLocker locker(&m_frameMutex);
        m_leftFrame = QImage();
        m_rightFrame = QImage();

        emit cameraRunningChanged();
        emit leftConnectedChanged();
        emit rightConnectedChanged();
        emit leftFrameChanged();
        emit rightFrameChanged();

        qDebug() << "Camera system stopped successfully";
        return true;
    } else {
        qDebug() << "Failed to stop camera system, error code:" << result;
        return false;
    }
}

QImage CameraManager::leftFrame() const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_frameMutex));
    return m_leftFrame;
}

QImage CameraManager::rightFrame() const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_frameMutex));
    return m_rightFrame;
}

bool CameraManager::cameraRunning() const
{
    return m_cameraRunning;
}

bool CameraManager::leftConnected() const
{
    return m_leftConnected;
}

bool CameraManager::rightConnected() const
{
    return m_rightConnected;
}

QPixmap CameraManager::getLeftPixmap() const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_frameMutex));
    return m_leftPixmap;
}

QPixmap CameraManager::getRightPixmap() const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_frameMutex));
    return m_rightPixmap;
}

void CameraManager::updateFrames()
{
    // 处理相机帧数据
    smartscope_process_camera_frames();

    // 更新状态
    updateStatus();

    bool frameUpdated = false;

    // 获取左相机帧
    CCameraFrame leftFrame;
    if (smartscope_get_left_frame(&leftFrame) == 0) {
        QImage newLeftImage = processFrame(leftFrame);
        if (!newLeftImage.isNull()) {
            QPixmap newLeftPixmap = QPixmap::fromImage(newLeftImage);

            {
                QMutexLocker locker(&m_frameMutex);
                m_leftFrame = newLeftImage;
                m_leftPixmap = newLeftPixmap;
                frameUpdated = true;
            }

            // 发送QPixmap信号给原生Widget（不需要锁，信号是异步的）
            emit leftPixmapUpdated(newLeftPixmap);
        }
    }

    // 获取右相机帧
    CCameraFrame rightFrame;
    if (smartscope_get_right_frame(&rightFrame) == 0) {
        QImage newRightImage = processFrame(rightFrame);
        if (!newRightImage.isNull()) {
            QPixmap newRightPixmap = QPixmap::fromImage(newRightImage);

            {
                QMutexLocker locker(&m_frameMutex);
                m_rightFrame = newRightImage;
                m_rightPixmap = newRightPixmap;
                frameUpdated = true;
            }

            // 发送QPixmap信号给原生Widget
            emit rightPixmapUpdated(newRightPixmap);
        }
    }

    // 发出信号通知QML更新
    if (frameUpdated) {
        emit leftFrameChanged();
        emit rightFrameChanged();
    }
}

QImage CameraManager::processFrame(const CCameraFrame& frame)
{
    if (frame.data == nullptr || frame.data_len == 0) {
        return QImage();
    }

    // 深拷贝MJPEG数据到C++管理的内存（关键：Rust的指针在返回后会失效）
    QByteArray imageData(reinterpret_cast<const char*>(frame.data), static_cast<int>(frame.data_len));

    QBuffer buffer(&imageData);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer, "JPEG");
    QImage image = reader.read();

    if (image.isNull()) {
        qDebug() << "Failed to decode MJPEG frame:" << reader.errorString();
        return QImage();
    }

    // 确保图像格式适合显示
    if (image.format() != QImage::Format_RGB888) {
        image = image.convertToFormat(QImage::Format_RGB888);
    }

    return image;
}

void CameraManager::updateStatus()
{
    CCameraStatus status;
    if (smartscope_get_camera_status(&status) == 0) {
        bool wasRunning = m_cameraRunning;
        bool wasLeftConnected = m_leftConnected;
        bool wasRightConnected = m_rightConnected;

        m_cameraRunning = status.running;
        m_leftConnected = status.left_camera_connected;
        m_rightConnected = status.right_camera_connected;

        if (wasRunning != m_cameraRunning) {
            emit cameraRunningChanged();
        }
        if (wasLeftConnected != m_leftConnected) {
            emit leftConnectedChanged();
        }
        if (wasRightConnected != m_rightConnected) {
            emit rightConnectedChanged();
        }
    }
}