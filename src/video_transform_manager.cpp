#include "video_transform_manager.h"
#include "logger.h"

// Rust FFI声明
extern "C" {
    int32_t smartscope_video_apply_rotation();
    int32_t smartscope_video_set_rotation(uint32_t degrees);
    int32_t smartscope_video_toggle_flip_horizontal();
    int32_t smartscope_video_toggle_flip_vertical();
    int32_t smartscope_video_toggle_invert();
    int32_t smartscope_video_reset_transforms();
    uint32_t smartscope_video_get_rotation();
    bool smartscope_video_get_flip_horizontal();
    bool smartscope_video_get_flip_vertical();
    bool smartscope_video_get_invert();
    bool smartscope_video_is_rga_available();

    // 畸变校正
    int32_t smartscope_toggle_distortion_correction();
    int32_t smartscope_set_distortion_correction(bool enabled);
    bool smartscope_get_distortion_correction();
}

VideoTransformManager::VideoTransformManager(QObject *parent)
    : QObject(parent)
    , m_rotationDegrees(0)
    , m_flipHorizontal(false)
    , m_flipVertical(false)
    , m_invertColors(false)
    , m_rgaAvailable(false)
    , m_distortionCorrectionEnabled(false)
{
    // 检查RGA硬件可用性
    m_rgaAvailable = smartscope_video_is_rga_available();
    LOG_INFO("VideoTransformManager", "RGA hardware available: ", m_rgaAvailable ? "Yes" : "No");

    // 加载初始状态
    updateStatus();
}

VideoTransformManager::~VideoTransformManager()
{
}

int VideoTransformManager::rotationDegrees() const
{
    return m_rotationDegrees;
}

bool VideoTransformManager::flipHorizontal() const
{
    return m_flipHorizontal;
}

bool VideoTransformManager::flipVertical() const
{
    return m_flipVertical;
}

bool VideoTransformManager::invertColors() const
{
    return m_invertColors;
}

bool VideoTransformManager::rgaAvailable() const
{
    return m_rgaAvailable;
}

bool VideoTransformManager::distortionCorrectionEnabled() const
{
    return m_distortionCorrectionEnabled;
}

void VideoTransformManager::applyRotation()
{
    int result = smartscope_video_apply_rotation();
    if (result == 0) {  // Success
        updateStatus();
        emit rotationChanged();
        emit transformApplied();
        LOG_DEBUG("VideoTransformManager", "Rotation applied: ", m_rotationDegrees, "°");
    } else {
        QString errorMsg = "Failed to apply rotation";
        LOG_ERROR("VideoTransformManager", errorMsg.toStdString());
        emit error(errorMsg);
    }
}

void VideoTransformManager::setRotation(int degrees)
{
    int result = smartscope_video_set_rotation(static_cast<uint32_t>(degrees));
    if (result == 0) {  // Success
        updateStatus();
        emit rotationChanged();
        emit transformApplied();
        LOG_DEBUG("VideoTransformManager", "Rotation set to: ", degrees, "°");
    } else {
        QString errorMsg = QString("Failed to set rotation to %1°").arg(degrees);
        LOG_ERROR("VideoTransformManager", errorMsg.toStdString());
        emit error(errorMsg);
    }
}

void VideoTransformManager::toggleFlipHorizontal()
{
    int result = smartscope_video_toggle_flip_horizontal();
    if (result == 0) {  // Success
        updateStatus();
        emit flipHorizontalChanged();
        emit transformApplied();
        LOG_DEBUG("VideoTransformManager", "Flip horizontal: ", m_flipHorizontal ? "ON" : "OFF");
    } else {
        QString errorMsg = "Failed to toggle horizontal flip";
        LOG_ERROR("VideoTransformManager", errorMsg.toStdString());
        emit error(errorMsg);
    }
}

void VideoTransformManager::toggleFlipVertical()
{
    int result = smartscope_video_toggle_flip_vertical();
    if (result == 0) {  // Success
        updateStatus();
        emit flipVerticalChanged();
        emit transformApplied();
        LOG_DEBUG("VideoTransformManager", "Flip vertical: ", m_flipVertical ? "ON" : "OFF");
    } else {
        QString errorMsg = "Failed to toggle vertical flip";
        LOG_ERROR("VideoTransformManager", errorMsg.toStdString());
        emit error(errorMsg);
    }
}

void VideoTransformManager::toggleInvert()
{
    int result = smartscope_video_toggle_invert();
    if (result == 0) {  // Success
        updateStatus();
        emit invertColorsChanged();
        emit transformApplied();
        LOG_DEBUG("VideoTransformManager", "Invert colors: ", m_invertColors ? "ON" : "OFF");
    } else {
        QString errorMsg = "Failed to toggle color invert";
        LOG_ERROR("VideoTransformManager", errorMsg.toStdString());
        emit error(errorMsg);
    }
}

void VideoTransformManager::resetAll()
{
    int result = smartscope_video_reset_transforms();
    if (result == 0) {  // Success
        // 同时重置畸变校正
        smartscope_set_distortion_correction(false);

        updateStatus();
        emit rotationChanged();
        emit flipHorizontalChanged();
        emit flipVerticalChanged();
        emit invertColorsChanged();
        emit distortionCorrectionChanged();
        emit transformApplied();
        LOG_INFO("VideoTransformManager", "All video transforms reset");
    } else {
        QString errorMsg = "Failed to reset transforms";
        LOG_ERROR("VideoTransformManager", errorMsg.toStdString());
        emit error(errorMsg);
    }
}

void VideoTransformManager::toggleDistortionCorrection()
{
    int result = smartscope_toggle_distortion_correction();
    if (result == 0) {  // Success
        updateStatus();
        emit distortionCorrectionChanged();
        emit transformApplied();
        LOG_INFO("VideoTransformManager", "Distortion correction: ", m_distortionCorrectionEnabled ? "ON" : "OFF");
    } else {
        QString errorMsg = "Failed to toggle distortion correction";
        LOG_ERROR("VideoTransformManager", errorMsg.toStdString());
        emit error(errorMsg);
    }
}

void VideoTransformManager::setDistortionCorrection(bool enabled)
{
    int result = smartscope_set_distortion_correction(enabled);
    if (result == 0) {  // Success
        updateStatus();
        emit distortionCorrectionChanged();
        emit transformApplied();
        LOG_DEBUG("VideoTransformManager", "Distortion correction set to: ", enabled ? "ON" : "OFF");
    } else {
        QString errorMsg = QString("Failed to set distortion correction to %1").arg(enabled ? "ON" : "OFF");
        LOG_ERROR("VideoTransformManager", errorMsg.toStdString());
        emit error(errorMsg);
    }
}

void VideoTransformManager::refreshStatus()
{
    updateStatus();
}

void VideoTransformManager::updateStatus()
{
    m_rotationDegrees = smartscope_video_get_rotation();
    m_flipHorizontal = smartscope_video_get_flip_horizontal();
    m_flipVertical = smartscope_video_get_flip_vertical();
    m_invertColors = smartscope_video_get_invert();
    m_distortionCorrectionEnabled = smartscope_get_distortion_correction();
}
