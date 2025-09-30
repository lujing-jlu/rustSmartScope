#ifndef POINT_CLOUD_GL_WIDGET_H
#define POINT_CLOUD_GL_WIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include <QVector3D>
#include <QQuaternion>
#include <vector>
#include <QPinchGesture>
#include "infrastructure/logging/logger.h"

// 定义点云中的几何对象类型
struct PointCloudSphere {
    QVector3D position;
    float radius;
    QColor color;
};

struct PointCloudLine {
    QVector3D start;
    QVector3D end;
    QColor color;
};

struct PointCloudText {
    QVector3D position;
    QString text;
    QColor color;
};

/**
 * @brief 使用OpenGL渲染3D点云的Widget
 */
class PointCloudGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父widget
     */
    explicit PointCloudGLWidget(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~PointCloudGLWidget();

    /**
     * @brief 更新点云数据
     * @param points 3D点坐标向量
     * @param colors 点颜色向量
     * @param centerPoints 是否将点云居中到原点，默认为true
     */
    void updatePointCloud(const std::vector<QVector3D>& points, const std::vector<QVector3D>& colors, bool centerPoints = true);
    
    /**
     * @brief 清空点云数据
     */
    void clearPointCloud();
    
    /**
     * @brief 添加一个球体
     * @param position 球体中心位置
     * @param radius 球体半径
     * @param color 球体颜色
     */
    void addSphere(const QVector3D& position, float radius, const QColor& color);
    
    /**
     * @brief 添加一条线
     * @param start 线段起点
     * @param end 线段终点
     * @param color 线段颜色
     */
    void addLine(const QVector3D& start, const QVector3D& end, const QColor& color);
    
    /**
     * @brief 添加文本标签
     * @param position 文本位置
     * @param text 文本内容
     * @param color 文本颜色
     */
    void addText(const QVector3D& position, const QString& text, const QColor& color);
    
    /**
     * @brief 清除所有添加的几何对象（球体、线段、文本）
     */
    void clearGeometryObjects();
    
    /**
     * @brief 设置旋转角度
     * @param x X轴旋转角度
     * @param y Y轴旋转角度
     * @param z Z轴旋转角度（暂未使用）
     */
    void setRotation(float x, float y, float z);
    
    /**
     * @brief 设置平移
     * @param x X轴平移量
     * @param y Y轴平移量
     * @param z Z轴平移量
     */
    void setTranslation(float x, float y, float z);
    
    /**
     * @brief 设置缩放
     * @param scale 缩放因子
     */
    void setScale(float scale);
    
    /**
     * @brief 设置点大小
     * @param size 点大小（像素）
     */
    void setPointSize(float size);
    
    /**
     * @brief 获取当前点大小
     * @return 点大小
     */
    float getPointSize() const { return m_pointSize; }
    
    /**
     * @brief 重置视图
     */
    void resetView();
    
    /**
     * @brief 自动调整视图以适合点云
     */
    void autoAdjustView();
    
    /**
     * @brief 获取点云中的点数量
     * @return 点数量
     */
    size_t getPointCount() const { return m_points.size(); }
    
    /**
     * @brief 设置以2D图像视角显示点云
     * 将点云的视角调整为与2D图像相同
     */
    void set2DImageView();
    
    /**
     * @brief 设置是否显示坐标轴
     * @param show 是否显示
     */
    void setShowAxes(bool show) { m_showAxes = show; update(); }
    
    /**
     * @brief 获取是否显示坐标轴
     * @return 是否显示
     */
    bool showAxes() const { return m_showAxes; }
    
    /**
     * @brief 获取点云包围盒中心点
     * @return 包围盒中心点坐标
     */
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

signals:
    /**
     * @brief 当在点云上选择一个点时发出此信号
     * @param point 被选中的3D点坐标（世界坐标系，单位：米）
     * @param pixelCoords 对应的窗口像素坐标
     */
    void pointSelected(QVector3D point, QPoint pixelCoords);

protected:
    /**
     * @brief OpenGL初始化
     */
    void initializeGL() override;
    
    /**
     * @brief OpenGL窗口大小改变
     */
    void resizeGL(int w, int h) override;
    
    /**
     * @brief OpenGL绘制
     */
    void paintGL() override;
    
    /**
     * @brief 鼠标按下事件
     */
    void mousePressEvent(QMouseEvent *event) override;
    
    /**
     * @brief 鼠标移动事件
     */
    void mouseMoveEvent(QMouseEvent *event) override;
    
    /**
     * @brief 鼠标滚轮事件
     */
    void wheelEvent(QWheelEvent *event) override;
    
    /**
     * @brief 键盘按键事件处理
     */
    void keyPressEvent(QKeyEvent *event) override;
    
    /**
     * @brief 事件处理函数，用于处理手势和触摸事件
     */
    bool event(QEvent *event) override;
    
    /**
     * @brief 处理捏合手势
     */
    bool gestureEvent(QGestureEvent *event);
    
    /**
     * @brief 触摸事件处理函数
     */
    void touchBeginEvent(QTouchEvent *event);
    void touchMoveEvent(QTouchEvent *event);
    void touchEndEvent(QTouchEvent *event);

private:
    /**
     * @brief 初始化着色器
     */
    void initShaders();
    
    /**
     * @brief 设置顶点缓冲区
     */
    void setupVertexBuffers();
    
    /**
     * @brief 计算点云的包围盒
     */
    void calculateBoundingBox();
    
    /**
     * @brief 初始化坐标轴
     */
    void initAxes();
    
    /**
     * @brief 绘制坐标轴
     */
    void drawAxes();
    
    /**
     * @brief 将世界坐标转换为屏幕坐标
     * @param worldPos 世界坐标
     * @return 屏幕坐标
     */
    QVector3D worldToScreen(const QVector3D& worldPos);

    /**
     * @brief 将屏幕坐标转换为世界坐标 (近似)
     * @param screenPos 屏幕坐标
     * @return 世界坐标 (可能需要进一步处理以获得准确深度)
     */
    QVector3D screenToWorld(const QPoint& screenPos);
    
    // OpenGL相关成员变量
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
    QOpenGLBuffer m_colorVBO;
    QOpenGLShaderProgram m_program;
    QMatrix4x4 m_projection;
    QMatrix4x4 m_view;
    QMatrix4x4 m_model;
    
    // 点云数据
    std::vector<QVector3D> m_points;
    std::vector<QVector3D> m_colors;
    
    // 几何对象
    std::vector<PointCloudSphere> m_spheres;
    std::vector<PointCloudLine> m_lines;
    std::vector<PointCloudText> m_texts;
    
    // 视图变换参数
    QQuaternion m_rotationQuaternion;
    float m_translateX = 0.0f;
    float m_translateY = 0.0f;
    float m_translateZ = -1.0f;
    float m_scale = 0.5f;
    float m_pointSize = 5.0f;
    
    // 鼠标和触摸相关
    QPoint m_lastPos;
    int m_touchPoints = 0;
    
    // 包围盒信息
    QVector3D m_boundingBoxMin;
    QVector3D m_boundingBoxMax;
    QVector3D m_boundingBoxCenter;
    float m_boundingBoxSize = 0.0f;
    
    // 坐标轴相关
    bool m_showAxes = true;
    QOpenGLVertexArrayObject m_axesVAO;
    QOpenGLBuffer m_axesVBO;
    QOpenGLBuffer m_axesColorVBO;
    QOpenGLShaderProgram m_axesProgram;
    
    // 标志变量
    bool m_autoAdjustOnNextPaint = false;
};

#endif // POINT_CLOUD_GL_WIDGET_H 