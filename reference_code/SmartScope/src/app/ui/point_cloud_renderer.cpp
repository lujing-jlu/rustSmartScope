#include "app/ui/point_cloud_renderer.h"
#include "infrastructure/logging/logger.h"
#include <pcl/point_types.h>
#include <cmath>
#include <vector>  // for std::vector
#include <algorithm>  // for std::sort

namespace SmartScope {
namespace App {
namespace Ui {

PointCloudRenderer::PointCloudRenderer(PointCloudGLWidget* widget)
    : m_widget(widget)
{
    if (!m_widget) {
        LOG_ERROR("PointCloudRenderer 初始化失败：PointCloudGLWidget 指针为空");
        // 可能需要抛出异常或设置错误状态
    }
     LOG_INFO("PointCloudRenderer 已创建");
}

PointCloudRenderer::~PointCloudRenderer()
{
     LOG_INFO("PointCloudRenderer 已销毁");
}

void PointCloudRenderer::renderMeasurements(const QVector<MeasurementObject*>& measurements)
{
    if (!m_widget) return;

    // 清除旧的几何对象
    clearGeometryObjects();
    
    // 强制更新，确保完全清除先前状态
    updateWidget();

    // 获取球体半径 - 基于点大小计算
    float pointSize = getPointSize(); // 使用封装的 getter
    float sphereRadius = m_baseRadius * 1.5f; // 球体半径

    LOG_INFO(QString("开始在点云中渲染 %1 个测量对象").arg(measurements.size()));

    // 绘制每个测量对象，使用唯一标识符
    for (int measurementIndex = 0; measurementIndex < measurements.size(); measurementIndex++) {
        MeasurementObject* measurement = measurements[measurementIndex];
        if (measurement && measurement->isVisible()) {
            MeasurementType type = measurement->getType(); // 获取类型
            const QVector<QVector3D>& points = measurement->getPoints(); // 毫米单位
            
            // 生成唯一组标识符
            QString groupId = QString("measurement_%1_%2").arg(static_cast<int>(type)).arg(measurementIndex);
            
            switch (type) {
                case MeasurementType::Length:
                    renderLengthMeasurement(measurement, points, sphereRadius, groupId);
                    break;

                case MeasurementType::PointToLine:
                    renderPointToLineMeasurement(measurement, points, sphereRadius, groupId);
                    break;
                    
                case MeasurementType::Depth:
                    renderDepthMeasurement(measurement, points, sphereRadius, groupId);
                    break;

                case MeasurementType::Area:
                    renderAreaMeasurement(measurement, points, sphereRadius, groupId);
                    break;

                case MeasurementType::Polyline:
                    renderPolylineMeasurement(measurement, points, sphereRadius, groupId);
                    break;

                case MeasurementType::Profile:
                    renderProfileMeasurement(measurement, points, sphereRadius, groupId);
                    break;

                case MeasurementType::MissingArea:
                    renderMissingAreaMeasurement(measurement, points, sphereRadius, groupId);
                    break;

                default:
                    LOG_DEBUG(QString("暂不支持在点云中渲染此测量类型: %1").arg(static_cast<int>(type)));
                    break;
            } // end switch
        } // end if measurement valid
    } // end for measurement loop

    // 所有测量对象渲染完成后再次强制更新
    updateWidget();
    LOG_INFO("点云测量对象渲染完成");
}

// 添加封装的渲染方法
void PointCloudRenderer::renderLengthMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points, float sphereRadius, const QString& groupId)
{
    if (points.size() < 2) return;
    
    // 转换为米单位用于渲染
    QVector3D point1Meters = points[0] / 1000.0f;
    QVector3D point2Meters = points[1] / 1000.0f;
    QColor color = QColor(0, 255, 0); // 绿色用于长度
    
    // 添加球体标记端点
    addSphere(point1Meters, sphereRadius, color, groupId + "_p1");
    addSphere(point2Meters, sphereRadius, color, groupId + "_p2");
    
    // 添加连接线
    addLine(point1Meters, point2Meters, color, groupId + "_line");
    
    // 添加测量结果文本 - 在线段中点显示
    QVector3D midPoint = (point1Meters + point2Meters) * 0.5f;
    addText(midPoint, measurement->getResult(), QColor(255, 255, 255), groupId + "_text");
    
    LOG_DEBUG(QString("渲染点云长度测量 [%1]: %2").arg(groupId).arg(measurement->getResult()));
}

void PointCloudRenderer::renderPointToLineMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points, float sphereRadius, const QString& groupId)
{
    if (points.size() != 3) return;
    
    // 转换为米单位用于渲染
    QVector3D p1 = points[0] / 1000.0f;
    QVector3D p2 = points[1] / 1000.0f;
    QVector3D p3 = points[2] / 1000.0f;
    QColor baseColor = QColor(255, 255, 0); // 黄色用于点到线
    QColor perpColor = QColor(255, 0, 255); // 紫色用于垂线
    
    // 绘制点 P1, P2, P3
    addSphere(p1, sphereRadius, baseColor, groupId + "_p1");
    addSphere(p2, sphereRadius, baseColor, groupId + "_p2");
    addSphere(p3, sphereRadius * 1.1f, baseColor, groupId + "_p3");
    
    // 计算投影点 P_proj
    QVector3D lineVec = p2 - p1;
    QVector3D pointVec = p3 - p1;
    float lineLenSq = lineVec.lengthSquared();
    QVector3D projPoint;
    float t = 0.0f;

    if (lineLenSq < 1e-9f) { // P1和P2非常接近
        projPoint = p1; // 投影点就是 P1
        t = 0.0f;
    } else {
        t = QVector3D::dotProduct(pointVec, lineVec) / lineLenSq;
        projPoint = p1 + t * lineVec;
    }

    // 绘制线段 P1-P2 (黄色实线)
    addLine(p1, p2, baseColor, groupId + "_baseLine"); 
    
    // 如果投影点在线段外，绘制虚线延长线
    double dashLen = 0.001; // 虚线长度 1mm (单位：米)
    double gapLen = 0.0005; // 间隔长度 0.5mm
    if (t < 0.0f) {
        addDashedLine(projPoint, p1, baseColor, dashLen, gapLen, groupId + "_dashExt"); // P1 到 投影点 (虚线)
    } else if (t > 1.0f) {
        addDashedLine(projPoint, p2, baseColor, dashLen, gapLen, groupId + "_dashExt"); // P2 到 投影点 (虚线)
    }

    // 绘制垂线 P3-P_proj (紫色实线)
    addLine(p3, projPoint, perpColor, groupId + "_perpLine");

    // 添加测量结果文本 - 在垂线段 P3-projPoint 中点
    QVector3D labelPos = (p3 + projPoint) * 0.5f;
    addText(labelPos, measurement->getResult(), QColor(255, 255, 255), groupId + "_text");
    
    LOG_DEBUG(QString("渲染点云点到线测量 [%1]: %2").arg(groupId).arg(measurement->getResult()));
}

void PointCloudRenderer::renderDepthMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points, float sphereRadius, const QString& groupId)
{
    if (points.size() != 4) {
        LOG_WARNING(QString("点到面测量点数错误: %1 (应为4)").arg(points.size()));
        return;
    }
    
    LOG_DEBUG(QString("渲染点到面测量 [%1]，点数=%2").arg(groupId).arg(points.size()));
    
    try {
        // 转换为米单位用于渲染
        QVector3D p1 = points[0] / 1000.0f;
        QVector3D p2 = points[1] / 1000.0f;
        QVector3D p3 = points[2] / 1000.0f; // 定义平面的点
        QVector3D p4 = points[3] / 1000.0f; // 目标点

        QColor pointColor = QColor(0, 255, 0);      // 绿色点
        QColor perpColor = QColor(0, 255, 0);       // 绿色垂线 (实线)
        QColor triangleColor = QColor(0, 255, 0);   // 绿色三角形 (虚线)
        QColor auxLineColor = QColor(255, 255, 0);   // 黄色辅助线 (虚线)

        // 1. 首先绘制所有点 - 这些很少出问题
        addSphere(p1, sphereRadius * 0.8f, pointColor, groupId + "_p1");
        addSphere(p2, sphereRadius * 0.8f, pointColor, groupId + "_p2");
        addSphere(p3, sphereRadius * 0.8f, pointColor, groupId + "_p3");
        addSphere(p4, sphereRadius, pointColor, groupId + "_p4");
        
        // 2. 计算平面法向量
        QVector3D v1 = p2 - p1;
        QVector3D v2 = p3 - p1;
        QVector3D normal = QVector3D::crossProduct(v1, v2);
        float normalLength = normal.length();
        
        if (normalLength < 1e-9f) {
            LOG_WARNING(QString("点到面测量 [%1] 的平面点共线或太近，无法计算法向量").arg(groupId));
            // 共线情况下，不绘制三角形和垂线，只显示文字
            addText(p4, measurement->getResult(), QColor(255, 255, 255), groupId + "_text");
            return; // 提前结束渲染
        }
        
        // 3. 绘制三角形边 (三条线)
        normal.normalize();
        double dashLen = 0.001; // 1mm
        double gapLen = 0.0005; // 0.5mm
        
        // 使用直接调用，防止addDashedLine内部复杂逻辑导致的问题
        addDashedLineSimple(p1, p2, triangleColor, dashLen, gapLen, groupId + "_edge1");
        addDashedLineSimple(p2, p3, triangleColor, dashLen, gapLen, groupId + "_edge2");
        addDashedLineSimple(p3, p1, triangleColor, dashLen, gapLen, groupId + "_edge3");
        
        // 4. 计算投影点
        float distToPlane = QVector3D::dotProduct(normal, p4 - p1);
        QVector3D projPoint = p4 - distToPlane * normal;
        
        // 5. 绘制垂线 (关键线)
        addLine(p4, projPoint, perpColor, groupId + "_perpLine");
        
        // 6. 绘制辅助线 (三条虚线)
        addDashedLineSimple(projPoint, p1, auxLineColor, dashLen, gapLen, groupId + "_aux1");
        addDashedLineSimple(projPoint, p2, auxLineColor, dashLen, gapLen, groupId + "_aux2");
        addDashedLineSimple(projPoint, p3, auxLineColor, dashLen, gapLen, groupId + "_aux3");
        
        // 7. 添加测量结果文本
        QVector3D labelPos = (p4 + projPoint) * 0.5f;
        addText(labelPos, measurement->getResult(), QColor(255, 255, 255), groupId + "_text");
        
        LOG_DEBUG(QString("点到面测量 [%1] 渲染完成").arg(groupId));
    } catch (const std::exception& e) {
        LOG_ERROR(QString("点到面测量 [%1] 渲染异常: %2").arg(groupId).arg(e.what()));
    } catch (...) {
        LOG_ERROR(QString("点到面测量 [%1] 渲染未知异常").arg(groupId));
    }
}

