#ifndef IMAGE_INTERACTION_MANAGER_H
#define IMAGE_INTERACTION_MANAGER_H

#include <QObject>
#include <QVector>
#include <QPoint>
#include <QVector3D>
#include <QPixmap>
#include <opencv2/opencv.hpp>
#include "app/ui/measurement_object.h"
#include "app/ui/measurement_state_manager.h"
#include "app/ui/clickable_image_label.h"
#include "app/measurement/measurement_calculator.h"
#include "app/ui/measurement_renderer.h"
#include "core/camera/camera_correction_manager.h"

namespace SmartScope {
namespace App {
namespace Ui {

/**
 * @brief 图像交互管理器，处理图像上的点击交互和测量功能
 */
class ImageInteractionManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit ImageInteractionManager(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~ImageInteractionManager();
    
    /**
     * @brief 初始化管理器
     * @param imageLabel 图像标签
     * @param stateManager 测量状态管理器
     * @param measurementManager 测量管理器
     * @param measurementRenderer 测量渲染器
     * @param measurementCalculator 测量计算器
     * @param calibrationHelper 校准助手
     * @return 初始化是否成功
     */
    bool initialize(ClickableImageLabel* imageLabel,
                    MeasurementStateManager* stateManager,
                    MeasurementManager* measurementManager,
                    MeasurementRenderer* measurementRenderer,
                    SmartScope::App::Measurement::MeasurementCalculator* measurementCalculator,
                    std::shared_ptr<SmartScope::Core::CameraCorrectionManager> correctionManager);
    
    /**
     * @brief 处理图像点击事件
     * @param imageX 图像X坐标
     * @param imageY 图像Y坐标
     * @param labelPoint 标签上的点坐标
     * @param depthMap 深度图
     * @param pointCloudPixelCoords 点云像素坐标
     * @param findNearestPointFunc 查找最近点的函数
     */
    void handleImageClick(int imageX, int imageY, const QPoint& labelPoint, 
                         const cv::Mat& depthMap,
                         const std::vector<cv::Point2i>& pointCloudPixelCoords,
                         std::function<QVector3D(int, int, int)> findNearestPointFunc);
    
    /**
     * @brief 重绘测量对象
     * @param baseImage 基础图像
     * @param originalImageSize 原始图像尺寸
     * @return 包含测量绘制的图像
     */
    cv::Mat redrawMeasurements(const cv::Mat& baseImage, const QSize& originalImageSize);
    
    /**
     * @brief 绘制临时测量
     * @param image 要绘制的图像
     */
    void drawTemporaryMeasurement(cv::Mat& image);
    
    /**
     * @brief 清除当前测量点
     */
    void clearCurrentMeasurementPoints();
    
    /**
     * @brief 获取当前测量点列表
     * @return 3D测量点列表
     */
    const QVector<QVector3D>& getMeasurementPoints() const { return m_measurementPoints; }
    
    /**
     * @brief 获取原始点击点列表
     * @return 2D点击点列表
     */
    const QVector<QPoint>& getOriginalClickPoints() const { return m_originalClickPoints; }

    /**
     * @brief 获取当前测量点列表
     * @return 3D测量点列表
     */
    const QVector<QVector3D>& getCurrentMeasurementPoints() const { return m_measurementPoints; }

    /**
     * @brief 获取缺失面积测量的多边形点（交点+后续点）
     * @return 多边形3D点列表
     */
    const QVector<QVector3D>& getMissingAreaPolygonPoints() const { return m_polygonPoints; }

    /**
     * @brief 获取缺失面积测量的多边形点对应的2D点击点
     * @return 多边形2D点击点列表
     */
    const QVector<QPoint>& getMissingAreaPolygonClickPoints() const { return m_polygonClickPoints; }

    /**
     * @brief 检查缺失面积测量是否已计算出交点
     * @return 是否有交点
     */
    bool hasMissingAreaIntersection() const { return m_hasIntersection; }
    
    /**
     * @brief 设置显示图像
     * @param displayImage 要显示的图像
     */
    void setDisplayImage(const cv::Mat& displayImage);
    
    // 设置裁剪ROI（进入3D测量时的中心3:4裁剪），用于修正内参主点
    void setCropROI(const cv::Rect& roi) { m_cropROI = roi; }

    /**
     * @brief 获取显示图像
     * @return 当前显示的图像
     */
    const cv::Mat& getDisplayImage() const { return m_displayImage; }

    /**
     * @brief 清除所有临时点
     */
    void clearTemporaryPoints();

signals:
    /**
     * @brief 测量完成信号
     * @param measurement 完成的测量对象
     */
    void measurementCompleted(MeasurementObject* measurement);
    
    /**
     * @brief 更新UI的信号
     */
    void updateUI();
    
    /**
     * @brief 显示消息提示信号
     * @param message 消息内容
     * @param duration 显示时长(ms)
     */
    void showToastMessage(const QString& message, int duration = 2000);

private:
    /**
     * @brief 处理长度测量
     * @param pcPointMeters 3D点坐标(米)
     * @param currentClickPoint 当前点击的2D坐标
     */
    void handleLengthMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint);
    
