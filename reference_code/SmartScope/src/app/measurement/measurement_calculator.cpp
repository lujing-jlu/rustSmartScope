#include "app/measurement/measurement_calculator.h"
#include "infrastructure/logging/logger.h"
#include <limits>
#include <cmath>
#include <QDebug>

namespace SmartScope {
namespace App {
namespace Measurement {

MeasurementCalculator::MeasurementCalculator()
{
    LOG_INFO("创建测量计算器实例");
}

MeasurementCalculator::~MeasurementCalculator()
{
    LOG_INFO("销毁测量计算器实例");
}

void MeasurementCalculator::calculateMeasurementResult(MeasurementObject* measurement)
{
    if (!measurement) return;
    
    // 获取测量点
    const QVector<QVector3D>& points = measurement->getPoints();
    
    // 根据测量类型计算结果
    QString resultText;
    
    switch (measurement->getType()) {
        case MeasurementType::Length:
            if (points.size() >= 2) {
                // 计算两点之间的距离
                float distance = (points[1] - points[0]).length();
                resultText = QString("长度: %1 mm").arg(distance, 0, 'f', 2);
            }
            break;
            
        case MeasurementType::PointToLine:
            if (points.size() >= 3) {
                // 计算点到线的距离
                QVector3D lineStart = points[0];
                QVector3D lineEnd = points[1];
                QVector3D point = points[2];
                
                // 计算线的方向向量
                QVector3D lineDir = lineEnd - lineStart;
                float lineLength = lineDir.length();
                lineDir /= lineLength;  // 归一化
                
                // 计算点到线的垂直向量
                QVector3D toPoint = point - lineStart;
                float dotProduct = QVector3D::dotProduct(toPoint, lineDir);
                QVector3D projection = lineStart + lineDir * dotProduct;
                QVector3D perpendicular = point - projection;
                
                float distance = perpendicular.length();
                resultText = QString("点到线距离: %1 mm").arg(distance, 0, 'f', 2);
            }
            break;
            
        case MeasurementType::Depth:
            if (points.size() >= 4) {
                // 深度测量 - 使用法向量计算点到面距离
                QVector3D p1 = points[0];
                QVector3D p2 = points[1];
                QVector3D p3 = points[2];
                QVector3D p4 = points[3]; // 目标点
                
                // 计算平面法向量 N = (P2-P1) x (P3-P1)
                QVector3D v1 = p2 - p1;
                QVector3D v2 = p3 - p1;
                QVector3D normal = QVector3D::crossProduct(v1, v2);
                
                // 检查三点是否共线 (法向量长度接近0)
                float normalLength = normal.length();
                if (normalLength < 1e-6f) {
                    resultText = "错误: 平面点共线";
                    break;
                }
                
                normal.normalize(); // 单位化法向量
                float A = normal.x();
                float B = normal.y();
                float C = normal.z();
                float D = -QVector3D::dotProduct(normal, p1);
                
                // 计算点到面距离
                float dotProduct = A * p4.x() + B * p4.y() + C * p4.z() + D;
                float distance = std::fabs(dotProduct); // 深度测量始终使用绝对值
                // 强制取绝对值
                distance = std::fabs(distance);
                resultText = QString("深度: %1 mm").arg(distance, 0, 'f', 2);
            } else if (points.size() == 1) {
                // 兼容旧版本的深度计算 (单点Z值)
                float depth = std::fabs(points[0].z()); // 使用绝对值确保深度为正
                // 强制取绝对值
                depth = std::fabs(depth);
                resultText = QString("深度: %1 mm").arg(depth, 0, 'f', 2);
            } else {
                resultText = "错误: 点数不足";
            }
            break;
            
        case MeasurementType::Area:
            if (points.size() >= 3) {
                // 面积测量 - 使用3D叉积计算真实的多边形面积
                float area = 0.0f;

                if (points.size() == 3) {
                    // 三角形面积计算
                    QVector3D edge1 = points[1] - points[0];
                    QVector3D edge2 = points[2] - points[0];
                    area = QVector3D::crossProduct(edge1, edge2).length() / 2.0f;
                } else {
                    // 多边形面积计算 - 将多边形分解为三角形
                    // 以第一个点为公共顶点，将多边形分解为多个三角形
                    for (int i = 1; i < points.size() - 1; i++) {
                        QVector3D edge1 = points[i] - points[0];
                        QVector3D edge2 = points[i+1] - points[0];
                        area += QVector3D::crossProduct(edge1, edge2).length() / 2.0f;
                    }
                }

                resultText = QString("面积: %1 mm²").arg(area, 0, 'f', 2);
            } else {
                resultText = "错误: 点数不足";
            }
            break;
            
        case MeasurementType::Polyline:
            if (points.size() >= 2) {
                // 折线测量 - 计算所有线段长度之和
                float totalLength = 0.0f;
                for (int i = 0; i < points.size() - 1; i++) {
                    totalLength += (points[i+1] - points[i]).length();
                }
                resultText = QString("折线长度: %1 mm").arg(totalLength, 0, 'f', 2);
            }
            break;
            
        case MeasurementType::Profile:
        case MeasurementType::RegionProfile:
            resultText = "剖面分析完成";
            break;
            
        case MeasurementType::MissingArea:
            if (points.size() >= 3) {
                // 缺失面积测量 - 计算多边形面积
                // 传入的points是多边形点（交点+后续点），第一个点是交点

                float area = 0.0f;

                if (points.size() == 3) {
                    // 三角形面积计算
                    QVector3D edge1 = points[1] - points[0];
                    QVector3D edge2 = points[2] - points[0];
                    area = QVector3D::crossProduct(edge1, edge2).length() / 2.0f;

                    LOG_INFO(QString("缺失面积测量：三角形面积计算 - 交点(%1,%2,%3), 点1(%4,%5,%6), 点2(%7,%8,%9)")
                            .arg(points[0].x(), 0, 'f', 2).arg(points[0].y(), 0, 'f', 2).arg(points[0].z(), 0, 'f', 2)
                            .arg(points[1].x(), 0, 'f', 2).arg(points[1].y(), 0, 'f', 2).arg(points[1].z(), 0, 'f', 2)
                            .arg(points[2].x(), 0, 'f', 2).arg(points[2].y(), 0, 'f', 2).arg(points[2].z(), 0, 'f', 2));
                } else {
                    // 多边形面积计算 - 将多边形分解为三角形
                    // 以第一个点（交点）为公共顶点，将多边形分解为多个三角形
                    QVector3D intersectionPoint = points[0]; // 交点是第一个点

                    for (int i = 1; i < points.size() - 1; i++) {
                        QVector3D edge1 = points[i] - intersectionPoint;
                        QVector3D edge2 = points[i+1] - intersectionPoint;
                        float triangleArea = QVector3D::crossProduct(edge1, edge2).length() / 2.0f;
                        area += triangleArea;

                        LOG_DEBUG(QString("缺失面积测量：三角形%1面积 = %2 mm²")
                                .arg(i).arg(triangleArea, 0, 'f', 2));
                    }

                    // 连接最后一个点和第二个点形成闭合多边形
                    if (points.size() > 2) {
                        QVector3D edge1 = points[points.size()-1] - intersectionPoint;
                        QVector3D edge2 = points[1] - intersectionPoint;
                        float triangleArea = QVector3D::crossProduct(edge1, edge2).length() / 2.0f;
                        area += triangleArea;

                        LOG_DEBUG(QString("缺失面积测量：闭合三角形面积 = %1 mm²")
                                .arg(triangleArea, 0, 'f', 2));
                    }

                    LOG_INFO(QString("缺失面积测量：多边形面积计算完成，总面积 = %1 mm²，点数 = %2")
                            .arg(area, 0, 'f', 2).arg(points.size()));
                }

                resultText = QString("缺失区域面积: %1 mm²").arg(area, 0, 'f', 2);
            } else {
                resultText = "点数不足，需要至少3个点";
            }
            break;
            
        default:
            resultText = "未知测量类型";
            break;
    }
    
    measurement->setResult(resultText);
    LOG_INFO(QString("计算测量结果: %1").arg(resultText));
}

QVector3D MeasurementCalculator::imageToPointCloudCoordinates(
    int x, int y, 
    const cv::Mat& depthMap, 
    const cv::Mat& cameraMatrix,
    const QSize& originalImageSize)
{
    // 只在起点、终点或每隔50个像素输出一次日志
    bool shouldLog = (x == 0 || y == 0 || 
                     x == originalImageSize.width()-1 || 
                     y == originalImageSize.height()-1 ||
                     (x % 50 == 0 && y % 50 == 0));

    // 检查深度图
    if (depthMap.empty()) {
        LOG_ERROR("深度图为空，无法进行坐标转换");
        return QVector3D(0, 0, 0);
    }

    // 检查相机内参矩阵
    if (cameraMatrix.empty()) {
        LOG_ERROR("相机内参矩阵为空，无法进行坐标转换");
        return QVector3D(0, 0, 0);
    }

    // 获取相机参数
    double focal_length = cameraMatrix.at<double>(0, 0);
    double cx = cameraMatrix.at<double>(0, 2);
    double cy = cameraMatrix.at<double>(1, 2);

    // 检查坐标是否在深度图范围内
    if (x < 0 || x >= depthMap.cols || y < 0 || y >= depthMap.rows) {
        LOG_ERROR(QString("坐标 (%1, %2) 超出深度图范围 (%3, %4)")
                 .arg(x).arg(y).arg(depthMap.cols).arg(depthMap.rows));
        return QVector3D(0, 0, 0);
    }

    // 获取深度值 Z
    float Z = depthMap.at<float>(y, x);
    if (Z <= 0) {
        LOG_ERROR(QString("坐标 (%1, %2) 处深度值无效: %3").arg(x).arg(y).arg(Z));

        // 尝试在周围找有效的深度值
        const int searchRadius = 10;
        bool foundValidDepth = false;
        float validZ = 0;
        int validX = x, validY = y; // 初始化为原始坐标
        float minDistance = std::numeric_limits<float>::max();

        for (int dy = -searchRadius; dy <= searchRadius; dy++) {
            for (int dx = -searchRadius; dx <= searchRadius; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < depthMap.cols && ny >= 0 && ny < depthMap.rows) {
                    float neighborZ = depthMap.at<float>(ny, nx);
                    if (neighborZ > 0) {
                        float distance = std::sqrt(dx*dx + dy*dy);
                        // 找到最近的有效深度点
                        if (distance < minDistance) { // 找到更近的有效点
                            validZ = neighborZ;
                            validX = nx;
                            validY = ny;
                            minDistance = distance;
                            foundValidDepth = true;
                        } else if (distance == minDistance && neighborZ > validZ) { // 相同距离，优先选择较大的深度值
                            validZ = neighborZ;
                            validX = nx;
                            validY = ny;
                        }
                    }
                }
            }
        }

        if (foundValidDepth) {
            LOG_INFO(QString("找到附近有效深度点: (%1, %2) 深度值: %3, 距离原点: %4 像素")
                    .arg(validX).arg(validY).arg(validZ).arg(minDistance, 0, 'f', 1));
            x = validX; // 使用找到的点的坐标
            y = validY;
            Z = validZ; // 使用找到的点的深度值
        } else {
            LOG_ERROR("在附近区域也找不到有效深度值，无法创建3D点");
            return QVector3D(0, 0, 0);
        }
    }