void PointCloudRenderer::renderAreaMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points, float sphereRadius, const QString& groupId)
{
    if (points.size() < 3) return;
    
    QColor areaColor(0, 0, 255); // 蓝色用于面积测量
    QVector3D centroid(0,0,0);

    // 绘制顶点和边线
    for (int i = 0; i < points.size(); ++i) {
        QVector3D currentPoint = points[i] / 1000.0f; // Convert to meters
        QVector3D nextPoint = points[(i + 1) % points.size()] / 1000.0f;

        addSphere(currentPoint, sphereRadius, areaColor, groupId + QString("_p%1").arg(i)); // Draw vertex
        addLine(currentPoint, nextPoint, areaColor, groupId + QString("_edge%1").arg(i)); // Draw edge
        centroid += currentPoint; // Accumulate for centroid calculation
    }

    // 计算质心
    if (!points.isEmpty()) {
        centroid /= points.size();
    }

    // 在质心位置添加面积结果文本
    addText(centroid, measurement->getResult(), QColor(255, 255, 255), groupId + "_text"); // White text

    LOG_DEBUG(QString("渲染点云面积测量 [%1]: %2").arg(groupId).arg(measurement->getResult()));
}

void PointCloudRenderer::renderPolylineMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points, float sphereRadius, const QString& groupId)
{
    if (points.size() < 2) return;
    
    QColor polylineColor(255, 165, 0); // 橙色用于折线
    QVector3D totalVector(0,0,0);
    int numSegments = 0;
    
    // 绘制所有线段和顶点
    for (int i = 0; i < points.size(); ++i) {
        QVector3D currentPoint = points[i] / 1000.0f; // Convert to meters
        
        // 绘制顶点
        addSphere(currentPoint, sphereRadius, polylineColor, groupId + QString("_p%1").arg(i));
        
        // 绘制连接线 (从第二个点开始)
        if (i > 0) {
            QVector3D prevPoint = points[i-1] / 1000.0f;
            addLine(prevPoint, currentPoint, polylineColor, groupId + QString("_line%1").arg(i-1));
            totalVector += (currentPoint - prevPoint);
            numSegments++;
        }
    }
    
    // 添加测量结果文本 - 在折线的大致中间位置显示
    if (points.size() > 0) {
        // 简单地在第一个和最后一个点的中点显示
        QVector3D firstPoint = points.first() / 1000.0f;
        QVector3D lastPoint = points.last() / 1000.0f;
        QVector3D labelPos = (firstPoint + lastPoint) * 0.5f;
        // 稍微向上偏移文本
        labelPos += QVector3D(0, sphereRadius * 2, 0); 
        addText(labelPos, measurement->getResult(), QColor(255, 255, 255), groupId + "_text");
    }

    LOG_DEBUG(QString("渲染点云折线测量 [%1]: %2").arg(groupId).arg(measurement->getResult()));
}

