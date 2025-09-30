#ifndef HOME_PAGE_H
#define HOME_PAGE_H

#include "app/ui/base_page.h"
#include <QLabel>
#include <QTimer>
#include <QMap>
#include <QSlider>
#include <QCheckBox>
#include <QMutex>
#include <atomic>
#include <opencv2/opencv.hpp>
#include "core/camera/multi_camera_manager.h"
#include "core/camera/camera_correction_manager.h"
#include "inference/yolov8_service.hpp"

// 前向声明
namespace SmartScope {
namespace App {
namespace Utils {
class ScreenshotManager;
}
}
class PathSelector;
}

/**
 * @brief 主页组件
 */
class HomePage : public BasePage
{
    Q_OBJECT
    Q_PROPERTY(QString currentWorkPath READ getCurrentWorkPath WRITE setCurrentWorkPath NOTIFY currentWorkPathChanged)
    Q_PROPERTY(bool objectDetectionEnabled READ isObjectDetectionEnabled NOTIFY objectDetectionEnabledChanged)

public:
    // 客户端ID常量
    static const std::string CLIENT_ID;
    
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit HomePage(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~HomePage() override;
    
    /**
     * @brief 获取当前工作路径
     * @return 当前工作路径
     */
    QString getCurrentWorkPath() const;
    
    /**
     * @brief 设置当前工作路径
     * @param path 工作路径
     */
    void setCurrentWorkPath(const QString &path);
    
    /**
     * @brief 获取左相机ID
     * @return 左相机ID
     */
    QString getLeftCameraId() const;
    
    /**
     * @brief 获取右相机ID
     * @return 右相机ID
     */
    QString getRightCameraId() const;
    
    /**
     * @brief 切换目标检测功能
     * @param enabled 是否启用目标检测
     */
    void toggleObjectDetection(bool enabled);
    
    /**
     * @brief 更新目标检测按钮状态
     * @param checked 是否选中
     */
    void updateDetectionButton(bool checked);

    /**
     * @brief 检查目标检测功能是否启用
     * @return 是否启用
     */
    bool isObjectDetectionEnabled() const;

protected:
    /**
     * @brief 处理通用事件（用于捏合手势缩放）
     */
    bool event(QEvent *event) override;
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
    
    /**
     * @brief 显示事件
     * @param event 事件
     */
    void showEvent(QShowEvent *event) override;
    
    /**
     * @brief 隐藏事件
     * @param event 事件
     */
    void hideEvent(QHideEvent *event) override;

signals:
    /**
     * @brief 当前工作路径变更信号
     * @param path 新的工作路径
     */
    void currentWorkPathChanged(const QString &path);
    
    /**
     * @brief 目标检测功能状态变更信号
     * @param enabled 是否启用
     */
    void objectDetectionEnabledChanged(bool enabled);

    /**
     * @brief 相机模式变更信号
     * @param isSingleCameraMode 是否为单相机模式
     */
    void cameraModeChanged(bool isSingleCameraMode);

private slots:
    /**
     * @brief 更新相机视图
     */
    void updateCameraViews();
    
    /**
     * @brief 恢复默认设置
     */
    void resetToDefaults();
    
    /**
     * @brief 应用设置
     */
    void applySettings();
    
    /**
     * @brief 加载当前相机参数
     */
    void loadCurrentSettings();
    
    /**
     * @brief 处理工作路径变更
     * @param path 新的工作路径
     */
    void onWorkPathChanged(const QString &path);
    
    /**
     * @brief 打开文件对话框
     */
    void openFileDialog();
    
    /**
     * @brief 截图并保存图像
     */
    void captureAndSaveImages();
    
    /**
     * @brief 接收到相机帧时的槽函数
     * @param cameraId 相机ID
     * @param frame 图像帧
     * @param timestamp 时间戳
     */
    void onFrameReceived(const std::string& cameraId, const cv::Mat& frame, int64_t timestamp);
    
    /**
     * @brief 处理YOLOv8检测完成信号
     * @param result 检测结果
     */
    void onDetectionCompleted(const SmartScope::YOLOv8Result& result);

private:
    /**
     * @brief 初始化相机
     */
    void initCameras();

    /**
     * @brief 获取相机支持的分辨率列表
     * @param cameraId 相机ID
     * @return 支持的分辨率列表
     */
    std::vector<cv::Size> getSupportedResolutions(const std::string& cameraId);
    
    /**
     * @brief 获取相机在指定分辨率下支持的帧率
     * @param cameraId 相机ID
     * @param resolution 分辨率
     * @return 支持的帧率列表
     */
    std::vector<double> getSupportedFrameRates(const std::string& cameraId, const cv::Size& resolution);
    
    /**
     * @brief 将OpenCV图像转换为QImage
     * @param mat OpenCV图像
     * @return QImage
     */
    QImage matToQImage(const cv::Mat& mat);
    
    /**
     * @brief 更新相机位置
     */
    void updateCameraPositions();
    
    /**
     * @brief 查找相机设备路径
     * @param cameraNames 相机名称列表
     * @return 相机设备路径
     */
    QString findCameraDevice(const QStringList& cameraNames);

    /**
     * @brief 获取所有可用的相机设备
     * @return 可用相机设备路径列表
     */
    QStringList getAllAvailableCameras();

    /**
     * @brief 智能检测相机模式并初始化
     */
    void smartCameraDetection();

    /**
     * @brief 初始化双相机模式
     */
    void initDualCameraMode();

    /**
     * @brief 初始化单相机模式
     */
    void initSingleCameraMode();
    
    /**
     * @brief 启用相机
     */
    void enableCameras();
    
    /**
     * @brief 禁用相机
     */
    void disableCameras();
    
    /**
     * @brief 创建调节面板
     */
    void createAdjustmentPanel();
    /**
     * @brief 创建RGA图像调整面板（简版）
     */
    void createRgaPanel();
    
    /**
     * @brief 初始化工具栏按钮
     */
    void initToolBarButtons();
    
    /**
     * @brief 创建标签
     * @param text 标签文本
     * @return 标签控件
     */
    QLabel* createLabel(const QString &text);
    
    /**
     * @brief 创建滑块
     * @param min 最小值
     * @param max 最大值
     * @param value 当前值
     * @return 滑块控件
     */
    QSlider* createSlider(int min, int max, int value);
    
    /**
     * @brief 应用参数到相机
     * @param cameraId 相机ID
     * @param params 参数映射
     */
    void applyParamsToCamera(const QString& cameraId, const QMap<QString, QString>& params);
    
    /**
     * @brief 创建调试边框
     */
    void createDebugBorders();
    
    /**
     * @brief 更新状态栏中的帧率显示
     * @param leftFps 左相机帧率
     * @param rightFps 右相机帧率
     */
    void updateStatusBarFps(float leftFps, float rightFps);
    
    /**
     * @brief 保存图像
     * @param image 图像
     * @param cameraName 相机名称
     * @return 是否保存成功
     */
    bool saveImage(const cv::Mat& image, const QString& cameraName);
    
    /**
     * @brief 提交帧进行检测
     * @param frame 图像帧
     * @param cameraId 相机ID
     */
    void submitFrameForDetection(const cv::Mat& frame, const std::string& cameraId);
    
    /**
     * @brief 开始下一次检测
     */
    void startNextDetection();
    
    /**
     * @brief 在图像上绘制检测结果
     * @param image 图像
     * @param detections 检测结果
     */
    void drawDetectionResults(cv::Mat& image, const std::vector<SmartScope::YOLOv8Result::Detection>& detections);
    
    /**
     * @brief 切换调节面板显示状态
     */
    void toggleAdjustmentPanel();
    /**
     * @brief 切换RGA图像调整面板显示状态
     */
    void toggleRgaPanel();
    
    /**
     * @brief 切换畸变校正开关
     */
    void toggleDistortionCorrection();
    
    /**
     * @brief 应用图像滤镜处理
     * @param image 输入图像
     * @param cameraId 相机ID
     * @return 处理后的图像
     */
    cv::Mat applyImageFilters(const cv::Mat& image, const std::string& cameraId);
    
    /**
     * @brief 调整画中画位置和大小
     * @param position 画中画位置
     * @param size 画中画大小
     */
    void adjustPipView(const QPoint& position, const QSize& size);

    // 相机视图
    QLabel* m_leftCameraView;                    ///< 左相机视图
    QLabel* m_rightCameraView;                   ///< 右相机视图（用作画中画）
    
    // 相机帧
    cv::Mat m_leftFrame;                         ///< 左相机帧
    cv::Mat m_rightFrame;                        ///< 右相机帧
    int64_t m_leftFrameTimestamp;                ///< 左相机帧时间戳
    int64_t m_rightFrameTimestamp;               ///< 右相机帧时间戳
    
    // 画中画拖动相关
    QPoint m_dragStartPosition;                  ///< 拖动起始位置
    
    // 相机相关
    bool m_camerasInitialized;                   ///< 相机是否已初始化
    QString m_leftCameraId;                      ///< 左相机ID
    QString m_rightCameraId;                     ///< 右相机ID
    QTimer* m_updateTimer;                       ///< 更新定时器
    
    // 路径相关
    SmartScope::PathSelector* m_pathSelector;    ///< 路径选择器
    QString m_currentWorkPath;                   ///< 当前工作路径
    
    // 截图管理器
    SmartScope::App::Utils::ScreenshotManager* m_screenshotManager; ///< 截图管理器

    // 拍照防抖相关
    QTimer* m_captureDebounceTimer;                  ///< 拍照防抖定时器
    bool m_isCapturing;                              ///< 是否正在拍照
    
    // 调节面板
    QWidget* m_adjustmentPanel;                  ///< 调节面板
    bool m_adjustmentPanelVisible;               ///< 调节面板是否可见
    QWidget* m_rgaPanel;                         ///< RGA图像调整面板（新）
    bool m_rgaPanelVisible;                      ///< RGA面板是否可见
    QMap<QString, QSlider*> m_sliders;           ///< 滑块映射
    QMap<QString, QCheckBox*> m_checkBoxes;      ///< 复选框映射
    
    // 相机模式相关
    enum class CameraMode {
        DUAL_CAMERA,    ///< 双相机模式
        SINGLE_CAMERA,  ///< 单相机模式
        NO_CAMERA       ///< 无相机模式
    };
    CameraMode m_cameraMode;                     ///< 当前相机模式

    // 目标检测相关
    bool m_detectionEnabled;                     ///< 是否启用目标检测
    bool m_detectionInProgress;                  ///< 是否正在进行检测
    cv::Mat m_lastDetectionFrame;                ///< 最后一帧检测图像
    std::string m_lastDetectionCameraId;         ///< 最后一帧检测相机ID
    std::atomic<bool> m_processingDetection;     ///< 是否正在处理检测
    std::chrono::steady_clock::time_point m_lastDetectionTime; ///< 上次检测时间
    QMutex m_detectionMutex;                     ///< 检测互斥锁
    std::vector<SmartScope::YOLOv8Result::Detection> m_lastDetectionResults; ///< 最后一次检测结果
    uint64_t m_lastDetectionSessionId;           ///< 最后一次检测会话ID
    float m_detectionConfidenceThreshold;        ///< 检测置信度阈值

    // 画面旋转与缩放
    int m_imageRotationDegrees = 0;              ///< 当前顺时针旋转角度：0/90/180/270
    double m_zoomScale = 1.0;                    ///< 无极缩放比例（捏合手势）
    bool m_forceFitOnce = false;                 ///< 下一帧强制按窗口完整显示
    double m_zoomScaleInitial = 1.0;             ///< 捏合开始时的缩放基准
    bool m_flipHorizontal = false;               ///< 水平翻转开关
    bool m_flipVertical = false;                 ///< 垂直翻转开关
    bool m_invertColors = false;                 ///< 反色开关

    // RGA面板-曝光控制
    bool m_autoExposureEnabledRga = true;        ///< RGA面板自动曝光开关
    int m_exposurePresetIndex = 0;               ///< RGA面板曝光预设索引
    
    // 相机校正相关
    std::shared_ptr<SmartScope::Core::CameraCorrectionManager> m_correctionManager; ///< 相机校正管理器
    bool m_distortionCorrectionEnabled;         ///< 畸变校正是否启用
};

#endif // HOME_PAGE_H 