#include "camera_manager.h"
#include <QDebug>
#include <QBuffer>
#include <QImageReader>
#include <QMutexLocker>
#include <QTransform>

// Rust FFI for video transforms
extern "C" {
    uint32_t smartscope_video_get_rotation();
    bool smartscope_video_get_flip_horizontal();
    bool smartscope_video_get_flip_vertical();
    bool smartscope_video_get_invert();
}

CameraManager::CameraManager(QObject *parent)
    : QObject(parent)
    , m_updateTimer(new QTimer(this))
    , m_cameraRunning(false)
    , m_leftConnected(false)
    , m_rightConnected(false)
    , m_cameraMode(NoCamera)
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
        m_cameraMode = NoCamera;

        QMutexLocker locker(&m_frameMutex);
        m_leftFrame = QImage();
        m_rightFrame = QImage();
        m_singleFrame = QImage();

        emit cameraRunningChanged();
        emit leftConnectedChanged();
        emit rightConnectedChanged();
        emit cameraModeChanged();
        emit leftFrameChanged();
        emit rightFrameChanged();
        emit singleFrameChanged();

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

QImage CameraManager::singleFrame() const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_frameMutex));
    return m_singleFrame;
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

int CameraManager::cameraMode() const
{
    return m_cameraMode;
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

QPixmap CameraManager::getSinglePixmap() const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_frameMutex));
    return m_singlePixmap;
}

void CameraManager::updateFrames()
{
    // 处理相机帧数据
    smartscope_process_camera_frames();

    // 更新状态
    updateStatus();

    bool frameUpdated = false;
    uint32_t mode = smartscope_get_camera_mode();

    // 根据相机模式获取对应的帧
    if (mode == StereoCamera) {
        // 双目模式：只处理左相机帧（当前只显示左相机）
        // 右相机帧虽然在Rust侧读取了，但不在C++侧解码，避免浪费CPU
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

        // 丢弃右相机帧（不解码，只是清空Rust缓存）
        CCameraFrame rightFrame;
        smartscope_get_right_frame(&rightFrame);  // 只获取不处理，避免缓存积压

        // 发出信号通知QML更新
        if (frameUpdated) {
            emit leftFrameChanged();
        }
    } else if (mode == SingleCamera) {
        // 单目模式：获取单相机帧
        CCameraFrame singleFrame;
        if (smartscope_get_single_frame(&singleFrame) == 0) {
            QImage newSingleImage = processFrame(singleFrame);
            if (!newSingleImage.isNull()) {
                QPixmap newSinglePixmap = QPixmap::fromImage(newSingleImage);

                {
                    QMutexLocker locker(&m_frameMutex);
                    m_singleFrame = newSingleImage;
                    m_singlePixmap = newSinglePixmap;
                    frameUpdated = true;
                }

                // 发送QPixmap信号给原生Widget
                emit singlePixmapUpdated(newSinglePixmap);
            }
        }

        // 发出信号通知QML更新
        if (frameUpdated) {
            emit singleFrameChanged();
        }
    }
    // NoCamera模式：不获取帧数据
}

QImage CameraManager::processFrame(const CCameraFrame& frame)
{
    if (frame.data == nullptr || frame.data_len == 0) {
        return QImage();
    }

    // 检查帧格式：0 表示 RGB，其他值表示 MJPEG
    if (frame.format == 0) {
        // Rust端已经处理好的RGB数据（包含畸变校正和视频变换）
        // 直接创建QImage，不需要解码或变换
        QImage image(frame.data, frame.width, frame.height, frame.width * 3, QImage::Format_RGB888);

        // 深拷贝数据（因为Rust的指针会失效）
        return image.copy();
    } else {
        // 旧的MJPEG处理流程（降级模式）
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

        // 应用视频变换（旋转、翻转、反色等）
        image = applyVideoTransforms(image);

        return image;
    }
}

void CameraManager::updateStatus()
{
    CCameraStatus status;
    if (smartscope_get_camera_status(&status) == 0) {
        bool wasRunning = m_cameraRunning;
        bool wasLeftConnected = m_leftConnected;
        bool wasRightConnected = m_rightConnected;
        int wasMode = m_cameraMode;

        m_cameraRunning = status.running;
        m_leftConnected = status.left_camera_connected;
        m_rightConnected = status.right_camera_connected;
        m_cameraMode = static_cast<int>(status.mode);

        if (wasRunning != m_cameraRunning) {
            emit cameraRunningChanged();
        }
        if (wasLeftConnected != m_leftConnected) {
            emit leftConnectedChanged();
        }
        if (wasRightConnected != m_rightConnected) {
            emit rightConnectedChanged();
        }
        if (wasMode != m_cameraMode) {
            qDebug() << "Camera mode changed:" << wasMode << "->" << m_cameraMode;
            emit cameraModeChanged();
        }
    }
}

QImage CameraManager::applyVideoTransforms(const QImage& image)
{
    if (image.isNull()) {
        return image;
    }

    QImage result = image;

    // 获取当前视频变换状态
    uint32_t rotation = smartscope_video_get_rotation();
    bool flipH = smartscope_video_get_flip_horizontal();
    bool flipV = smartscope_video_get_flip_vertical();
    bool invert = smartscope_video_get_invert();

    // 应用旋转
    if (rotation != 0) {
        QTransform transform;
        transform.rotate(rotation);
        result = result.transformed(transform, Qt::SmoothTransformation);
    }

    // 应用翻转
    if (flipH || flipV) {
        result = result.mirrored(flipH, flipV);
    }

    // 应用反色
    if (invert) {
        result.invertPixels();
    }

    return result;
}