    // 计算深度缩放因子
    float depthScaleFactor = 1.0f;
    if (originalImageSize.width() > 0 && depthMap.cols > 0) {
        depthScaleFactor = static_cast<float>(originalImageSize.width()) / static_cast<float>(depthMap.cols);
    }
    
    // 如果有深度缩放，应用缩放
    if (depthScaleFactor != 1.0f) {
        Z *= depthScaleFactor;
    }

    // 计算3D坐标
    double X = (x - cx) * Z / focal_length;
    double Y = (y - cy) * Z / focal_length;

    // 转换坐标系 (OpenCV -> 标准3D坐标系)
    // OpenCV: X向右，Y向下，Z向前
    // 标准3D: X向右，Y向上，Z向前（保持Z轴正方向，便于剖面计算）
    // 修复：保持Z轴为正值，Y轴翻转为向上
    QVector3D point3D(X, -Y, Z);
    
    if(shouldLog) {
        qDebug() << QString("图像坐标 (%1, %2) 转换为3D坐标: (%3, %4, %5) mm")
            .arg(x).arg(y)
            .arg(point3D.x(), 0, 'f', 2)
            .arg(point3D.y(), 0, 'f', 2)
            .arg(point3D.z(), 0, 'f', 2);
    }
    
    return point3D;
}

QVector3D MeasurementCalculator::findNearestPointInCloud(
    int pixelX, int pixelY,
    const std::vector<QVector3D>& pointCloud,
    const std::vector<cv::Point2i>& pixelCoords,
    const QVector3D& cloudCenter,
    int searchRadius)
{
    // 检查参数
    if (pointCloud.empty() || pixelCoords.empty()) {
        LOG_ERROR("点云或像素坐标映射为空，无法查找最近点");
        return QVector3D(0, 0, 0);
    }

    // 检查尺寸是否匹配
    if (pointCloud.size() != pixelCoords.size()) {
        LOG_ERROR(QString("点云数据不一致: 点云中有 %1 个点，像素映射中有 %2 个点")
                .arg(pointCloud.size()).arg(pixelCoords.size()));
        return QVector3D(0, 0, 0);
    }

    LOG_INFO(QString("在点云中查找像素(%1, %2)附近的点，搜索半径: %3")
            .arg(pixelX).arg(pixelY).arg(searchRadius));
    
    // 查找最近的点
    float minDistance = std::numeric_limits<float>::max();
    int nearestIndex = -1;
    
    for (size_t i = 0; i < pixelCoords.size(); ++i) {
        const cv::Point2i& pix = pixelCoords[i];
        float dx = pix.x - pixelX;
        float dy = pix.y - pixelY;
        float distance = std::sqrt(dx * dx + dy * dy);
        
        if (distance < minDistance && distance <= searchRadius) {
            minDistance = distance;
            nearestIndex = i;
        }
    }
    
    if (nearestIndex >= 0) {
        // 获取点云中对应的3D点
        QVector3D point = pointCloud[nearestIndex];
        
        // 返回带有补偿的点坐标 (考虑点云居中可能引入的偏移)
        QVector3D compensatedPoint = point + cloudCenter;
        
        LOG_INFO(QString("找到最近的点云点：索引=%1，像素坐标=(%2,%3)，原始3D坐标=(%4,%5,%6)，补偿后坐标=(%7,%8,%9)，距离=%10像素")
               .arg(nearestIndex)
               .arg(pixelCoords[nearestIndex].x)
               .arg(pixelCoords[nearestIndex].y)
               .arg(point.x(), 0, 'f', 4)
               .arg(point.y(), 0, 'f', 4)
               .arg(point.z(), 0, 'f', 4)
               .arg(compensatedPoint.x(), 0, 'f', 4)
               .arg(compensatedPoint.y(), 0, 'f', 4)
               .arg(compensatedPoint.z(), 0, 'f', 4)
               .arg(minDistance, 0, 'f', 2));
        
        return compensatedPoint;
    } else {
        LOG_WARNING(QString("在半径%1像素内找不到点云中的点").arg(searchRadius));
        return QVector3D(0, 0, 0);
    }
}

QVector<QPointF> MeasurementCalculator::calculateProfileData(
    const MeasurementObject* measurement,
    const cv::Mat& depthMap,
    const QSize& originalImageSize,
    std::shared_ptr<SmartScope::Core::CameraCorrectionManager> correctionManager)
{
    QVector<QPointF> profileData;
    
    // 基本参数检查
    if (!measurement || measurement->getType() != MeasurementType::Profile || 
        measurement->getOriginalClickPoints().size() != 2 || 
        depthMap.empty() || depthMap.type() != CV_32F) {
        LOG_ERROR("剖面测量：无效的参数");
        return profileData;
    }

    // 获取相机内参
    auto stereoHelper = correctionManager ? correctionManager->getStereoCalibrationHelper() : nullptr;
    cv::Mat cameraMatrix = stereoHelper ? stereoHelper->getCameraMatrixLeft() : cv::Mat();
    if (cameraMatrix.empty()) {
        LOG_ERROR("剖面测量：无法获取相机内参矩阵");
        return profileData;
    }

    // 获取原始点击点和3D点
    const QVector<QPoint>& clickPoints = measurement->getOriginalClickPoints();
    
    // 重要：检查坐标系统一致性
    // originalImageSize应该是显示图像（校正裁剪后）的尺寸，与深度图尺寸应该一致
    LOG_INFO(QString("剖面测量坐标系统检查 - 原始图像尺寸: %1x%2, 深度图尺寸: %3x%4")
            .arg(originalImageSize.width()).arg(originalImageSize.height())
            .arg(depthMap.cols).arg(depthMap.rows));

    // 计算缩放因子，用于将点击坐标转换到深度图尺寸
    float scaleX = static_cast<float>(depthMap.cols) / originalImageSize.width();
    float scaleY = static_cast<float>(depthMap.rows) / originalImageSize.height();

    LOG_INFO(QString("剖面测量缩放因子 - X: %1, Y: %2").arg(scaleX, 0, 'f', 3).arg(scaleY, 0, 'f', 3));

    // 将点击点缩放到深度图尺寸
    cv::Point p1(clickPoints[0].x() * scaleX, clickPoints[0].y() * scaleY);
    cv::Point p2(clickPoints[1].x() * scaleX, clickPoints[1].y() * scaleY);

    // 确保坐标在深度图范围内
    p1.x = qBound(0, p1.x, depthMap.cols - 1);
    p1.y = qBound(0, p1.y, depthMap.rows - 1);
    p2.x = qBound(0, p2.x, depthMap.cols - 1);
    p2.y = qBound(0, p2.y, depthMap.rows - 1);
    
    LOG_INFO(QString("剖面测量 - 起点: (%1,%2) 终点: (%3,%4) - 深度图坐标: (%5,%6) 到 (%7,%8)")
            .arg(clickPoints[0].x()).arg(clickPoints[0].y())
            .arg(clickPoints[1].x()).arg(clickPoints[1].y())
            .arg(p1.x).arg(p1.y).arg(p2.x).arg(p2.y));
    
    // 使用LineIterator遍历线段上的所有像素点
    cv::LineIterator it(depthMap, p1, p2, 8);
    int numPoints = it.count;
    
    LOG_INFO(QString("剖面测量 - 线段总像素数: %1").arg(numPoints));
    
    // 获取线段的起点3D坐标（用于计算每个点的相对距离）
    QVector3D startPoint3D;
    bool hasValidStartPoint = false;
    
    // 存储所有有效的3D点和它们在线段上的位置
    struct ProfilePoint {
        float distance;  // 沿线段的距离
        QVector3D point; // 3D点
        bool valid;      // 是否有效
    };
    QVector<ProfilePoint> points3D;
    
    // 遍历线段上的所有像素点
    for (int i = 0; i < numPoints; i++, ++it) {
        cv::Point pos = it.pos();
        
        // 检查坐标是否在深度图范围内
        if (pos.x < 0 || pos.x >= depthMap.cols || pos.y < 0 || pos.y >= depthMap.rows) {
            continue;
        }
        
        // 获取当前像素的深度值
        float depth = depthMap.at<float>(pos.y, pos.x);

        // 跳过无效深度值 - 注意深度图中的值可能是负数（表示距离）
        if (qIsNaN(depth) || qIsInf(depth) || qAbs(depth) < 0.1f) {
            // 尝试在小范围内寻找有效深度值
            bool foundValid = false;
            for (int dy = -2; dy <= 2 && !foundValid; dy++) {
                for (int dx = -2; dx <= 2 && !foundValid; dx++) {
                    int nx = pos.x + dx;
                    int ny = pos.y + dy;
                    if (nx >= 0 && nx < depthMap.cols && ny >= 0 && ny < depthMap.rows) {
                        float nearbyDepth = depthMap.at<float>(ny, nx);
                        // 修改：检查深度值的有效性，允许负值（距离）
                        if (!qIsNaN(nearbyDepth) && !qIsInf(nearbyDepth) && qAbs(nearbyDepth) >= 0.1f) {
                            depth = nearbyDepth;
                            pos.x = nx;
                            pos.y = ny;
                            foundValid = true;
                            break;
                        }
                    }
                }
            }
            
            if (!foundValid) {
                continue; // 跳过无法找到有效深度的点
            }
        }
        
        // 重要：直接使用深度图坐标进行3D转换，避免不必要的坐标转换
        // 因为imageToPointCloudCoordinates内部会处理坐标缩放
        QVector3D point3D = imageToPointCloudCoordinates(
            pos.x, pos.y, depthMap, cameraMatrix, QSize(depthMap.cols, depthMap.rows));
        
        // 跳过无效的3D点
        if (point3D.isNull() || qIsNaN(point3D.x()) || qIsNaN(point3D.y()) || qIsNaN(point3D.z())) {
            continue;
        }
        
        // 保存起点3D坐标（第一个有效点）
        if (!hasValidStartPoint) {
            startPoint3D = point3D;
            hasValidStartPoint = true;
        }
        
        // 计算沿剖面线的距离 - 使用像素位置比例
        float distance = 0.0f;
        if (hasValidStartPoint && numPoints > 1) {
            // 使用像素位置在线段上的比例来计算距离
            float pixelIndex = static_cast<float>(i);
            float totalPixels = static_cast<float>(numPoints - 1);
            float t = (totalPixels > 0) ? (pixelIndex / totalPixels) : 0;

            // 计算剖面线的总长度（使用起点和终点的3D坐标）
            static QVector3D scanStartPoint, scanEndPoint;
            static float totalLineLength = 0;
            static bool lineInfoCalculated = false;

            if (!lineInfoCalculated) {
                // 只计算一次起点和终点的3D坐标
                cv::Point startPixel(clickPoints[0].x() * scaleX, clickPoints[0].y() * scaleY);
                cv::Point endPixel(clickPoints[1].x() * scaleX, clickPoints[1].y() * scaleY);

                scanStartPoint = imageToPointCloudCoordinates(
                    startPixel.x, startPixel.y, depthMap, cameraMatrix, QSize(depthMap.cols, depthMap.rows));
                scanEndPoint = imageToPointCloudCoordinates(
                    endPixel.x, endPixel.y, depthMap, cameraMatrix, QSize(depthMap.cols, depthMap.rows));

                if (!scanStartPoint.isNull() && !scanEndPoint.isNull()) {
                    QVector3D lineVector = scanEndPoint - scanStartPoint;
                    totalLineLength = lineVector.length();
                    lineInfoCalculated = true;
                    LOG_INFO(QString("剖面线总长度: %1 mm").arg(totalLineLength, 0, 'f', 2));
                }
            }

            // 沿剖面线的距离 = 比例 * 总长度
            if (lineInfoCalculated && totalLineLength > 0) {
                distance = t * totalLineLength;
            } else {
                // 备用方法：使用3D空间距离
                QVector3D diff = point3D - startPoint3D;
                distance = diff.length();
            }
        }
        
        // 保存有效点
        ProfilePoint pp;
        pp.distance = distance;
        pp.point = point3D;
        pp.valid = true;
        points3D.append(pp);
        
        // 记录一些点的信息（开始、中间、结束）
        if (i == 0 || i == numPoints-1 || i % 20 == 0) {
            LOG_INFO(QString("剖面点 #%1: 沿线距离=%2mm, 深度=%3mm, 3D坐标=(%4,%5,%6)")
                    .arg(i).arg(distance, 0, 'f', 2).arg(point3D.z(), 0, 'f', 2)
                    .arg(point3D.x(), 0, 'f', 2).arg(point3D.y(), 0, 'f', 2).arg(point3D.z(), 0, 'f', 2));
        }
    }
    
    // 检查是否获取了足够的有效点
    if (points3D.size() < 2) {
        LOG_WARNING("剖面测量：有效点数不足，无法生成剖面图");
        
        // 如果只有起点终点，至少创建一个简单的两点剖面
        if (measurement->getPoints().size() >= 2) {
            QVector3D start = measurement->getPoints()[0];
            QVector3D end = measurement->getPoints()[1];
            
            // 修复：计算相对深度 - 使用绝对值确保深度为正，与其他测量功能一致
            float startDepth = std::fabs(start.z());
            float endDepth = std::fabs(end.z());
            float minDepth = qMin(startDepth, endDepth);
            float startRelativeDepth = startDepth - minDepth;
            float endRelativeDepth = endDepth - minDepth;
            
            profileData.append(QPointF(0, startRelativeDepth));
            profileData.append(QPointF((end - start).length(), endRelativeDepth));
            
            LOG_INFO(QString("剖面测量：使用起点和终点创建简单剖面，基准深度: %1mm")
                    .arg(minDepth, 0, 'f', 2));
            return profileData;
        }
        
        return profileData;
    }
    
    // 按距离排序所有点
    std::sort(points3D.begin(), points3D.end(), 
              [](const ProfilePoint& a, const ProfilePoint& b) { return a.distance < b.distance; });
    
    // 正确的剖面测量：计算表面起伏变化
    // 1. 先计算剖面线的基准平面（最佳拟合平面）
    // 2. 计算每个点到基准平面的偏差，这才是真正的"起伏"
    
    if (points3D.size() < 3) {
        LOG_WARNING("剖面测量：点数不足以计算基准平面，使用简单线性基准");
        
        // 使用起点和终点定义线性基准
        QVector3D startPoint = points3D.first().point;
        QVector3D endPoint = points3D.last().point;
        float totalDistance = points3D.last().distance;
        
        for (const auto& pp : points3D) {
            // 计算在线性基准上的插值点
            float t = (totalDistance > 0) ? (pp.distance / totalDistance) : 0;
            QVector3D baselinePoint = startPoint + t * (endPoint - startPoint);
            
            // 计算实际点与基准点的高程差（Z方向）
            float elevation = pp.point.z() - baselinePoint.z();
            
            profileData.append(QPointF(pp.distance, elevation));
        }
    } else {
        // 使用最小二乘法拟合基准平面
        LOG_INFO("剖面测量：计算基准平面（最小二乘拟合）");
        
        // 收集所有点的坐标
        QVector<QVector3D> allPoints;
        for (const auto& pp : points3D) {
            allPoints.append(pp.point);
        }
        
        // 计算基准平面：ax + by + cz + d = 0
        // 使用最小二乘法拟合平面
        QVector3D centroid(0, 0, 0);
        for (const QVector3D& p : allPoints) {
            centroid += p;
        }
        centroid /= allPoints.size();
        
        // 构建协方差矩阵用于主成分分析
        float xx = 0, xy = 0, xz = 0, yy = 0, yz = 0, zz = 0;
        for (const QVector3D& p : allPoints) {
            QVector3D dp = p - centroid;
            xx += dp.x() * dp.x();
            xy += dp.x() * dp.y();
            xz += dp.x() * dp.z();
            yy += dp.y() * dp.y();
            yz += dp.y() * dp.z();
            zz += dp.z() * dp.z();
        }
        
        // 简化版本：假设剖面线主要在XY平面内，使用Z值变化作为高程
        // 更精确的实现可以使用完整的主成分分析
        
        // 计算沿剖面线方向的线性趋势（基准线）- 使用一致的深度计算方式
        float sumDistance = 0, sumZ = 0, sumDistanceZ = 0, sumDistanceSq = 0;
        for (const auto& pp : points3D) {
            float pointDepth = std::fabs(pp.point.z()); // 与后续计算保持一致
            sumDistance += pp.distance;
            sumZ += pointDepth;
            sumDistanceZ += pp.distance * pointDepth;
            sumDistanceSq += pp.distance * pp.distance;
        }
        
        int n = points3D.size();
        float meanDistance = sumDistance / n;
        float meanZ = sumZ / n;
        
        // 计算线性拟合的斜率和截距
        float denominator = sumDistanceSq - n * meanDistance * meanDistance;
        float slope = 0, intercept = meanZ;
        
        if (qAbs(denominator) > 1e-6) {
            slope = (sumDistanceZ - n * meanDistance * meanZ) / denominator;
            intercept = meanZ - slope * meanDistance;
        }
        
        LOG_INFO(QString("剖面测量：基准线拟合 - 斜率: %1, 截距: %2mm")
                .arg(slope, 0, 'f', 6).arg(intercept, 0, 'f', 2));
        
        // 计算每个点相对于基准线的高程偏差
        for (const auto& pp : points3D) {
            // 基准线上对应位置的理论Z值
            float baselineZ = slope * pp.distance + intercept;

            // 修复：确保深度值的正确性 - 使用绝对值确保深度为正
            // 与其他测量功能保持一致的深度计算方式
            float actualDepth = std::fabs(pp.point.z());
            float baselineDepth = std::fabs(baselineZ);

            // 实际点与基准线的高程差
            float elevation = actualDepth - baselineDepth;

            profileData.append(QPointF(pp.distance, elevation));
        }
    }
    
    // 计算剖面起伏的统计信息
    float minElevation = std::numeric_limits<float>::max();
    float maxElevation = std::numeric_limits<float>::lowest();
    
    if (!profileData.isEmpty()) {
        for (const QPointF& point : profileData) {
            float elevation = point.y();
            if (elevation < minElevation) minElevation = elevation;
            if (elevation > maxElevation) maxElevation = elevation;
        }
    }
    
    float elevationRange = maxElevation - minElevation;
    
    LOG_INFO(QString("剖面测量：成功生成表面起伏剖面图，共%1个点")
            .arg(profileData.size()));
    LOG_INFO(QString("剖面测量：距离范围: [%1, %2]mm")
            .arg(profileData.isEmpty() ? 0 : profileData.first().x(), 0, 'f', 2)
            .arg(profileData.isEmpty() ? 0 : profileData.last().x(), 0, 'f', 2));
    LOG_INFO(QString("剖面测量：高程变化范围: [%1, %2]mm，最大起伏: %3mm")
            .arg(minElevation, 0, 'f', 2)
            .arg(maxElevation, 0, 'f', 2)
            .arg(elevationRange, 0, 'f', 2));
    
    // 详细调试：输出剖面数据样本
    if (!profileData.isEmpty()) {
        LOG_INFO(QString("=== MeasurementCalculator剖面数据样本 ==="));
        int sampleCount = qMin(5, profileData.size());
        
        LOG_INFO(QString("前%1个点:").arg(sampleCount));
        for (int i = 0; i < sampleCount; ++i) {
            LOG_INFO(QString("  点[%1]: 距离=%2mm, 起伏=%3mm")
                    .arg(i)
                    .arg(profileData[i].x(), 0, 'f', 3)
                    .arg(profileData[i].y(), 0, 'f', 3));
        }
        
        if (profileData.size() > 10) {
            LOG_INFO(QString("后%1个点:").arg(sampleCount));
            for (int i = profileData.size() - sampleCount; i < profileData.size(); ++i) {
                LOG_INFO(QString("  点[%1]: 距离=%2mm, 起伏=%3mm")
                        .arg(i)
                        .arg(profileData[i].x(), 0, 'f', 3)
                        .arg(profileData[i].y(), 0, 'f', 3));
            }
        }
        
        LOG_INFO(QString("============================================"));
    }
    
    return profileData;
}

bool MeasurementCalculator::calculateLinesIntersection(
    const QVector3D& line1Point1,
    const QVector3D& line1Point2,
    const QVector3D& line2Point1,
    const QVector3D& line2Point2,
    QVector3D& intersectionPoint)
{
    // 计算方向向量
    QVector3D dir1 = (line1Point2 - line1Point1).normalized();
    QVector3D dir2 = (line2Point2 - line2Point1).normalized();
    
    // 计算向量叉积
    QVector3D cross = QVector3D::crossProduct(dir1, dir2);
    
    // 检查两条直线是否平行或重合
    float crossLengthSq = cross.lengthSquared();
    if (crossLengthSq < 1e-10f) {
        // 直线平行或重合，没有唯一交点
        LOG_WARNING("线段平行或重合，无法计算交点");
        return false;
    }
    
    // 计算两条线之间的最近点
    // 参考: https://en.wikipedia.org/wiki/Skew_lines#Distance
    QVector3D w0 = line1Point1 - line2Point1;
    
    // 计算系数
    float a = QVector3D::dotProduct(dir1, dir1);
    float b = QVector3D::dotProduct(dir1, dir2);
    float c = QVector3D::dotProduct(dir2, dir2);
    float d = QVector3D::dotProduct(dir1, w0);
    float e = QVector3D::dotProduct(dir2, w0);
    
    // 计算参数t1和t2
    float denominator = a*c - b*b;
    
    // 防止除零
    if (qAbs(denominator) < 1e-10f) {
        LOG_WARNING("交点计算中出现零除错误");
        return false;
    }
    
    float t1 = (b*e - c*d) / denominator;
    float t2 = (a*e - b*d) / denominator;
    
    // 计算最接近点
    QVector3D point1 = line1Point1 + dir1 * t1;
    QVector3D point2 = line2Point1 + dir2 * t2;
    
    // 计算两点之间的距离
    float distance = (point2 - point1).length();
    
    // 如果距离太大，认为没有交点
    const float MAX_INTERSECTION_DISTANCE = 10.0f; // 最大允许距离，单位：毫米
    if (distance > MAX_INTERSECTION_DISTANCE) {
        LOG_WARNING(QString("线段交点计算：距离太大 (%1 mm) > 阈值 (%2 mm)")
                   .arg(distance, 0, 'f', 2).arg(MAX_INTERSECTION_DISTANCE, 0, 'f', 2));
        return false;
    }
    
    // 取中点作为交点
    intersectionPoint = (point1 + point2) * 0.5f;
    
    LOG_INFO(QString("计算线段交点: (%1, %2, %3)，距离: %4 mm")
             .arg(intersectionPoint.x(), 0, 'f', 2)
             .arg(intersectionPoint.y(), 0, 'f', 2)
             .arg(intersectionPoint.z(), 0, 'f', 2)
             .arg(distance, 0, 'f', 2));
    
    return true;
}

// 获取最新的深度图
cv::Mat MeasurementCalculator::getLatestDepthMap() const
{
    return m_latestDepthMap;
}

// 设置最新的深度图
void MeasurementCalculator::setLatestDepthMap(const cv::Mat& depthMap)
{
    if (!depthMap.empty()) {
        m_latestDepthMap = depthMap.clone();
    }
}

} // namespace Measurement
} // namespace App
} // namespace SmartScope 