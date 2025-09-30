#include "improved_measurement_calculator.h"
#include "infrastructure/logging/logger.h"
#include <limits>
#include <cmath>
#include <QDebug>

namespace SmartScope {
namespace App {
namespace Measurement {

ImprovedMeasurementCalculator::ImprovedMeasurementCalculator() {
    LOG_INFO("创建改进的测量计算器实例");
}

ImprovedMeasurementCalculator::~ImprovedMeasurementCalculator() {
    LOG_INFO("销毁改进的测量计算器实例");
}

QVector3D ImprovedMeasurementCalculator::imageToPointCloudCoordinates(
    int x, int y, 
    const cv::Mat& depthMap, 
    const cv::Mat& cameraMatrix,
    const QSize& originalImageSize) {
    
    // 检查输入参数
    if (depthMap.empty()) {
        LOG_ERROR("深度图为空，无法进行坐标转换");
        return QVector3D(0, 0, 0);
    }

    if (cameraMatrix.empty()) {
        LOG_ERROR("相机内参矩阵为空，无法进行坐标转换");
        return QVector3D(0, 0, 0);
    }

    // 检查坐标范围
    if (x < 0 || x >= depthMap.cols || y < 0 || y >= depthMap.rows) {
        LOG_ERROR(QString("坐标 (%1, %2) 超出深度图范围 (%3, %4)")
                 .arg(x).arg(y).arg(depthMap.cols).arg(depthMap.rows));
        return QVector3D(0, 0, 0);
    }

    // 获取相机内参
    double fx = cameraMatrix.at<double>(0, 0);
    double fy = cameraMatrix.at<double>(1, 1);
    double cx = cameraMatrix.at<double>(0, 2);
    double cy = cameraMatrix.at<double>(1, 2);

    // 获取深度值
    float Z = depthMap.at<float>(y, x);
    
    // 检查深度值有效性
    if (Z <= 0 || !std::isfinite(Z)) {
        // 尝试在附近区域查找有效深度值
        Z = findNearestValidDepth(depthMap, x, y, 5); // 5像素搜索半径
        if (Z <= 0) {
            LOG_WARNING(QString("坐标 (%1, %2) 处无有效深度值").arg(x).arg(y));
            return QVector3D(0, 0, 0);
        }
    }

    // 计算深度缩放因子（如果有图像缩放）
    float depthScaleFactor = 1.0f;
    if (originalImageSize.width() > 0 && depthMap.cols > 0) {
        depthScaleFactor = static_cast<float>(originalImageSize.width()) / static_cast<float>(depthMap.cols);
    }
    
    // 注意：这里不应用深度缩放，因为深度值应该已经是正确的物理单位
    // 之前的代码错误地应用了深度缩放，这可能导致深度值不准确
    
    // 计算3D坐标（毫米单位）
    double X = (x - cx) * Z / fx;
    double Y = (y - cy) * Z / fy;

    // 转换坐标系 (OpenCV -> 标准3D坐标系)
    // OpenCV: X向右，Y向下，Z向前
    // 标准3D: X向右，Y向上，Z向前
    QVector3D point3D(X, -Y, Z);
    
    LOG_DEBUG(QString("图像坐标 (%1, %2) 转换为3D坐标: (%3, %4, %5) mm")
        .arg(x).arg(y)
        .arg(point3D.x(), 0, 'f', 2)
        .arg(point3D.y(), 0, 'f', 2)
        .arg(point3D.z(), 0, 'f', 2));
    
    return point3D;
}

float ImprovedMeasurementCalculator::findNearestValidDepth(
    const cv::Mat& depthMap, int centerX, int centerY, int searchRadius) {
    
    float bestDepth = 0.0f;
    float minDistance = std::numeric_limits<float>::max();
    
    for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
        for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
            int x = centerX + dx;
            int y = centerY + dy;
            
            if (x < 0 || x >= depthMap.cols || y < 0 || y >= depthMap.rows) {
                continue;
            }
            
            float depth = depthMap.at<float>(y, x);
            if (depth > 0 && std::isfinite(depth)) {
                float distance = std::sqrt(dx * dx + dy * dy);
                if (distance < minDistance) {
                    minDistance = distance;
                    bestDepth = depth;
                }
            }
        }
    }
    