void PointCloudRenderer::renderProfileMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points, float sphereRadius, const QString& groupId)
{
    if (points.size() != 2) {
        LOG_WARNING(QString("剖面测量点数错误: %1 (应为2)").arg(points.size()));
        return;
    }
    
    // 转换为米单位用于渲染
    QVector3D point1Meters = points[0] / 1000.0f;
    QVector3D point2Meters = points[1] / 1000.0f;
    QColor color = QColor(255, 0, 255); // 紫色用于剖面线
    
    // 添加球体标记端点
    addSphere(point1Meters, sphereRadius, color, groupId + "_p1");
    addSphere(point2Meters, sphereRadius, color, groupId + "_p2");
    
    // 添加连接线
    addLine(point1Meters, point2Meters, color, groupId + "_line");
    
    // 添加测量结果文本 - 在线段中点显示
    QVector3D midPoint = (point1Meters + point2Meters) * 0.5f;
    addText(midPoint, measurement->getResult(), QColor(255, 255, 255), groupId + "_text");
    
    LOG_DEBUG(QString("渲染点云剖面测量 [%1]: %2").arg(groupId).arg(measurement->getResult()));
}

void PointCloudRenderer::renderMissingAreaMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points, float sphereRadius, const QString& groupId)
{
    // 缺失面积测量完成后只显示多边形（交点+后续点），至少需要3个点
    if (points.size() < 3) {
        LOG_WARNING(QString("缺失面积测量多边形点数错误: %1 (应为至少3个)").arg(points.size()));
        return;
    }

    // 将毫米单位转换为米单位用于渲染
    QVector<QVector3D> pointsInMeters;
    for (const auto& point : points) {
        pointsInMeters.append(point / 1000.0f);
    }
    
    // 设置颜色方案
    QColor intersectionColor(0, 255, 0); // 绿色 - 交点（第一个点）
    QColor pointColor(255, 0, 0);        // 红色 - 其他点
    QColor polygonColor(255, 255, 0);    // 黄色 - 多边形边缘

    // 传入的points就是多边形点（交点+后续点）
    // 第一个点是交点，其余是用户添加的点

    // 绘制交点（第一个点）
    addSphere(pointsInMeters[0], sphereRadius * 1.2f, intersectionColor, groupId + "_intersection");

    // 绘制其他点
    for (int i = 1; i < pointsInMeters.size(); ++i) {
        addSphere(pointsInMeters[i], sphereRadius, pointColor, groupId + QString("_p%1").arg(i));
    }

    // 绘制多边形边缘线 - 连续连接点，形成一个封闭的多边形
    if (pointsInMeters.size() >= 3) {  // 至少需要3个点才能形成多边形
        for (int i = 0; i < pointsInMeters.size(); ++i) {
            QVector3D current = pointsInMeters[i];
            QVector3D next = pointsInMeters[(i + 1) % pointsInMeters.size()]; // 循环到第一个点，形成封闭多边形
            addLine(current, next, polygonColor, groupId + QString("_edge_%1").arg(i));
        }

        // 找到合适的位置显示面积值 - 使用多边形中心点
        QVector3D sum(0, 0, 0);
        for (const QVector3D& point : pointsInMeters) {
            sum += point;
        }
        QVector3D textPosition = sum / pointsInMeters.size();

        // 稍微提升文本位置避免与多边形重叠
        textPosition.setZ(textPosition.z() + 0.01f);

        // 获取面积文本
        QString areaText = measurement->getResult();

        // 添加面积文本
        addText(textPosition, areaText, QColor(255, 255, 255), groupId + "_area_text");
    }
    
    // 提示必须调用此方法更新显示
    updateWidget();
}

