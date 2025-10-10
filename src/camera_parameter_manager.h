#ifndef CAMERA_PARAMETER_MANAGER_H
#define CAMERA_PARAMETER_MANAGER_H

#include <QObject>
#include <QVariantMap>

// C FFI declarations for camera parameters
extern "C" {
    enum CCameraProperty {
        Brightness = 0,
        Contrast = 1,
        Saturation = 2,
        Hue = 3,
        Gain = 4,
        Exposure = 5,
        Focus = 6,
        WhiteBalance = 7,
        FrameRate = 8,
        Resolution = 9,
        Gamma = 10,
        BacklightCompensation = 11,
        AutoExposure = 12,
        AutoWhiteBalance = 13
    };

    struct CCameraParameterRange {
        int32_t min;
        int32_t max;
        int32_t step;
        int32_t default_value;
        int32_t current;
    };

    int smartscope_set_left_camera_parameter(CCameraProperty property, int32_t value);
    int smartscope_set_right_camera_parameter(CCameraProperty property, int32_t value);
    int smartscope_set_single_camera_parameter(CCameraProperty property, int32_t value);

    int32_t smartscope_get_left_camera_parameter(CCameraProperty property);
    int32_t smartscope_get_right_camera_parameter(CCameraProperty property);
    int32_t smartscope_get_single_camera_parameter(CCameraProperty property);

    int smartscope_get_left_camera_parameter_range(CCameraProperty property, CCameraParameterRange* range_out);
    int smartscope_get_right_camera_parameter_range(CCameraProperty property, CCameraParameterRange* range_out);
    int smartscope_get_single_camera_parameter_range(CCameraProperty property, CCameraParameterRange* range_out);

    int smartscope_reset_left_camera_parameters();
    int smartscope_reset_right_camera_parameters();
    int smartscope_reset_single_camera_parameters();
}

/**
 * @brief 相机参数管理器 - QML接口
 *
 * 提供QML可调用的相机参数控制接口，所有业务逻辑在Rust中实现
 */
class CameraParameterManager : public QObject
{
    Q_OBJECT

public:
    explicit CameraParameterManager(QObject *parent = nullptr);
    ~CameraParameterManager();

    // QML可调用的方法

    /**
     * @brief 设置左相机参数
     * @param propertyId 参数ID (0-9, 对应CCameraProperty枚举)
     * @param value 参数值
     * @return 成功返回true
     */
    Q_INVOKABLE bool setLeftCameraParameter(int propertyId, int value);

    /**
     * @brief 设置右相机参数
     */
    Q_INVOKABLE bool setRightCameraParameter(int propertyId, int value);

    /**
     * @brief 设置单相机参数
     */
    Q_INVOKABLE bool setSingleCameraParameter(int propertyId, int value);

    /**
     * @brief 获取左相机参数当前值
     * @param propertyId 参数ID
     * @return 参数值，失败返回-1
     */
    Q_INVOKABLE int getLeftCameraParameter(int propertyId);

    /**
     * @brief 获取右相机参数当前值
     */
    Q_INVOKABLE int getRightCameraParameter(int propertyId);

    /**
     * @brief 获取单相机参数当前值
     */
    Q_INVOKABLE int getSingleCameraParameter(int propertyId);

    /**
     * @brief 获取左相机参数范围
     * @param propertyId 参数ID
     * @return QVariantMap包含: min, max, step, default_value, current
     */
    Q_INVOKABLE QVariantMap getLeftCameraParameterRange(int propertyId);

    /**
     * @brief 获取右相机参数范围
     */
    Q_INVOKABLE QVariantMap getRightCameraParameterRange(int propertyId);

    /**
     * @brief 获取单相机参数范围
     */
    Q_INVOKABLE QVariantMap getSingleCameraParameterRange(int propertyId);

    /**
     * @brief 重置左相机所有参数到默认值
     */
    Q_INVOKABLE bool resetLeftCameraParameters();

    /**
     * @brief 重置右相机所有参数到默认值
     */
    Q_INVOKABLE bool resetRightCameraParameters();

    /**
     * @brief 重置单相机所有参数到默认值
     */
    Q_INVOKABLE bool resetSingleCameraParameters();

signals:
    // 参数变化信号（可选，用于UI同步）
    void parameterChanged();

private:
    CCameraProperty toCCameraProperty(int propertyId);
};

#endif // CAMERA_PARAMETER_MANAGER_H
