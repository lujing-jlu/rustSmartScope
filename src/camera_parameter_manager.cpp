#include "camera_parameter_manager.h"
#include <QDebug>

CameraParameterManager::CameraParameterManager(QObject *parent)
    : QObject(parent)
{
    qDebug() << "CameraParameterManager initialized";
}

CameraParameterManager::~CameraParameterManager()
{
}

CCameraProperty CameraParameterManager::toCCameraProperty(int propertyId)
{
    return static_cast<CCameraProperty>(propertyId);
}

bool CameraParameterManager::setLeftCameraParameter(int propertyId, int value)
{
    CCameraProperty property = toCCameraProperty(propertyId);
    int result = smartscope_set_left_camera_parameter(property, value);

    if (result == 0) {
        qDebug() << "Set left camera parameter" << propertyId << "=" << value;
        emit parameterChanged();
        return true;
    } else {
        qDebug() << "Failed to set left camera parameter" << propertyId << ", error code:" << result;
        return false;
    }
}

bool CameraParameterManager::setRightCameraParameter(int propertyId, int value)
{
    CCameraProperty property = toCCameraProperty(propertyId);
    int result = smartscope_set_right_camera_parameter(property, value);

    if (result == 0) {
        qDebug() << "Set right camera parameter" << propertyId << "=" << value;
        emit parameterChanged();
        return true;
    } else {
        qDebug() << "Failed to set right camera parameter" << propertyId << ", error code:" << result;
        return false;
    }
}

bool CameraParameterManager::setSingleCameraParameter(int propertyId, int value)
{
    CCameraProperty property = toCCameraProperty(propertyId);
    int result = smartscope_set_single_camera_parameter(property, value);

    if (result == 0) {
        qDebug() << "Set single camera parameter" << propertyId << "=" << value;
        emit parameterChanged();
        return true;
    } else {
        qDebug() << "Failed to set single camera parameter" << propertyId << ", error code:" << result;
        return false;
    }
}

int CameraParameterManager::getLeftCameraParameter(int propertyId)
{
    CCameraProperty property = toCCameraProperty(propertyId);
    int32_t value = smartscope_get_left_camera_parameter(property);
    return value;
}

int CameraParameterManager::getRightCameraParameter(int propertyId)
{
    CCameraProperty property = toCCameraProperty(propertyId);
    int32_t value = smartscope_get_right_camera_parameter(property);
    return value;
}

int CameraParameterManager::getSingleCameraParameter(int propertyId)
{
    CCameraProperty property = toCCameraProperty(propertyId);
    int32_t value = smartscope_get_single_camera_parameter(property);
    qDebug() << "getSingleCameraParameter: propertyId=" << propertyId << " value=" << value;
    return value;
}

QVariantMap CameraParameterManager::getLeftCameraParameterRange(int propertyId)
{
    CCameraProperty property = toCCameraProperty(propertyId);
    CCameraParameterRange range;

    QVariantMap result;

    if (smartscope_get_left_camera_parameter_range(property, &range) == 0) {
        result["min"] = range.min;
        result["max"] = range.max;
        result["step"] = range.step;
        result["default_value"] = range.default_value;
        result["current"] = range.current;
        return result;
    } else {
        qDebug() << "Failed to get left camera parameter range for" << propertyId;
        // 返回空map或默认值
        result["min"] = 0;
        result["max"] = 0;
        result["step"] = 1;
        result["default_value"] = 0;
        result["current"] = 0;
        return result;
    }
}

QVariantMap CameraParameterManager::getRightCameraParameterRange(int propertyId)
{
    CCameraProperty property = toCCameraProperty(propertyId);
    CCameraParameterRange range;

    QVariantMap result;

    if (smartscope_get_right_camera_parameter_range(property, &range) == 0) {
        result["min"] = range.min;
        result["max"] = range.max;
        result["step"] = range.step;
        result["default_value"] = range.default_value;
        result["current"] = range.current;
        return result;
    } else {
        qDebug() << "Failed to get right camera parameter range for" << propertyId;
        result["min"] = 0;
        result["max"] = 0;
        result["step"] = 1;
        result["default_value"] = 0;
        result["current"] = 0;
        return result;
    }
}

QVariantMap CameraParameterManager::getSingleCameraParameterRange(int propertyId)
{
    CCameraProperty property = toCCameraProperty(propertyId);
    CCameraParameterRange range;

    QVariantMap result;

    if (smartscope_get_single_camera_parameter_range(property, &range) == 0) {
        result["min"] = range.min;
        result["max"] = range.max;
        result["step"] = range.step;
        result["default_value"] = range.default_value;
        result["current"] = range.current;
        return result;
    } else {
        qDebug() << "Failed to get single camera parameter range for" << propertyId;
        result["min"] = 0;
        result["max"] = 0;
        result["step"] = 1;
        result["default_value"] = 0;
        result["current"] = 0;
        return result;
    }
}

bool CameraParameterManager::resetLeftCameraParameters()
{
    int result = smartscope_reset_left_camera_parameters();

    if (result == 0) {
        qDebug() << "Reset left camera parameters to defaults";
        emit parameterChanged();
        return true;
    } else {
        qDebug() << "Failed to reset left camera parameters, error code:" << result;
        return false;
    }
}

bool CameraParameterManager::resetRightCameraParameters()
{
    int result = smartscope_reset_right_camera_parameters();

    if (result == 0) {
        qDebug() << "Reset right camera parameters to defaults";
        emit parameterChanged();
        return true;
    } else {
        qDebug() << "Failed to reset right camera parameters, error code:" << result;
        return false;
    }
}

bool CameraParameterManager::resetSingleCameraParameters()
{
    int result = smartscope_reset_single_camera_parameters();

    if (result == 0) {
        qDebug() << "Reset single camera parameters to defaults";
        emit parameterChanged();
        return true;
    } else {
        qDebug() << "Failed to reset single camera parameters, error code:" << result;
        return false;
    }
}