void PointCloudRenderer::clearGeometryObjects()
{
    if (m_widget) {
        m_widget->clearGeometryObjects();
    }
}

void PointCloudRenderer::setPointSize(float size)
{
    if (m_widget) {
        m_widget->setPointSize(size);
    }
}

float PointCloudRenderer::getPointSize() const
{
    return m_widget ? m_widget->getPointSize() : 1.0f; // 如果 widget 无效，返回默认值
}

void PointCloudRenderer::addSphere(const QVector3D& center, float radius, const QColor& color, const QString& id)
{
    if (m_widget) {
        m_widget->addSphere(center, radius, color);
    }
}

void PointCloudRenderer::addLine(const QVector3D& start, const QVector3D& end, const QColor& color, const QString& id)
{
    if (m_widget) {
        m_widget->addLine(start, end, color);
    }
}

void PointCloudRenderer::addText(const QVector3D& position, const QString& text, const QColor& color, const QString& id)
{
    if (m_widget) {
        m_widget->addText(position, text, color);
    }
}

void PointCloudRenderer::updateWidget()
{
    if (m_widget) {
        m_widget->update(); // 调用 QOpenGLWidget::update() 触发重绘
    }
}

void PointCloudRenderer::addDashedLine(const QVector3D& start, const QVector3D& end, const QColor& color, 
                                        double dashLengthMeter, double gapLengthMeter, const QString& id)
{
    if (!m_widget) return;

    QVector3D vec = end - start;
    double totalDist = vec.length();
    if (totalDist < 1e-6 || dashLengthMeter <= 0 || gapLengthMeter < 0) {
        // 距离太近或参数无效，绘制实线
        addLine(start, end, color, id + "_solid"); // 调用现有的 addLine
        return;
    }

    QVector3D dir = vec.normalized();
    double currentDist = 0;
    bool drawDash = true;
    int segmentIndex = 0; // 用于生成唯一ID (如果需要的话)

    while (currentDist < totalDist) {
        double segmentStartDist = currentDist;
        double segmentLength = drawDash ? dashLengthMeter : gapLengthMeter;
        double segmentEndDist = currentDist + segmentLength;

        // 确保最后一段不会超出终点 end
        if (segmentEndDist > totalDist) {
            segmentEndDist = totalDist;
            segmentLength = totalDist - segmentStartDist;
        }

        if (drawDash && segmentLength > 1e-9) { // 仅绘制长度大于零的虚线段
            // 计算当前虚线段的起点和终点
            QVector3D dashStartPoint = start + dir * segmentStartDist;
            QVector3D dashEndPoint = start + dir * segmentEndDist;
            
            // 添加带ID的线段
            QString segmentId = QString("%1_dash%2").arg(id).arg(segmentIndex);
            m_widget->addLine(dashStartPoint, dashEndPoint, color);
            segmentIndex++;
        }

        currentDist = segmentEndDist; // 更新当前距离
        drawDash = !drawDash;       // 切换绘制状态
    }
}