    /**
     * @brief 处理点到线测量
     * @param pcPointMeters 3D点坐标(米)
     * @param currentClickPoint 当前点击的2D坐标
     */
    void handlePointToLineMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint);
    
    /**
     * @brief 处理深度测量
     * @param pcPointMeters 3D点坐标(米)
     * @param currentClickPoint 当前点击的2D坐标
     */
    void handleDepthMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint);
    
    /**
     * @brief 处理剖面测量
     * @param pcPointMeters 3D点坐标(米)
     * @param currentClickPoint 当前点击的2D坐标
     */
    void handleProfileMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint);
    
    /**
     * @brief 处理区域测量
     * @param pcPointMeters 3D点坐标(米)
     * @param currentClickPoint 当前点击的2D坐标
     */
    void handleAreaMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint);
    
    /**
     * @brief 处理折线测量
     * @param pcPointMeters 3D点坐标(米)
     * @param currentClickPoint 当前点击的2D坐标
     */
    void handlePolylineMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint);
    
    /**
     * @brief 处理补缺测量
     * @param pcPointMeters 3D点坐标(米)
     * @param currentClickPoint 当前点击的2D坐标
     */
    void handleMissingAreaMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint);

    /**
     * @brief 计算2D线段交点
     * @param line1P1 第一条线段的起点
     * @param line1P2 第一条线段的终点
     * @param line2P1 第二条线段的起点
     * @param line2P2 第二条线段的终点
     * @return 交点坐标
     */
    QPoint calculate2DIntersection(const QPoint& line1P1, const QPoint& line1P2,
                                  const QPoint& line2P1, const QPoint& line2P2);

private:
    ClickableImageLabel* m_imageLabel;            ///< 图像标签控件
    MeasurementStateManager* m_stateManager;      ///< 测量状态管理器
    MeasurementManager* m_measurementManager;     ///< 测量管理器
    MeasurementRenderer* m_measurementRenderer;   ///< 测量渲染器
    SmartScope::App::Measurement::MeasurementCalculator* m_measurementCalculator; ///< 测量计算器
    std::shared_ptr<SmartScope::Core::CameraCorrectionManager> m_correctionManager;   ///< 相机校正管理器
    
    QVector<QPoint> m_originalClickPoints;        ///< 记录当前测量在2D图像上的原始点击点
    QVector<QVector3D> m_measurementPoints;       ///< 临时存储当前测量的3D点
    cv::Mat m_displayImage;                       ///< 当前显示的图像

    // 记录进入3:4裁剪时的ROI（供主点修正使用）
    cv::Rect m_cropROI;                           ///< (x,y,w,h)

    // 缺失面积测量专用变量
    QVector<QVector3D> m_lineSegmentPoints;       ///< 缺失面积测量：线段点（前4个点）
    QVector<QPoint> m_lineSegmentClickPoints;     ///< 缺失面积测量：线段点对应的2D点击点
    QVector3D m_intersectionPoint;                ///< 缺失面积测量：计算得到的交点
    QVector<QVector3D> m_polygonPoints;           ///< 缺失面积测量：多边形点（交点+后续点）
    QVector<QPoint> m_polygonClickPoints;         ///< 缺失面积测量：多边形点对应的2D点击点
    bool m_hasIntersection;                       ///< 缺失面积测量：是否已计算出交点
};

} // namespace Ui
} // namespace App
} // namespace SmartScope

#endif // IMAGE_INTERACTION_MANAGER_H 