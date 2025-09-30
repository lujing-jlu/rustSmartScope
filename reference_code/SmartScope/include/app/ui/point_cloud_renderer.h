#pragma once

#include <QWidget>
#include <QVector3D>
#include <QColor>
#include <QString>
#include "app/ui/point_cloud_gl_widget.h"
#include "app/ui/measurement_object.h"

namespace SmartScope {
namespace App {
namespace Ui {

/**
 * @brief 点云渲染器类，负责在3D视图中绘制测量结果
 */
class PointCloudRenderer
{
public:
    /**
     * @brief 构造函数
     * @param widget PointCloudGLWidget 指针
     */
    explicit PointCloudRenderer(PointCloudGLWidget* widget);

    /**
     * @brief 析构函数
     */
    ~PointCloudRenderer();

    /**
     * @brief 在点云中渲染所有测量对象
     * @param measurements 测量对象列表
     */
    void renderMeasurements(const QVector<MeasurementObject*>& measurements);

    /**
     * @brief 清除所有几何对象
     */
    void clearGeometryObjects();

    /**
     * @brief 设置点大小
     * @param size 点大小
     */
    void setPointSize(float size);

    /**
     * @brief 获取点大小
     * @return 点大小
     */
    float getPointSize() const;
    
    /**
     * @brief 添加球体
     * @param center 球心坐标
     * @param radius 半径
     * @param color 颜色
     * @param id 对象标识
     */
    void addSphere(const QVector3D& center, float radius, const QColor& color, const QString& id = QString());
    
    /**
     * @brief 添加线段
     * @param start 起点坐标
     * @param end 终点坐标
     * @param color 颜色
     * @param id 对象标识
     */
    void addLine(const QVector3D& start, const QVector3D& end, const QColor& color, const QString& id = QString());
    
    /**
     * @brief 添加文本
     * @param position 位置坐标
     * @param text 文本内容
     * @param color 颜色
     * @param id 对象标识
     */
    void addText(const QVector3D& position, const QString& text, const QColor& color, const QString& id = QString());
    
    /**
     * @brief 添加虚线
     * @param start 起点坐标
     * @param end 终点坐标
     * @param color 颜色
     * @param dashLengthMeter 虚线段长度(米)
     * @param gapLengthMeter 间隔长度(米)
     * @param id 对象标识
     */
    void addDashedLine(const QVector3D& start, const QVector3D& end, const QColor& color, 
                      double dashLengthMeter, double gapLengthMeter, const QString& id = QString());

    /**
     * @brief 添加简化版虚线，更稳定且减少计算复杂度
     * @param start 起点坐标
     * @param end 终点坐标
     * @param color 颜色
     * @param dashLengthMeter 虚线段长度(米)
     * @param gapLengthMeter 间隔长度(米)
     * @param id 对象标识
     */
    void addDashedLineSimple(const QVector3D& start, const QVector3D& end, const QColor& color,
                           double dashLengthMeter, double gapLengthMeter, const QString& id = QString());

    /**
     * @brief 强制点云窗口重绘
     */
    void updateWidget();

    /**
     * @brief 渲染剖面测量对象
     * @param measurement 测量对象
     * @param points 测量点位
     * @param sphereRadius 球体半径
     * @param groupId 组标识
     */
    void renderProfileMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points, 
                                float sphereRadius, const QString& groupId);

private:
    /**
     * @brief 渲染长度测量对象
     * @param measurement 测量对象
     * @param points 测量点位
     * @param sphereRadius 球体半径
     * @param groupId 组标识
     */
    void renderLengthMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points, 
                                float sphereRadius, const QString& groupId);
    
    /**
     * @brief 渲染点到线测量对象
     * @param measurement 测量对象
     * @param points 测量点位
     * @param sphereRadius 球体半径
     * @param groupId 组标识
     */
    void renderPointToLineMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points, 
                                     float sphereRadius, const QString& groupId);
    
    /**
     * @brief 渲染深度(点到面)测量对象
     * @param measurement 测量对象
     * @param points 测量点位
     * @param sphereRadius 球体半径
     * @param groupId 组标识
     */
    void renderDepthMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points, 
                               float sphereRadius, const QString& groupId);
    
    /**
     * @brief 渲染面积测量对象
     * @param measurement 测量对象
     * @param points 测量点位
     * @param sphereRadius 球体半径
     * @param groupId 组标识
     */
    void renderAreaMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points, 
                              float sphereRadius, const QString& groupId);

    /**
     * @brief 渲染折线测量对象
     * @param measurement 测量对象
     * @param points 测量点位
     * @param sphereRadius 球体半径
     * @param groupId 组标识
     */
    void renderPolylineMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points,
                                 float sphereRadius, const QString& groupId);

    /**
     * @brief 渲染补缺测量对象
     * @param measurement 测量对象
     * @param points 测量点位
     * @param sphereRadius 球体半径
     * @param groupId 组标识
     */
    void renderMissingAreaMeasurement(MeasurementObject* measurement, const QVector<QVector3D>& points,
                                   float sphereRadius, const QString& groupId);

    PointCloudGLWidget* m_widget;   ///< 点云OpenGL控件
    float m_baseRadius = 0.001f;    ///< 基准半径，1mm (单位:米)
};

} // namespace Ui
} // namespace App
} // namespace SmartScope 