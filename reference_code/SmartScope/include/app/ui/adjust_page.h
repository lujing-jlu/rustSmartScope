#ifndef ADJUST_PAGE_H
#define ADJUST_PAGE_H

#include "app/ui/base_page.h"
#include "core/camera/multi_camera_manager.h"
#include <QTimer>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>
#include <QFormLayout>
#include <opencv2/opencv.hpp>
#include <memory>

/**
 * @brief 相机参数调节页面
 */
class AdjustPage : public BasePage
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit AdjustPage(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~AdjustPage();

protected:
    /**
     * @brief 初始化内容区域
     */
    void initContent() override;
    
    /**
     * @brief 事件过滤器
     * @param obj 对象
     * @param event 事件
     * @return 是否处理事件
     */
    bool eventFilter(QObject *obj, QEvent *event) override;
    
    /**
     * @brief 调整大小事件
     * @param event 事件
     */
    void resizeEvent(QResizeEvent *event) override;

private slots:
    /**
     * @brief 亮度调节改变
     * @param value 亮度值
     */
    void onBrightnessChanged(int value);
    
    /**
     * @brief 对比度调节改变
     * @param value 对比度值
     */
    void onContrastChanged(int value);
    
    /**
     * @brief 饱和度调节改变
     * @param value 饱和度值
     */
    void onSaturationChanged(int value);
    
    /**
     * @brief 曝光调节改变
     * @param value 曝光值
     */
    void onExposureChanged(int value);
    
    /**
     * @brief 白平衡调节改变
     * @param value 白平衡值
     */
    void onWhiteBalanceChanged(int value);
    
    /**
     * @brief 自动白平衡状态改变
     * @param state 复选框状态
     */
    void onAutoWhiteBalanceChanged(int state);
    
    /**
     * @brief 自动曝光状态改变
     * @param state 复选框状态
     */
    void onAutoExposureChanged(int state);
    
    /**
     * @brief 应用参数到相机
     */
    void applySettings();
    
    /**
     * @brief 重置参数到默认值
     */
    void resetSettings();
    
    /**
     * @brief 保存当前参数
     */
    void saveSettings();
    
    /**
     * @brief 加载保存的参数
     */
    void loadSettings();
    
    /**
     * @brief 切换调节面板显示状态
     */
    void toggleAdjustPanel();

private:
    /**
     * @brief 初始化相机
     */
    void initCameras();
    
    /**
     * @brief 初始化调节面板
     */
    void initAdjustPanel();
    
    /**
     * @brief 将OpenCV图像转换为QImage
     * @param mat OpenCV图像
     * @return QImage
     */
    QImage matToQImage(const cv::Mat &mat);
    
    /**
     * @brief 更新相机视图
     */
    void updateCameraViews();
    
    /**
     * @brief 更新相机位置
     */
    void updateCameraPositions();
    
    /**
     * @brief 应用图像处理效果
     * @param src 源图像
     * @param dst 目标图像
     */
    void applyImageProcessing(const cv::Mat &src, cv::Mat &dst);
    
    /**
     * @brief 更新相机参数
     * @param cameraId 相机ID
     */
    void updateCameraParams(const std::string &cameraId);
    
    SmartScope::Core::CameraUtils::MultiCameraManager m_cameraManager; ///< 相机管理器
    QLabel *m_leftCameraLabel;                                    ///< 左相机显示标签
    QLabel *m_rightCameraLabel;                                   ///< 右相机显示标签
    QWidget *m_adjustPanel;                                       ///< 调节面板
    QPushButton *m_togglePanelButton;                             ///< 切换面板按钮
    
    // 相机参数控件
    QSlider *m_brightnessSlider;                                  ///< 亮度滑块
    QSlider *m_contrastSlider;                                    ///< 对比度滑块
    QSlider *m_saturationSlider;                                  ///< 饱和度滑块
    QSlider *m_exposureSlider;                                    ///< 曝光滑块
    QSlider *m_whiteBalanceSlider;                                ///< 白平衡滑块
    QCheckBox *m_autoWhiteBalanceCheckBox;                        ///< 自动白平衡复选框
    QCheckBox *m_autoExposureCheckBox;                            ///< 自动曝光复选框
    QComboBox *m_resolutionComboBox;                              ///< 分辨率下拉框
    QSpinBox *m_fpsSpinBox;                                       ///< 帧率微调框
    
    // 相机参数值
    int m_brightness;                                             ///< 亮度值
    int m_contrast;                                               ///< 对比度值
    int m_saturation;                                             ///< 饱和度值
    int m_exposure;                                               ///< 曝光值
    int m_whiteBalance;                                           ///< 白平衡值
    bool m_autoWhiteBalance;                                      ///< 自动白平衡
    bool m_autoExposure;                                          ///< 自动曝光
    
    QString m_leftCameraId;                                       ///< 左相机ID
    QString m_rightCameraId;                                      ///< 右相机ID
    bool m_camerasInitialized;                                    ///< 相机是否已初始化
    bool m_adjustPanelVisible;                                    ///< 调节面板是否可见
};

#endif // ADJUST_PAGE_H 