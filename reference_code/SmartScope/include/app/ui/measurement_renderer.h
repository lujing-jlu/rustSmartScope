#pragma once

#include "app/ui/measurement_object.h"
#include "core/camera/camera_correction_manager.h"
#include <opencv2/opencv.hpp>
#include <QVector>
#include <QPoint>
#include <QVector3D>
#include <QSize>
#include <opencv2/freetype.hpp>

// -- 添加前向声明 --
class PointCloudGLWidget; 
// -- 结束添加 --

namespace SmartScope {
namespace App {
namespace Ui {

/**
 * @brief 负责在图像上绘制测量对象
 */
class MeasurementRenderer {
public:
    MeasurementRenderer();
    ~MeasurementRenderer();

    /**
     * @brief 在给定的基础图像上绘制所有可见的测量对象
     * @param baseImage 要在其上绘制的基础图像 (cv::Mat)
     * @param measurements 测量对象列表
     * @param calibrationHelper 包含相机参数的标定助手
     * @param originalImageSize 原始图像的尺寸 (用于缩放计算)
     * @return 绘制了测量对象的图像副本 (cv::Mat)
     */
    cv::Mat drawMeasurements(const cv::Mat& baseImage,
                             const QVector<MeasurementObject*>& measurements,
                             std::shared_ptr<SmartScope::Core::CameraCorrectionManager> correctionManager,
                             const QSize& originalImageSize);

    /**
     * @brief 在图像上绘制临时的测量点和可能的连线（例如，正在进行的长度测量）
     * @param image 要绘制的图像 (cv::Mat, 会被直接修改)
     * @param originalClickPoints 原始点击的2D图像坐标
     * @param measurementPoints 对应的3D测量点 (毫米单位, 当前未使用，但保留以备将来使用)
     * @param type 当前正在进行的测量类型
     */
    void drawTemporaryMeasurement(cv::Mat& image,
                                  const QVector<QPoint>& originalClickPoints,
                                  const QVector<QVector3D>& measurementPoints,
                                  MeasurementType type);

    /**
     * @brief 绘制折线测量
     * @param image 要绘制到的图像
     * @param measurement 测量对象
     */
    void drawPolylineFromClickPoints(
        cv::Mat& image,
        MeasurementObject* measurement);
        
    /**
     * @brief 绘制补缺测量
     * @param image 要绘制到的图像
     * @param measurement 测量对象
     */
    void drawMissingAreaFromClickPoints(
        cv::Mat& image,
        const MeasurementObject* measurement);

    /**
     * @brief 绘制剖面测量
     * @param image 要绘制到的图像
     */
    void drawProfileFromClickPoints(cv::Mat& image, const MeasurementObject* measurement);

    /**
     * @brief 绘制基于原始点击点的长度测量对象
     * @param image 要绘制的图像 (cv::Mat, 会被直接修改)
     * @param measurement 测量对象
     */
    void drawLengthFromClickPoints(cv::Mat& image, const MeasurementObject* measurement);

    /**
     * @brief 绘制基于原始点击点的点到线测量对象
     * @param image 要绘制的图像 (cv::Mat, 会被直接修改)
     * @param measurement 测量对象
     */
    void drawPointToLineFromClickPoints(cv::Mat& image, const MeasurementObject* measurement);
    
    /**
     * @brief 绘制基于原始点击点的面积测量对象
     * @param image 要绘制的图像 (cv::Mat, 会被直接修改)
     * @param measurement 测量对象 (包含至少3个点)
     */
    void drawAreaFromClickPoints(cv::Mat& image, const MeasurementObject* measurement);

    /**
     * @brief 绘制深度测量的 2D 可视化元素（点、线、标签）
     * @param image 要绘制的图像 (cv::Mat, 会被直接修改)
     * @param measurement 测量对象 (包含4个点)
     * @param projectionPoint2D P4 在 P1-P2-P3 平面上的 2D 投影点
     */
    void drawDepthMeasurementVisuals(cv::Mat& image, 
                                     const MeasurementObject* measurement, 
                                     const QPoint& projectionPoint2D);

    /**
     * @brief 在图像上绘制距离标签
     * @param image 要绘制的图像 (cv::Mat, 会被直接修改)
     * @param position 标签的位置 (通常是线的中点或端点附近)
     * @param text 要显示的文本
     */
    void drawDistanceLabel(cv::Mat& image, const cv::Point& position, const QString& text);

    /**
     * @brief 辅助函数：绘制单个点和标签
     * @param image 要绘制的图像 (cv::Mat, 会被直接修改)
     * @param point 点的 cv::Point 坐标
     * @param label 点的标签 (如 "P1")
     */
    void drawPoint(cv::Mat& image, const cv::Point& point, const std::string& label);
    
    /**
     * @brief 辅助函数：转换 QPoint 到 cv::Point 并做边界检查
     * @param qPoint Qt 点
     * @param maxWidth 图像宽度
     * @param maxHeight 图像高度
     * @return OpenCV 点
     */
    cv::Point toCvPoint(const QPoint& qPoint, int maxWidth, int maxHeight);

    /**
     * @brief 辅助函数：绘制虚线
     * @param image 要绘制的图像
     * @param p1 起始点
     * @param p2 结束点
     * @param color 颜色
     * @param thickness 线宽
     * @param dashLength 虚线段长度
     * @param gapLength 间隔长度
     */
    void drawDashedLine(cv::Mat& image, const cv::Point& p1, const cv::Point& p2,
                        const cv::Scalar& color, int thickness, int dashLength, int gapLength);

    /**
     * @brief 辅助函数：在指定位置绘制带有半透明背景的文本
     * @param image 要绘制的图像
     * @param text 要绘制的文本 (QString)
     * @param position 文本的中心位置 (cv::Point)
     * @param textColor 文本颜色
     * @param fontScale 字体大小比例
     * @param thickness 文本线宽
     * @param bgColor 背景颜色 (例如 cv::Scalar(0, 0, 0, 128) for semi-transparent black)
     */
    void drawTextWithBackground(cv::Mat& image,
                                const QString& text,
                                const cv::Point& position,
                                const cv::Scalar& textColor,
                                double fontScale,
                                int thickness,
                                const cv::Scalar& bgColor);

    /**
     * @brief 辅助函数：将 QColor 转换为 cv::Scalar (BGR)
     * @param color Qt 颜色
     * @return OpenCV 颜色 (Scalar)
     */
    cv::Scalar toCvColor(const QColor& color);

    cv::Ptr<cv::freetype::FreeType2> ft2;

    // --- Drawing Constants ---
    static constexpr int DEFAULT_THICKNESS = 2;
    static constexpr int SELECTED_THICKNESS = 4;
    static constexpr int MARKER_RADIUS = 10;
    static constexpr double TEXT_FONT_SCALE = 0.7;
    static constexpr int TEXT_THICKNESS = 2;
    static const cv::Scalar DEFAULT_TEXT_COLOR;

    /**
     * @brief 计算线段与图像边界的交点
     * @param start 线段起点
     * @param end 线段终点
     * @param width 图像宽度
     * @param height 图像高度
     * @param outPoint 输出参数：与图像边界的交点
     * @return 是否找到有效交点
     */
    bool clipLineToImage(const cv::Point2f& start, const cv::Point2f& end, int width, int height, cv::Point& outPoint);
};

} // namespace Ui
} // namespace App
} // namespace SmartScope 