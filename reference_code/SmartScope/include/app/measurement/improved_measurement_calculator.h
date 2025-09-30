#pragma once

#include <QVector>
#include <QVector3D>
#include <QPointF>
#include <QSize>
#include <opencv2/opencv.hpp>
#include "app/ui/measurement_object.h"

namespace SmartScope {
namespace App {
namespace Measurement {

/**
 * @brief 改进的测量计算器
 * 
 * 提供更精确的测量计算，包括：
 * 1. 改进的坐标转换算法
 * 2. 更严格的测量验证
 * 3. 异常值检测和处理
 * 4. 详细的测量日志
 */
class ImprovedMeasurementCalculator {
public:
    /**
     * @brief 构造函数
     */
    ImprovedMeasurementCalculator();
    
    /**
     * @brief 析构函数
     */
    ~ImprovedMeasurementCalculator();

    /**
     * @brief 将图像坐标转换为点云坐标（改进版）
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
     * @brief 计算深度测量（改进版）
     * @param points 测量点（需要4个点：3个平面点 + 1个测量点）
     * @return 测量结果字符串
     */
    QString calculateDepthMeasurement(const QVector<QVector3D>& points);

    /**
     * @brief 计算长度测量（改进版）
     * @param points 测量点（需要2个点）
     * @return 测量结果字符串
     */
    QString calculateLengthMeasurement(const QVector<QVector3D>& points);

    /**
     * @brief 计算点到线距离测量（改进版）
     * @param points 测量点（需要3个点：1个测量点 + 2个线段端点）
     * @return 测量结果字符串
     */
    QString calculatePointToLineMeasurement(const QVector<QVector3D>& points);

    /**
     * @brief 计算面积测量（改进版）
     * @param points 测量点（需要至少3个点）
     * @return 测量结果字符串
     */
    QString calculateAreaMeasurement(const QVector<QVector3D>& points);

    /**
     * @brief 验证测量点的有效性
     * @param points 测量点
     * @param type 测量类型
     * @return 是否有效
     */
    bool validateMeasurementPoints(const QVector<QVector3D>& points, MeasurementType type);

    /**
     * @brief 记录测量详情
     * @param points 测量点
     * @param type 测量类型
     * @param result 测量结果
     */
    void logMeasurementDetails(const QVector<QVector3D>& points, MeasurementType type, const QString& result);

private:
    /**
     * @brief 在附近区域查找有效深度值
     * @param depthMap 深度图
     * @param centerX 中心X坐标
     * @param centerY 中心Y坐标
     * @param searchRadius 搜索半径
     * @return 找到的深度值
     */
    float findNearestValidDepth(const cv::Mat& depthMap, int centerX, int centerY, int searchRadius);
};

} // namespace Measurement
} // namespace App
} // namespace SmartScope
