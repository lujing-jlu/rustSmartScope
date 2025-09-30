#ifndef SMARTSCOPE_APP_MEASUREMENT_CALCULATOR_H
#define SMARTSCOPE_APP_MEASUREMENT_CALCULATOR_H

#include <QVector>
#include <QVector3D>
#include <QPointF>
#include <QSize>
#include <opencv2/core.hpp>
#include "app/measurement/measurement_object.h"
#include "core/camera/camera_correction_manager.h"

namespace SmartScope {
namespace App {
namespace Measurement {

class MeasurementCalculator
{
public:
    MeasurementCalculator();
    ~MeasurementCalculator();
    
    // 计算测量结果
    void calculateMeasurementResult(MeasurementObject* measurement);
    
    // 图像坐标转换为点云坐标
    QVector3D imageToPointCloudCoordinates(
        int x, int y, 
        const cv::Mat& depthMap, 
        const cv::Mat& cameraMatrix,
        const QSize& originalImageSize);
    
    // 从点云中查找最近的点
    QVector3D findNearestPointInCloud(
        int pixelX, int pixelY,
        const std::vector<QVector3D>& pointCloud,
        const std::vector<cv::Point2i>& pixelCoords,
        const QVector3D& cloudCenter,
        int searchRadius = 10);
    
    // 计算剖面数据
    QVector<QPointF> calculateProfileData(
        const MeasurementObject* measurement,
        const cv::Mat& depthMap,
        const QSize& originalImageSize,
        std::shared_ptr<SmartScope::Core::CameraCorrectionManager> correctionManager);
    
    // 计算两条线段的交点
    bool calculateLinesIntersection(
        const QVector3D& line1Point1,
        const QVector3D& line1Point2,
        const QVector3D& line2Point1,
        const QVector3D& line2Point2,
        QVector3D& intersectionPoint);
    
    // 设置剖面测量深度缩放因子
    void setProfileDepthScaleFactor(float factor) { m_profileDepthScaleFactor = factor; }
    
    // 获取剖面测量深度缩放因子
    float getProfileDepthScaleFactor() const { return m_profileDepthScaleFactor; }
    
private:
    // 剖面测量深度缩放因子 - 用于调整深度值与实际测量结果一致
    float m_profileDepthScaleFactor = 1.0f;
};

} // namespace Measurement
} // namespace App
} // namespace SmartScope

#endif // SMARTSCOPE_APP_MEASUREMENT_CALCULATOR_H 