#ifndef MEASUREMENT_PAGE_H
#define MEASUREMENT_PAGE_H

#include "app/ui/base_page.h"
#include "core/camera/multi_camera_manager.h"
#include "inference/inference_service.hpp"
#include "app/ui/point_cloud_gl_widget.h"
#include "app/ui/measurement_object.h"
#include "app/ui/measurement_state_manager.h"
#include "app/ui/measurement_type_selection_page.h"
#include "app/ui/measurement_menu.h"
#include "app/ui/clickable_image_label.h"
#include "core/camera/camera_correction_manager.h"
#include "app/ui/magnifier_manager.h"
#include "app/image/image_processor.h"
#include "app/measurement/measurement_calculator.h"
#include "app/ui/point_cloud_renderer.h"
#include "app/ui/measurement_renderer.h"
// #include "app/measurement/point_cloud_generator.h" // removed
#include "app/ui/profile_chart_manager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QImage>
#include <QTimer>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QColor>
#include <opencv2/opencv.hpp>
#include <QtWidgets>
#include "app/ui/measurement_delete_dialog.h"
#include "app/ui/utils/dialog_utils.h"
#include "app/ui/profile_chart_dialog.h"
#include "qcustomplot.h"
#include "app/utils/screenshot_manager.h"

// Forward declarations
namespace SmartScope::App::Ui {
class MeasurementDeleteDialog;
class ProfileChartDialog;
class ProfileChartManager;
class ImageInteractionManager;
class DebugPage;
} // namespace SmartScope::App::Ui

namespace SmartScope {
namespace App {
namespace Measurement {
class ScreenshotManager;
}
}
}

// --- Enum Definition START ---
enum class MeasurementState {
    Idle,       // 空闲状态
    Ready,      // 图像就绪
    Processing, // 处理中
    Completed,  // 完成
    Error       // 错误
};
// --- Enum Definition END ---

// AspectRatioLabel类已移至 clickable_image_label.h

namespace SmartScope {
namespace App {
namespace Ui {
// 将 PointCloudRenderer 的声明移到这里可能导致重复定义，因为它已在 point_cloud_renderer.h 中声明
// class PointCloudRenderer;
}
}
}

/**
 * @brief 3D测量页面组件
 */