    return bestDepth;
}

QString ImprovedMeasurementCalculator::calculateDepthMeasurement(
    const QVector<QVector3D>& points) {
    
    if (points.size() < 4) {
        return "错误: 深度测量需要4个点（3个平面点 + 1个测量点）";
    }
    
    // 提取四个点
    QVector3D p1 = points[0]; // 平面点1
    QVector3D p2 = points[1]; // 平面点2
    QVector3D p3 = points[2]; // 平面点3
    QVector3D p4 = points[3]; // 测量点
    
    // 计算平面法向量
    QVector3D v1 = p2 - p1;
    QVector3D v2 = p3 - p1;
    QVector3D normal = QVector3D::crossProduct(v1, v2);
    
    // 检查三点是否共线
    float normalLength = normal.length();
    if (normalLength < 1e-6f) {
        return "错误: 平面点共线，无法定义平面";
    }
    
    // 单位化法向量
    normal.normalize();
    
    // 计算点到平面的距离
    QVector3D pointToPlane = p4 - p1;
    float distance = QVector3D::dotProduct(pointToPlane, normal);
    
    // 使用绝对值确保深度为正
    distance = std::fabs(distance);
    
    // 验证测量结果的合理性
    if (distance > 10000.0f) { // 超过10米的深度可能不合理
        LOG_WARNING(QString("深度测量结果异常: %1 mm").arg(distance, 0, 'f', 2));
        return QString("深度: %1 mm (异常)").arg(distance, 0, 'f', 2);
    }
    
    return QString("深度: %1 mm").arg(distance, 0, 'f', 2);
}

QString ImprovedMeasurementCalculator::calculateLengthMeasurement(
    const QVector<QVector3D>& points) {
    
    if (points.size() < 2) {
        return "错误: 长度测量需要2个点";
    }
    
    QVector3D p1 = points[0];
    QVector3D p2 = points[1];
    
    // 计算两点间距离
    float distance = (p2 - p1).length();
    
    // 验证测量结果的合理性
    if (distance > 10000.0f) { // 超过10米的长度可能不合理
        LOG_WARNING(QString("长度测量结果异常: %1 mm").arg(distance, 0, 'f', 2));
        return QString("长度: %1 mm (异常)").arg(distance, 0, 'f', 2);
    }
    
    return QString("长度: %1 mm").arg(distance, 0, 'f', 2);
}

QString ImprovedMeasurementCalculator::calculatePointToLineMeasurement(
    const QVector<QVector3D>& points) {
    
    if (points.size() < 3) {
        return "错误: 点到线测量需要3个点";
    }
    
    const QVector3D& point = points[0];
    const QVector3D& lineStart = points[1];
    const QVector3D& lineEnd = points[2];
    
    // 计算线段向量
    QVector3D lineVec = lineEnd - lineStart;
    float lineLength = lineVec.length();
    
    if (lineLength < 1e-6f) {
        return "错误: 线段长度为0";
    }
    
    // 计算点到线段的向量
    QVector3D pointVec = point - lineStart;
    
    // 计算投影参数
    float t = QVector3D::dotProduct(pointVec, lineVec) / (lineLength * lineLength);
    
    // 限制t在0和1之间，确保是到线段而不是到直线的距离
    t = qBound(0.0f, t, 1.0f);
    
    // 计算投影点
    QVector3D projection = lineStart + t * lineVec;
    
    // 计算点到投影点的距离
    float distance = (point - projection).length();
    
    // 验证测量结果的合理性
    if (distance > 10000.0f) { // 超过10米的距离可能不合理
        LOG_WARNING(QString("点到线测量结果异常: %1 mm").arg(distance, 0, 'f', 2));
        return QString("距离: %1 mm (异常)").arg(distance, 0, 'f', 2);
    }
    
    return QString("距离: %1 mm").arg(distance, 0, 'f', 2);
}

QString ImprovedMeasurementCalculator::calculateAreaMeasurement(
    const QVector<QVector3D>& points) {
    
    if (points.size() < 3) {
        return "错误: 面积测量需要至少3个点";
    }
    
    // 使用Shoelace公式计算多边形面积
    float area = 0.0f;
    int n = points.size();
    
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area += points[i].x() * points[j].y();
        area -= points[j].x() * points[i].y();
    }
    
    area = std::fabs(area) / 2.0f;
    
    // 验证测量结果的合理性
    if (area > 1000000.0f) { // 超过1平方米的面积可能不合理
        LOG_WARNING(QString("面积测量结果异常: %1 mm²").arg(area, 0, 'f', 2));
        return QString("面积: %1 mm² (异常)").arg(area, 0, 'f', 2);
    }
    
    return QString("面积: %1 mm²").arg(area, 0, 'f', 2);
}

bool ImprovedMeasurementCalculator::validateMeasurementPoints(
    const QVector<QVector3D>& points, MeasurementType type) {
    
    if (points.isEmpty()) {
        LOG_ERROR("测量点为空");
        return false;
    }
    
    // 检查点的有效性
    for (const QVector3D& point : points) {
        if (point.isNull() || qIsNaN(point.x()) || qIsNaN(point.y()) || qIsNaN(point.z())) {
            LOG_ERROR("发现无效的3D点");
            return false;
        }
        
        // 检查坐标范围是否合理
        if (std::abs(point.x()) > 10000.0f || 
            std::abs(point.y()) > 10000.0f || 
            std::abs(point.z()) > 10000.0f) {
            LOG_WARNING(QString("3D点坐标异常: (%1, %2, %3)")
                       .arg(point.x(), 0, 'f', 2)
                       .arg(point.y(), 0, 'f', 2)
                       .arg(point.z(), 0, 'f', 2));
            return false;
        }
    }
    
    // 根据测量类型检查点数
    switch (type) {
        case MeasurementType::Length:
            if (points.size() < 2) {
                LOG_ERROR("长度测量需要至少2个点");
                return false;
            }
            break;
            
        case MeasurementType::Depth:
            if (points.size() < 4) {
                LOG_ERROR("深度测量需要4个点");
                return false;
            }
            break;
            
        case MeasurementType::PointToLine:
            if (points.size() < 3) {
                LOG_ERROR("点到线测量需要3个点");
                return false;
            }
            break;
            
        case MeasurementType::Area:
            if (points.size() < 3) {
                LOG_ERROR("面积测量需要至少3个点");
                return false;
            }
            break;
            
        default:
            break;
    }
    
    return true;
}

void ImprovedMeasurementCalculator::logMeasurementDetails(
    const QVector<QVector3D>& points, MeasurementType type, const QString& result) {
    
    LOG_INFO(QString("测量详情 - 类型: %1, 点数: %2, 结果: %3")
             .arg(static_cast<int>(type))
             .arg(points.size())
             .arg(result));
    
    for (int i = 0; i < points.size(); ++i) {
        const QVector3D& point = points[i];
        LOG_DEBUG(QString("  点%1: (%2, %3, %4) mm")
                 .arg(i + 1)
                 .arg(point.x(), 0, 'f', 2)
                 .arg(point.y(), 0, 'f', 2)
                 .arg(point.z(), 0, 'f', 2));
    }
}

} // namespace Measurement
} // namespace App
} // namespace SmartScope
