#pragma once

#include <QVector>
#include <QVector3D>
#include <QPointF>
#include <QSize>
#include <opencv2/opencv.hpp>
#include "app/ui/measurement_object.h"
#include "core/camera/camera_correction_manager.h"

namespace SmartScope {
namespace App {
namespace Measurement {

class MeasurementCalculator
{
public:
    /**
     * @brief 构造函数
     */
    MeasurementCalculator();
    
    /**
     * @brief 析构函数
     */
    ~MeasurementCalculator();

    /**
     * @brief 计算测量对象的结果
     * @param measurement 测量对象
     */
    void calculateMeasurementResult(MeasurementObject* measurement);

    /**
     * @brief 将图像坐标转换为点云坐标
     * @param x 图像X坐标
     * @param y 图像Y坐标
     * @param depthMap 深度图
     * @param cameraMatrix 相机内参矩阵
     * @param originalImageSize 原始图像尺寸
     * @return 点云中的3D坐标（毫米）
     */
    QVector3D imageToPointCloudCoordinates(
        int x, int y, 
        const cv::Mat& depthMap, 
        const cv::Mat& cameraMatrix,
        const QSize& originalImageSize);

    /**
     * @brief 在点云中查找最接近指定像素的3D点
     * @param pixelX 像素X坐标
     * @param pixelY 像素Y坐标 
     * @param pointCloud 点云数据
     * @param pixelCoords 点云中点对应的像素坐标
     * @param cloudCenter 点云中心点
     * @param searchRadius 搜索半径（像素）
     * @return 点云中的3D点（米单位）
     */
    QVector3D findNearestPointInCloud(
        int pixelX, int pixelY,
        const std::vector<QVector3D>& pointCloud,
        const std::vector<cv::Point2i>& pixelCoords,
        const QVector3D& cloudCenter,
        int searchRadius = 10);

    /**
     * @brief 计算轮廓测量数据
     * @param measurement 轮廓测量对象 (必须包含2个原始点击点)
     * @param depthMap 用于查找深度值的深度图 (CV_32F)
     * @param originalImageSize 点击发生时的原始图像尺寸 (用于坐标缩放)
     * @param calibrationHelper 立体校准助手
     * @return QVector<QPointF> 包含 (像素距离, 深度值mm) 的数据点列表
     */
    QVector<QPointF> calculateProfileData(
        const MeasurementObject* measurement,
        const cv::Mat& depthMap,
        const QSize& originalImageSize,
        std::shared_ptr<SmartScope::Core::CameraCorrectionManager> correctionManager
    );
    
    /**
     * @brief 计算3D空间中两条线段延长线的交点
     * @param line1Point1 第一条线的起点
     * @param line1Point2 第一条线的终点
     * @param line2Point1 第二条线的起点
     * @param line2Point2 第二条线的终点
     * @param intersectionPoint 如果找到交点，将设置为交点坐标
     * @return bool 是否成功找到交点
     * 
     * 注意：在3D空间中，两条线通常不会精确相交，这个方法找到的是最接近的点
     */
    bool calculateLinesIntersection(
        const QVector3D& line1Point1,
        const QVector3D& line1Point2,
        const QVector3D& line2Point1,
        const QVector3D& line2Point2,
        QVector3D& intersectionPoint
    );
    
    /**
     * @brief 获取最新的深度图
     * @return 最新的深度图 (CV_32F)
     */
    cv::Mat getLatestDepthMap() const;
    
    /**
     * @brief 设置最新的深度图
     * @param depthMap 深度图 (CV_32F)
     */
    void setLatestDepthMap(const cv::Mat& depthMap);

private:
    cv::Mat m_latestDepthMap;  // 存储最新的深度图
};

} // namespace Measurement
} // namespace App
} // namespace SmartScope 