class MeasurementPage : public BasePage
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit MeasurementPage(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~MeasurementPage() override;

    // 设置页控制：公开包装，内部调用私有的 setDebugControlsEnabled
    void setDebugModeFromSettings(bool enabled);

    /**
     * @brief 设置从主页传递的相机图像
     * @param leftImage 左相机图像
     * @param rightImage 右相机图像
     * @return 是否成功处理图像
     */
    bool setImagesFromHomePage(const cv::Mat &leftImage, const cv::Mat &rightImage);

    /**
     * @brief 检查图像标签是否已初始化
     * @return 如果图像标签已初始化则返回true，否则返回false
     */
    bool areImagesLabelsInitialized() const;

    /**
     * @brief 获取左相机ID
     * @return 左相机ID
     */
    QString getLeftCameraId() const { return m_leftCameraId; }

    /**
     * @brief 获取右相机ID
     * @return 右相机ID
     */
    QString getRightCameraId() const { return m_rightCameraId; }

    /**
     * @brief 更新相机ID
     * 当相机配置变更时调用此方法更新ID
     */
    void updateCameraIds();

    /**
     * @brief 获取点云中与指定像素最接近的3D点
     * @param pixelX 像素X坐标
     * @param pixelY 像素Y坐标
     * @param searchRadius 搜索半径（像素）
     * @return 点云中的3D点（米单位）
     */
    QVector3D findNearestPointInCloud(int pixelX, int pixelY, int searchRadius = 5);

    QVector3D getBoundingBoxCenter() const { return m_boundingBoxCenter; }

    /**
     * @brief 获取指定索引处的点
     * @param index 点的索引
     * @return 点的3D坐标
     */
    QVector3D getPointAt(size_t index) const {
        if (index < m_points.size()) {
            return m_points[index];
        }
        return QVector3D(0, 0, 0);
    }
    
    // 添加临时点用于可视化
    void addTemporaryPoint(const QVector3D& point);
    // 清除所有临时点
    void clearTemporaryPoints();

    // 导航栏触发：在测量页上点击Home/Back时调用，内部弹出确认并处理清空或返回
    void invokeBackConfirmationFromNav();
 
    // 在隐藏时保留数据（用于从调试页面返回时不清空）
    void setPreserveOnHide(bool preserve) { m_preserveOnHide = preserve; }
    void setSkipClearOnNextShow(bool skip) { m_skipClearOnNextShow = skip; }

protected:
    /**
     * @brief 初始化内容区域
     */
    void initContent() override;
    
    /**
     * @brief 显示事件处理
     * @param event 显示事件
     */
    void showEvent(QShowEvent *event) override;
    
    /**
     * @brief 隐藏事件处理
     * @param event 隐藏事件
     */
    void hideEvent(QHideEvent *event) override;

    /**
     * @brief 调整大小事件处理
     * @param event 调整大小事件
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief 事件过滤器
     * @param watched 观察的对象
     * @param event 事件
     * @return 是否处理了事件
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    /**
     * @brief 捕获相机图像
     */
    void captureImages();
    
    /**
     * @brief 重置3D测量
     */
    void resetMeasurement();
    
    /**
     * @brief 开始3D测量
     */
    void startMeasurement();
    
    /**
     * @brief 导出3D模型
     */
    void exportModel();

    /**
     * @brief 处理推理结果
     * @param result 推理结果
     */
    void handleInferenceResult(const SmartScope::InferenceResult& result);

    /**
     * @brief 打开测量类型选择页面
     */
    void openMeasurementTypeSelection();
    
    /**
     * @brief 处理测量类型选择
     * @param type 选择的测量类型
     */
    void handleMeasurementTypeSelected(MeasurementType type);
    
    /**
     * @brief 取消测量类型选择
     */
    void handleMeasurementTypeSelectionCancelled();
    
    /**
     * @brief 处理测量状态变化
     * @param mode 新的测量模式
     */
    void handleMeasurementModeChanged(MeasurementMode mode);
    
    /**
     * @brief 取消当前测量操作
     */
    void cancelMeasurementOperation();
    
    /**
     * @brief 完成当前测量操作
     */
    void completeMeasurementOperation();

    // 图像点击事件处理
    void handleImageClicked(int imageX, int imageY, const QPoint& labelPoint);

    /**
     * @brief 处理来自点云控件的点击事件
     * @param point 点击的3D点坐标（世界坐标系，单位：米）
     * @param pixelCoords 点击的窗口像素坐标
     */

    /**
     * @brief 打开删除测量对象的对话框
     */
    void openDeleteMeasurementDialog();

    /**
     * @brief 处理从删除对话框发出的删除请求
     * @param obj 要删除的测量对象指针
     */
    void handleDeleteMeasurementRequested(MeasurementObject* obj);

    // 导航栏触发：在测量页上点击Home/Back时调用，内部弹出确认并处理清空或返回
    void handleIntelligentBack();

    void onScreenshot();
    // 新增：剖面曲线旋转
    void rotateProfileLeft();
    void rotateProfileRight();

private:
    // UI Components
    ClickableImageLabel *m_leftImageLabel = nullptr;
    QLabel *m_rightImageLabel = nullptr;
    QLabel *m_depthImageLabel = nullptr;
    QLabel *m_disparityImageLabel = nullptr;
    PointCloudGLWidget *m_pointCloudWidget = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    QSplitter *m_leftSplitter = nullptr;
    MeasurementMenuBar *m_menuBar = nullptr;
    QPushButton* m_addMeasurementButton = nullptr;
    MeasurementTypeSelectionPage* m_typeSelectionPage = nullptr;
    QWidget* m_resultsPanel = nullptr;
    QVBoxLayout* m_resultsLayout = nullptr;

    // 放大镜管理器
    MagnifierManager* m_magnifierManager = nullptr;
    
    // 图像交互管理器
    SmartScope::App::Ui::ImageInteractionManager* m_imageInteractionManager = nullptr;

    // 是否在隐藏时保留测量内容（避免清空测量/点云等），用于跳转到调试页面
    bool m_preserveOnHide = false;
    // 下次显示时是否跳过清理（从调试页返回时保留点云/测量绘制）
    bool m_skipClearOnNextShow = false;

    // --- Calibration Related Members Removed/Commented ---
    // cv::Mat m_cameraMatrixLeft, m_distCoeffsLeft; // Removed
    // cv::Mat m_cameraMatrixRight, m_distCoeffsRight; // Removed
    // cv::Mat m_rotationMatrix; // Removed
    // cv::Mat m_translationVector; // Removed
    // cv::Mat m_R1, m_R2, m_P1, m_P2, m_Q; // Removed
    // cv::Mat m_map1x, m_map1y; // Removed
    // cv::Mat m_map2x, m_map2y; // Removed
    // bool m_camerasParamsLoaded = false; // Removed
    // bool m_remapInitialized = false; // Removed
    // cv::Rect m_roi1, m_roi2; // Removed

    // --- Added Camera Correction Manager ---
    std::shared_ptr<SmartScope::Core::CameraCorrectionManager> m_correctionManager = nullptr;
    
    // --- 添加测量计算器 ---
    SmartScope::App::Measurement::MeasurementCalculator* m_measurementCalculator = nullptr;

    // Other Members
    MeasurementManager* m_measurementManager = nullptr;
    MeasurementStateManager* m_stateManager = nullptr;
    QString m_leftCameraId;
    QString m_rightCameraId;
    cv::Mat m_leftImage;   // 左相机图像（原始）
    cv::Mat m_rightImage;  // 右相机图像（原始）
    cv::Mat m_leftRectifiedImage;  // 校正后的左相机图像
    cv::Mat m_rightRectifiedImage; // 校正后的右相机图像
    cv::Mat m_depthMap;    // 深度图
    cv::Mat m_disparityMap; // 添加视差图数据
    cv::Mat m_monoDepthCalibrated; // 校准后的单目深度图
    cv::Mat m_monoDepthRaw; // 新增：单目原始深度缓存
    QSize m_originalImageSize; // 原始图像尺寸
    cv::Mat m_inferenceInputLeftImage;  // 推理服务使用的裁剪后图像
    cv::Mat m_displayImage; // 当前显示的图像缓存
    
    // Point Cloud Data
    std::vector<cv::Point2i> m_pointCloudPixelCoords; // Changed to std::vector for consistency with generator and other point data
    std::vector<QVector3D> m_points;              // Points for PointCloudGLWidget (Consider renaming/removing if redundant)
    std::vector<QVector3D> m_colors;              // Colors for PointCloudGLWidget (Added for clarity)
    QVector3D m_boundingBoxCenter = QVector3D(0,0,0); // 点云包围盒中心

    // Measurement Data
    QVector<QPoint> m_measurementPointsTemp; // 临时测量点（2D像素点，<X,Y>）
    QVector<QVector3D> m_measurementPoints; // 测量点（3D坐标）
    QVector<QPoint> m_originalClickPoints; // 原始点击点（2D像素点）

    // State Flags
    bool m_imagesReady = false;
    MeasurementState m_measurementState = MeasurementState::Idle; // 当前页面状态

    // Services
    SmartScope::InferenceService& m_inferenceService; // Use reference
    bool m_inferenceInitialized = false;

    // Client ID
    static const std::string CLIENT_ID; // 客户端ID，用于相机引用计数

    // Temporary points for visualization
    QVector<QVector3D> m_temporaryPoints;

    // PointCloudRenderer
    SmartScope::App::Ui::PointCloudRenderer* m_pointCloudRenderer = nullptr;

    // MeasurementRenderer
    SmartScope::App::Ui::MeasurementRenderer* m_measurementRenderer = nullptr;

    // MeasurementDeleteDialog
    SmartScope::App::Ui::MeasurementDeleteDialog* m_deleteDialog = nullptr;

    // 简化实现：不再使用 PointCloudGenerator
    // SmartScope::App::Measurement::PointCloudGenerator* m_pointCloudGenerator = nullptr;

    // 左区域比例，用于放大镜定位
    float m_leftAreaRatio = 0.3f;

    // 添加用于存储UI元素的指针
    QWidget* m_pointCloudContainer = nullptr; // <-- 添加：存储点云容器的指针

    // 添加图表对话框指针
    	QCustomPlot* m_profileChartPlot = nullptr; // New plot widget
	QPushButton* m_profileChartButton = nullptr; // 添加剖面图按钮成员变量
	// 新增：剖面旋转按钮
	QPushButton* m_profileRotateLeftButton = nullptr;
	QPushButton* m_profileRotateRightButton = nullptr;
	// 新增：剖面累计旋转角度（度）
	double m_profileRotationAngleDeg = 0.0;

    // 保存菜单栏中完成按钮的引用
    MeasurementMenuButton *m_finishButton = nullptr;
    // 调试按钮（仅在设置页启用调试模式时显示）
    MeasurementMenuButton *m_debugButton = nullptr;

    // 添加截图管理器
    SmartScope::App::Utils::ScreenshotManager* m_screenshotManager;

    // 坐标变换跟踪结构
    struct CoordinateTransform {
        QSize originalSize;    // 原始图像尺寸 (1280x720)
        QSize rectifiedSize;   // 立体校正后尺寸 (720x1280)
        QSize finalSize;       // 最终推理输入尺寸 (720x1080)
    } m_coordinateTransform;

    // 记录进入3D测量时的中心裁剪ROI（用于K的主点修正）
    cv::Rect m_cropROI; // (x,y,w,h)

    // 添加 ProfileChartManager 成员变量
    SmartScope::App::Ui::ProfileChartManager* m_profileChartManager = nullptr;

    // 添加 ProfileChartDialog 成员变量
    SmartScope::App::Ui::ProfileChartDialog* m_profileDialog = nullptr;

    // 深度模式相关成员变量
    SmartScope::InferenceService::DepthMode m_depthMode = SmartScope::InferenceService::DepthMode::STEREO_ONLY;
    // QPushButton* m_depthModeButton = nullptr;          ///< 深度模式切换按钮（已移除）

    QLabel* m_statusInfoLabel;                         ///< 状态信息标签
    QLabel* m_measurementInfoLabel;                    ///< 测量信息标签

    // 定义私有方法
    void createMenuBar();
    void initMeasurementFeatures();
    void initToolBarButtons();
    void updateLayout();
    void showToast(QWidget *parent, const QString &message, int duration = 2000);
    void performDepthInference(const cv::Mat& leftImage, const cv::Mat& rightImage);
    void generatePointCloud(const cv::Mat& depthMap, const cv::Mat& colors = cv::Mat());
    void updateMeasurementInfoText();
    void updateStatusInfo();
    void initMenuButtons();
    void updatePointCloudMeasurements();
    void updateUIBasedOnMeasurementState();
    void completeReset();
    void updateMeasurementState();
    bool undoLastOperation();
    bool pointsMatch(const QVector<QVector3D>& points1, const QVector<QVector3D>& points2);
    bool updateCloudPoints(const QVector<cv::Point3f>& pointCloud);
    void handleZoomControl(int direction);
    void updateMagnifierContent();
    QImage getScreenshotImage();
        void handleProfileButtonClick();
    
    // 深度模式相关方法
    void setDepthMode(SmartScope::InferenceService::DepthMode mode);
    SmartScope::InferenceService::DepthMode getDepthMode() const;
    void updateDepthModeUI();

    void redrawMeasurementsOnLabel();
    void updateMeasurementsFromManager();
    
    // 更新剖面图表控件可见性
    void updateProfileControlsVisibility(); // Declare new method
    // 根据设置页的调试模式开关，控制调试入口可见性
    void setDebugControlsEnabled(bool enabled);

    /**
     * @brief 更新剖面起伏测量结果
     * @param measurement 测量对象
     * @param profileData 旋转后的剖面数据
     */
    void updateProfileElevationResult(MeasurementObject* measurement, const QVector<QPointF>& profileData);

    // 相机校正管理器相关方法
    void initializeCorrectionManager();
    void onCorrectionCompleted(const SmartScope::Core::CorrectionResult& result);
    void onCorrectionError(const QString& errorMessage);

    // PLY文件读取方法
    // 简化实现：不再读取PLY
    // bool readPLYFile(const QString& filename, std::vector<QVector3D>& points, std::vector<QVector3D>& colors);

    // 私有方法 - UI 相关
    void initMenuBar();
    void initControls();
    void initResultsPanel();
    void initPointCloudWidget();
    void initProfileChart();
    
    // 私有方法 - 测量相关
    void updateMeasurementModeUI(MeasurementMode mode);
    void resetMeasurementState();
    
    // 私有方法 - 测量对象管理
    void setupMeasurementResultItem(QWidget* item, MeasurementObject* measurement);
    void deleteMeasurementObject(MeasurementObject* obj);
};

#endif // MEASUREMENT_PAGE_H 