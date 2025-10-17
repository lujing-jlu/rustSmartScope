#include "camera_manager.h"
#include "logger.h"
#include <QBuffer>
#include <QImageReader>
#include <QFile>
#include <QIODevice>
#include <QMutexLocker>
#include <QTransform>
#include <QDateTime>
#include <thread>

// Rust FFI for video transforms
extern "C" {
    uint32_t smartscope_video_get_rotation();
    int smartscope_capture_stereo_to_dir(const char* dir);
    int smartscope_capture_single_to_dir(const char* dir);
    bool smartscope_video_get_flip_horizontal();
    bool smartscope_video_get_flip_vertical();
    bool smartscope_video_get_invert();
}

// Camera video recorder FFI (NOT screen recorder - currently disabled)
// TODO: Re-implement camera video recording using frame submission
// extern "C" {
//     int smartscope_recorder_init_simple(...);
//     int smartscope_recorder_start();
//     int smartscope_recorder_stop();
//     int smartscope_recorder_is_recording();
//     int smartscope_recorder_submit_rgb888(...);
// }

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

    LOG_INFO("CameraManager", "CameraManager initialized");
}

CameraManager::~CameraManager()
{
    stopCamera();
}

bool CameraManager::startCamera()
{
    LOG_INFO("CameraManager", "Starting camera system...");

    int result = smartscope_start_camera();
    if (result == 0) { // Success
        m_cameraRunning = true;
        m_updateTimer->start();
        emit cameraRunningChanged();
        LOG_INFO("CameraManager", "Camera system started successfully");
        return true;
    } else {
        LOG_ERROR("CameraManager", "Failed to start camera system, error code: ", result);
        return false;
    }
}

bool CameraManager::stopCamera()
{
    LOG_INFO("CameraManager", "Stopping camera system...");

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

        LOG_INFO("CameraManager", "Camera system stopped successfully");
        return true;
    } else {
        LOG_ERROR("CameraManager", "Failed to stop camera system, error code: ", result);
        return false;
    }
}

bool CameraManager::startVideoRecording(const QString& outputPath)
{
    // TODO: Re-implement camera video recording using frame submission
    LOG_WARN("CameraManager", "Camera video recording not yet implemented (use screen recorder instead)");
    (void)outputPath;
    return false;
}

bool CameraManager::stopVideoRecording()
{
    // TODO: Re-implement camera video recording
    LOG_WARN("CameraManager", "Camera video recording not yet implemented");
    return false;
}

bool CameraManager::isRecording() const
{
    // TODO: Re-implement camera video recording status
    return false;
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

QImage CameraManager::decodeRawFrame(const CCameraFrame& frame)
{
    if (frame.data == nullptr || frame.data_len == 0) {
        return QImage();
    }
    if (frame.format == 0) {
        // 已是RGB888，按原样拷贝（可能已包含上游处理）
        QImage image(frame.data, frame.width, frame.height, frame.width * 3, QImage::Format_RGB888);
        return image.copy();
    } else {
        // MJPEG 解码为RGB，不做额外变换
        QByteArray imageData(reinterpret_cast<const char*>(frame.data), static_cast<int>(frame.data_len));
        QBuffer buffer(&imageData);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "JPEG");
        QImage image = reader.read();
        if (image.isNull()) {
            return QImage();
        }
        if (image.format() != QImage::Format_RGB888) {
            image = image.convertToFormat(QImage::Format_RGB888);
        }
        return image;
    }
}

bool CameraManager::saveRawFrameToFile(const CCameraFrame& frame, const QString& basePathNoExt)
{
    if (frame.data == nullptr || frame.data_len == 0) return false;
    // format == 0: RGB888 raw buffer; otherwise assume compressed (e.g., MJPEG)
    if (frame.format == 0) {
        QImage img(frame.data, frame.width, frame.height, frame.width * 3, QImage::Format_RGB888);
        QImage copy = img.copy();
        return copy.save(basePathNoExt + ".png", "PNG");
    } else {
        QFile f(basePathNoExt + ".jpg");
        if (!f.open(QIODevice::WriteOnly)) return false;
        qint64 written = f.write(reinterpret_cast<const char*>(frame.data), static_cast<qint64>(frame.data_len));
        f.close();
        return written == static_cast<qint64>(frame.data_len);
    }
}