// 添加一个更简化的虚线绘制函数，降低复杂度
void PointCloudRenderer::addDashedLineSimple(const QVector3D& start, const QVector3D& end, 
                                           const QColor& color, double dashLen, double gapLen, 
                                           const QString& id)
{
    if (!m_widget) return;
    
    QVector3D vec = end - start;
    double totalDist = vec.length();
    
    // 距离太短，直接画实线
    if (totalDist < 0.001) {
        addLine(start, end, color, id + "_solid");
        return;
    }
    
    // 计算方向向量
    QVector3D dir = vec.normalized();
    
    // 计算段数，根据线段长度动态调整，保持虚线密度一致
    // 基准：每0.003米一个虚线段，增加密度
    const double segmentLengthBase = 0.003; // 从0.005改为0.003，增加密度
    int numSegments = static_cast<int>(totalDist / segmentLengthBase);
    
    // 限制段数在合理范围内
    numSegments = std::max(8, numSegments); // 至少8段（从4改为8）
    numSegments = std::min(50, numSegments); // 最多50段（从30改为50）
    
    // 确保段数为偶数，便于创建均匀的虚线
    numSegments = numSegments + (numSegments % 2);
    
    double step = totalDist / numSegments;
    double dashRatio = 0.7; // 从0.6改为0.5，使虚线与间隙长度相等，更加协调
    
    LOG_DEBUG(QString("绘制虚线 [%1]: 总长=%2米, 分%3段")
             .arg(id).arg(totalDist, 0, 'f', 4).arg(numSegments));
    
    for (int i = 0; i < numSegments; i++) {
        // 只绘制偶数段 (0,2,4,...)
        if (i % 2 == 0) {
            double startPos = i * step;
            double endPos = startPos + step * dashRatio;
            
            if (endPos > totalDist) endPos = totalDist;
            
            QVector3D segStart = start + dir * startPos;
            QVector3D segEnd = start + dir * endPos;
            
            QString lineId = QString("%1_seg%2").arg(id).arg(i / 2);
            addLine(segStart, segEnd, color, lineId);
        }
    }
}

} // namespace Ui
} // namespace App
} // namespace SmartScope 