bool CameraManager::saveScreenshotSession(const QString& sessionDir)
{
    if (sessionDir.isEmpty()) return false;

    auto saveImage = [](const QImage& img, const QString& path) -> bool {
        if (img.isNull()) return false;
        return img.save(path, "PNG");
    };

    int saved = 0;
    uint32_t mode = smartscope_get_camera_mode();

    if (mode == StereoCamera) {
        // 由后端统一采集与处理并落盘，避免前端解码与失败边界
        QByteArray dir = sessionDir.toUtf8();
        int rc = smartscope_capture_stereo_to_dir(dir.constData());
        if (rc == 0) {
            saved = 1; // 标记成功
        }
    } else if (mode == SingleCamera) {
        QByteArray dir = sessionDir.toUtf8();
        int rc = smartscope_capture_single_to_dir(dir.constData());
        if (rc == 0) {
            saved = 1;
        }
    }

    if (saved == 0) {
        LOG_ERROR("CameraManager", "saveScreenshotSession: no files saved, mode=", static_cast<int>(mode), 
                  ", dir=", sessionDir.toStdString());
    }
    return saved > 0;
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

        // 不再主动丢弃右相机帧，保留以便拍照/立体处理使用

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

                // PiP 缩略图改为前端直接缩放显示帧
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
            LOG_WARN("CameraManager", "Failed to decode MJPEG frame: ", reader.errorString().toStdString());
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
            LOG_INFO("CameraManager", "Camera mode changed: ", wasMode, "->", m_cameraMode);
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

QImage CameraManager::recoverOriginalFromProcessed(const QImage& processed)
{
    if (processed.isNull()) {
        return processed;
    }

    QImage result = processed;

    // 获取当前视频变换状态（按逆序还原）
    uint32_t rotation = smartscope_video_get_rotation();
    bool flipH = smartscope_video_get_flip_horizontal();
    bool flipV = smartscope_video_get_flip_vertical();
    bool invert = smartscope_video_get_invert();

    // 先还原反色
    if (invert) {
        result.invertPixels();
    }
    // 再还原翻转（镜像一次）
    if (flipH || flipV) {
        result = result.mirrored(flipH, flipV);
    }
    // 最后还原旋转（逆向角度）
    if (rotation != 0) {
        QTransform transform;
        uint32_t r = rotation % 360u;
        if (r != 0u) {
            transform.rotate(360 - r);
            result = result.transformed(transform, Qt::SmoothTransformation);
        }
    }

    return result;
}


void CameraManager::startCameraAsync()
{
    if (m_starting || m_cameraRunning) return;
    m_starting = true;
    LOG_INFO("CameraManager", "startCameraAsync: spawning worker");
    std::thread([this]() {
        int result = smartscope_start_camera();
        QMetaObject::invokeMethod(this, [this, result]() {
            m_starting = false;
            if (result == 0) {
                m_cameraRunning = true;
                m_updateTimer->start();
                emit cameraRunningChanged();
                LOG_INFO("CameraManager", "Camera system started (async)");
            } else {
                LOG_ERROR("CameraManager", "Camera start (async) failed, code: ", result);
            }
        });
    }).detach();
}


void CameraManager::stopCameraAsync()
{
    if (m_stopping || !m_cameraRunning) {
        return;
    }
    m_stopping = true;
    LOG_INFO("CameraManager", "stopCameraAsync: spawning worker");
    m_updateTimer->stop();
    std::thread([this]() {
        int result = smartscope_stop_camera();
        QMetaObject::invokeMethod(this, [this, result]() {
            m_stopping = false;
            if (result == 0) {
                m_cameraRunning = false;
                m_leftConnected = false;
                m_rightConnected = false;
                m_cameraMode = NoCamera;
                emit cameraRunningChanged();
                emit leftConnectedChanged();
                emit rightConnectedChanged();
                emit cameraModeChanged();
                emit leftFrameChanged();
                emit rightFrameChanged();
                emit singleFrameChanged();
                LOG_INFO("CameraManager", "Camera system stopped (async)");
            } else {
                m_updateTimer->start();
                LOG_ERROR("CameraManager", "Camera stop (async) failed, code: ", result);
            }
        });
    }).detach();
}
