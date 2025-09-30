#include "app/ui/point_cloud_gl_widget.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDebug>
#include <QPainter>
#include <QGestureEvent>
#include <QTouchEvent>
#include <QtMath>
#include <limits>
#include <QOpenGLFramebufferObject> // 用于读取像素颜色/深度

// 顶点着色器
static const char *vertexShaderSource = R"(
    #version 100
    attribute vec3 position;
    attribute vec3 color;
    varying vec3 fragColor;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform float pointSize;
    uniform float scale;  // 添加缩放因子
    void main()
    {
        vec4 viewPos = view * model * vec4(position, 1.0);
        gl_Position = projection * viewPos;
        
            // 计算点到摄像机的距离（兼容 +Z 为前方：使用绝对值）
    float distance = length(viewPos.xyz);
    
    // 进一步降低衰减系数，确保远处点也有足够大小
    float distanceFactor = 1.0 / (1.0 + 0.8 * distance);
        
        // 增大最大点大小增益限制为2.0倍，使近处点更明显
        distanceFactor = min(distanceFactor, 2.0);
        
        // 显著增大最小点大小，确保远处点清晰可见
        float finalPointSize = max(pointSize * distanceFactor * scale, 2.0);
        
        // 应用计算后的点大小
        gl_PointSize = finalPointSize;
        
        fragColor = color;
    }
)";

// 片段着色器
static const char *fragmentShaderSource = R"(
    #version 100
    precision mediump float;
    varying vec3 fragColor;
    void main()
    {
        // 转换坐标到[-1,1]范围
        vec2 coord = gl_PointCoord * 2.0 - 1.0;
        
        // 计算到圆心的距离
        float r = length(coord);
        
        // 如果超出单位圆，丢弃片段
        if(r > 1.0) {
            discard;
        }
        
        // 直接使用传入的颜色，不添加光照效果
        gl_FragColor = vec4(fragColor, 1.0);
    }
)";

PointCloudGLWidget::PointCloudGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_vbo(QOpenGLBuffer::VertexBuffer)
    , m_colorVBO(QOpenGLBuffer::VertexBuffer)
    , m_rotationQuaternion()
{
    LOG_INFO("初始化点云渲染控件");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    // 设置初始Z轴正方向视角（+Z为前方）
    m_translateZ = 15.0f;
    m_scale = 1.2f;       // 默认缩放比例增大到1.2倍
    m_translateX = 0.0f;  // 水平居中
    m_translateY = 0.0f;  // 垂直居中
    
    // 初始化点云参数
    m_pointSize = 4.0f;   // 增大默认点大小，从3.0f增加到4.0f
    m_showAxes = false;   // 默认不显示坐标轴
    m_autoAdjustOnNextPaint = false; // 禁用自动调整视图
    
    // 初始化包围盒
    m_boundingBoxMin = QVector3D(std::numeric_limits<float>::max(), 
                                std::numeric_limits<float>::max(), 
                                std::numeric_limits<float>::max());
    m_boundingBoxMax = QVector3D(-std::numeric_limits<float>::max(), 
                                -std::numeric_limits<float>::max(), 
                                -std::numeric_limits<float>::max());
    
    // 启用手势识别
    grabGesture(Qt::PinchGesture);
    setAttribute(Qt::WA_AcceptTouchEvents);
    
    LOG_INFO("点云控件初始化完成，默认设置为Z轴负方向视角");
}

PointCloudGLWidget::~PointCloudGLWidget()
{
    LOG_INFO("销毁点云渲染控件");
    makeCurrent();
    m_vbo.destroy();
    m_colorVBO.destroy();
    m_vao.destroy();
    m_program.removeAllShaders();
    doneCurrent();
}

void PointCloudGLWidget::initializeGL()
{
    LOG_INFO("初始化OpenGL环境");
    initializeOpenGLFunctions();
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    
    // 启用深度测试并设置深度函数
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // 注意：GL_POINT_SMOOTH已在现代OpenGL中弃用
    // 我们将使用着色器实现点的平滑效果
    
    // 禁用面剔除，确保所有点都被渲染
    glDisable(GL_CULL_FACE);
    
    // 不使用GL_PROGRAM_POINT_SIZE，改为在着色器中控制点大小
    
    LOG_INFO("OpenGL深度测试和点渲染设置已应用");
    
    initShaders();
    
    m_view.setToIdentity();
    m_view.translate(0.0f, 0.0f, m_translateZ);
    
    setupVertexBuffers();
    
    // 初始化坐标轴
    initAxes();
}

void PointCloudGLWidget::initShaders()
{
    LOG_INFO("初始化OpenGL着色器");
    if (!m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        LOG_ERROR("顶点着色器编译失败");
    }
    
    if (!m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        LOG_ERROR("片段着色器编译失败");
    }
    
    if (!m_program.link()) {
        LOG_ERROR("着色器程序链接失败");
    }
}

void PointCloudGLWidget::setupVertexBuffers()
{
    LOG_INFO("设置OpenGL顶点缓冲区");
    m_vao.create();
    m_vao.bind();
    
    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    
    m_program.enableAttributeArray(0);
    m_program.setAttributeBuffer(0, GL_FLOAT, 0, 3, 0);
    
    m_colorVBO.create();
    m_colorVBO.bind();
    m_colorVBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    
    m_program.enableAttributeArray(1);
    m_program.setAttributeBuffer(1, GL_FLOAT, 0, 3, 0);
    
    m_vao.release();
    m_vbo.release();
    m_colorVBO.release();
}

void PointCloudGLWidget::resizeGL(int w, int h)
{
    float aspect = float(w) / float(h ? h : 1);
    const float zNear = 0.01f;
    const float zFar = 500.0f;
    const float fov = 45.0f;
    
    m_projection.setToIdentity();
    // Qt 的 perspective 定义固定相机在原点朝 -Z。为兼容 +Z 前方渲染，我们在 view 做了 -m_translateZ 修正，投影保持不变
    m_projection.perspective(fov, aspect, zNear, zFar);
}

void PointCloudGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (m_points.empty()) {
        // 如果没有点云数据，绘制提示文字
        QPainter painter(this);
        painter.setPen(Qt::black);  // 改为黑色文字，适应浅灰色背景
        painter.setFont(QFont("Arial", 14));
        painter.drawText(rect(), Qt::AlignCenter, "等待点云数据...");
        return;
    }
    
    // 如果需要自动调整视图
    if (m_autoAdjustOnNextPaint) {
        autoAdjustView();
        m_autoAdjustOnNextPaint = false;
    }
    
    m_program.bind();
    
    // 重新组织模型视图矩阵计算顺序
    m_model.setToIdentity();
    
    // 由于点云已经在原点，直接应用旋转和缩放
    m_model.rotate(m_rotationQuaternion);
    m_model.scale(m_scale);
    
    // 记录关键变换信息
    LOG_INFO(QString("绘制点云 - 缩放: %1").arg(m_scale));
    
    // 视图矩阵设置
    m_view.setToIdentity();
    // +Z 为前方：先沿 -Z 平移，再在视图中对Z取反，把世界+Z映射到相机- Z
    m_view.translate(m_translateX, m_translateY, -m_translateZ);
    m_view.scale(1.0f, 1.0f, -1.0f);
    
    // 设置着色器参数
    m_program.setUniformValue("model", m_model);
    m_program.setUniformValue("view", m_view);
    m_program.setUniformValue("projection", m_projection);
    m_program.setUniformValue("pointSize", m_pointSize);
    m_program.setUniformValue("scale", m_scale);  // 设置缩放因子
    
    // 保证深度测试有效
    glEnable(GL_DEPTH_TEST);
    
    // 绘制点云
    m_vao.bind();
    glDrawArrays(GL_POINTS, 0, m_points.size());
    m_vao.release();
    
    m_program.release();
    
    // 绘制坐标轴
    if (m_showAxes) {
        drawAxes();
    }
    
    // 使用QPainter在OpenGL上绘制几何对象
    if (!m_spheres.empty() || !m_lines.empty() || !m_texts.empty()) {
        // 开启QPainter
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        
        // 绘制球体（绘制为2D圆）
        for (const auto& sphere : m_spheres) {
            // 将3D坐标转换为窗口坐标
            QVector3D screenPos = worldToScreen(sphere.position);
            
            // 放宽可见性条件，完全移除Z坐标限制，确保所有球体都能显示
            // 即使在远处或视锥体外，只要能投影到屏幕上就显示
            if (true) {  // 移除Z坐标检查，确保所有球体都显示
                // 改进的球体半径计算，使其随视角变化更加平缓
                // 基础屏幕半径（像素单位）
                float baseScreenRadius = 8.0f;  // 增大基础半径
                
                // 计算距离因子，限制在合理范围内
                float distanceFactor = qMin(1.0f, 0.5f / qMax(0.05f, qAbs(screenPos.z())));
                
                // 将3D半径转换为屏幕半径，加入缩放因子但影响减小
                float screenRadius = baseScreenRadius + sphere.radius * m_scale * 30.0f * distanceFactor;
                
                // 确保半径在合理范围内，增大最小半径，使远处球体更明显
                screenRadius = qBound(5.0f, screenRadius, 18.0f);
                
                // 设置颜色和画笔
                painter.setPen(QPen(sphere.color, 2));
                painter.setBrush(QBrush(sphere.color.lighter(130)));
                
                // 绘制圆形
                painter.drawEllipse(
                    QPointF(screenPos.x(), screenPos.y()), 
                    screenRadius, 
                    screenRadius
                );
            }
        }
        
        // 绘制线段
        for (const auto& line : m_lines) {
            // 将3D坐标转换为窗口坐标
            QVector3D startScreen = worldToScreen(line.start);
            QVector3D endScreen = worldToScreen(line.end);
            
            // 完全移除Z坐标限制，确保所有线段都能显示
            if (true) {  // 移除Z坐标检查，确保所有线段都显示
                // 调整线宽，使线段更加明显
                int lineWidth = 4;  // 增大线宽
                painter.setPen(QPen(line.color, lineWidth, Qt::SolidLine, Qt::RoundCap));
                
                LOG_INFO(QString("绘制线段 - 屏幕坐标 起点: (%1, %2, %3), 终点: (%4, %5, %6)")
                       .arg(startScreen.x()).arg(startScreen.y()).arg(startScreen.z())
                       .arg(endScreen.x()).arg(endScreen.y()).arg(endScreen.z()));
                
                // 绘制线段
                painter.drawLine(
                    QPointF(startScreen.x(), startScreen.y()),
                    QPointF(endScreen.x(), endScreen.y())
                );
            } else {
                LOG_INFO(QString("线段在视口外 - 屏幕坐标 起点: (%1, %2, %3), 终点: (%4, %5, %6)")
                       .arg(startScreen.x()).arg(startScreen.y()).arg(startScreen.z())
                       .arg(endScreen.x()).arg(endScreen.y()).arg(endScreen.z()));
            }
        }
        
        // 绘制文本标签
        for (const auto& textObj : m_texts) {
            // 将3D坐标转换为窗口坐标
            QVector3D screenPos = worldToScreen(textObj.position);
            
            // 完全移除Z坐标限制，确保所有文本都能显示
            if (true) {  // 移除Z坐标检查，确保所有文本都显示
                // 计算文本尺寸，用于绘制背景矩形
                QFont textFont("Arial", 16, QFont::Bold);  // 增大字体
                painter.setFont(textFont);
                QFontMetrics fm(textFont);
                QRect textRect = fm.boundingRect(textObj.text);
                
                // 在文本周围添加一些边距
                QRect bgRect = textRect.adjusted(-10, -6, 10, 6);  // 增大边距
                
                // 不要使用translate，直接在正确位置绘制
                int textX = screenPos.x() + 5;
                int textY = screenPos.y() - 5;
                bgRect.moveTo(textX, textY - textRect.height());
                
                // 绘制半透明深色背景
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 0, 0, 180)); // 半透明黑色
                painter.drawRoundedRect(bgRect, 6, 6); // 增大圆角
                
                // 设置文本颜色
                painter.setPen(textObj.color);
                
                // 绘制文本，精确定位到背景矩形的正确位置
                painter.drawText(
                    QPoint(textX + 5, textY - 5),  // 调整位置
                    textObj.text
                );
            }
        }
        
        // 结束QPainter
        painter.end();
    }
}

void PointCloudGLWidget::updatePointCloud(const std::vector<QVector3D>& points, const std::vector<QVector3D>& colors, bool centerPoints)
{
    LOG_INFO(QString("更新点云数据: %1个点").arg(points.size()));
    m_points = points;
    m_colors = colors;
    
    if (!m_points.empty()) {
        // 计算点云包围盒
        calculateBoundingBox();
        
        // 将所有点云数据移动到原点 - 只在centerPoints为true时执行
        if (centerPoints) {
            QVector3D offset = m_boundingBoxCenter;
            for (QVector3D& point : m_points) {
                point -= offset;
            }
            
            // 更新包围盒中心为原点
            m_boundingBoxMin -= offset;
            m_boundingBoxMax -= offset;
            m_boundingBoxCenter = QVector3D(0.0f, 0.0f, 0.0f);
            
            LOG_INFO(QString("将点云移动到原点 - 偏移量: (%1, %2, %3)")
                   .arg(offset.x(), 0, 'f', 4)
                   .arg(offset.y(), 0, 'f', 4)
                   .arg(offset.z(), 0, 'f', 4));
        } else {
            LOG_INFO("保持点云在原始位置，不移动到原点");
        }
        
        // 更新VBO
        m_vbo.bind();
        m_vbo.allocate(m_points.data(), m_points.size() * sizeof(QVector3D));
        m_vbo.release();
        
        // 启用自动调整视图，确保每次加载新点云时都有良好的视角
        m_autoAdjustOnNextPaint = true;
    }
    
    if (!m_colors.empty()) {
        m_colorVBO.bind();
        m_colorVBO.allocate(m_colors.data(), m_colors.size() * sizeof(QVector3D));
        m_colorVBO.release();
    }
    
    update();
}

void PointCloudGLWidget::mousePressEvent(QMouseEvent *event)
{
    m_lastPos = event->pos();
    
    // 处理左键点击以选择点
    if (event->button() == Qt::LeftButton) {
        // 获取点击的3D点
        QVector3D clickedPoint = screenToWorld(event->pos());
        
        // 发射点选择信号
        emit pointSelected(clickedPoint, event->pos());
        
        LOG_INFO(QString("点云控件左键点击 - 屏幕坐标: (%1, %2), 世界坐标: (%3, %4, %5)")
               .arg(event->pos().x()).arg(event->pos().y())
               .arg(clickedPoint.x(), 0, 'f', 4)
               .arg(clickedPoint.y(), 0, 'f', 4)
               .arg(clickedPoint.z(), 0, 'f', 4));
    }
}

void PointCloudGLWidget::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->pos().x() - m_lastPos.x();
    int dy = event->pos().y() - m_lastPos.y();
    bool isTouchEvent = (m_touchPoints > 0); // 检查是否来自触摸事件

    if ((event->buttons() & Qt::LeftButton && !isTouchEvent) || (isTouchEvent && m_touchPoints == 1)) {
        // 左键拖动 或 单指触摸 调整旋转角度
        // ---------> 使用四元数更新旋转 <---------
        float angularSpeed = 0.25f;

        // 绕视图 Y 轴旋转 (基于鼠标水平移动 dx)
        QQuaternion rotationY = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, angularSpeed * dx);
        // 绕视图 X 轴旋转 (基于鼠标垂直移动 dy)
        QQuaternion rotationX = QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, angularSpeed * dy);

        // 组合新旋转并应用到现有旋转
        m_rotationQuaternion = rotationY * rotationX * m_rotationQuaternion;
        m_rotationQuaternion.normalize();
        // ---------> 更新结束 <---------

        LOG_INFO(QString("旋转点云 - 触控点: %1, 是触摸事件: %2")
               .arg(m_touchPoints).arg(isTouchEvent));
        update();

    } else if (event->buttons() & Qt::RightButton) {
        // 右键拖动调整视图平移 (保持不变)
        float translateSpeed = 0.002f * std::abs(m_translateZ) / m_scale;
        m_translateX += dx * translateSpeed;
        m_translateY -= dy * translateSpeed; // Qt Y轴向下，屏幕Y轴向上，所以用 -= dy
        update();
    }

    m_lastPos = event->pos();
}

void PointCloudGLWidget::wheelEvent(QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QPoint numDegrees = event->angleDelta() / 8;
#else
    QPoint numDegrees = event->angleDelta() / 8;
#endif
    
    if (!numDegrees.isNull()) {
        m_scale += numDegrees.y() / 100.0f;
        m_scale = qBound(0.1f, m_scale, 10.0f);
        update();
    }
    
    event->accept();
}

bool PointCloudGLWidget::event(QEvent *event)
{
    switch (event->type()) {
        case QEvent::TouchBegin:
            touchBeginEvent(static_cast<QTouchEvent*>(event));
            return true;
        case QEvent::TouchUpdate:
            touchMoveEvent(static_cast<QTouchEvent*>(event));
            return true;
        case QEvent::TouchEnd:
            touchEndEvent(static_cast<QTouchEvent*>(event));
            return true;
        case QEvent::Gesture:
            return gestureEvent(static_cast<QGestureEvent*>(event));
    }
    return QOpenGLWidget::event(event);
}

void PointCloudGLWidget::touchBeginEvent(QTouchEvent *event)
{
    m_touchPoints = event->touchPoints().size();
    LOG_INFO(QString("触摸开始 - 触控点数量: %1").arg(m_touchPoints));
    event->accept();
}

void PointCloudGLWidget::touchMoveEvent(QTouchEvent *event)
{
    m_touchPoints = event->touchPoints().size();
    LOG_INFO(QString("触摸移动 - 触控点数量: %1").arg(m_touchPoints));

    // 如果是单指触控，处理旋转
    if (m_touchPoints == 1) {
        QList<QTouchEvent::TouchPoint> touchPoints = event->touchPoints();
        if (!touchPoints.isEmpty()) {
            const QTouchEvent::TouchPoint &tp = touchPoints.first();
            QPointF pos = tp.pos();
            QPointF lastPos = tp.lastPos();

            int dx = pos.x() - lastPos.x();
            int dy = pos.y() - lastPos.y();

            // ---------> 使用四元数更新旋转 <---------
            float angularSpeed = 0.25f; // 使用与鼠标一致的速度

            // 绕视图 Y 轴旋转 (基于水平移动 dx)
            QQuaternion rotationY = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, -angularSpeed * dx);
            // 绕视图 X 轴旋转 (基于垂直移动 dy)
            QQuaternion rotationX = QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, -angularSpeed * dy);

            // 组合新旋转并应用到现有旋转
            m_rotationQuaternion = rotationY * rotationX * m_rotationQuaternion;
            m_rotationQuaternion.normalize();
            // ---------> 更新结束 <---------
            
            LOG_INFO(QString("触摸旋转点云 - dx: %1, dy: %2").arg(dx).arg(dy));
            update();
        }
    }
    // 如果是双指及以上触控，处理平移（双指拖动平移）
    else if (m_touchPoints >= 2) {
        const QList<QTouchEvent::TouchPoint> touchPoints = event->touchPoints();
        if (touchPoints.size() >= 2) {
            QPointF posAvg(0.0, 0.0), lastAvg(0.0, 0.0);
            int useCount = qMin(2, touchPoints.size());
            for (int i = 0; i < useCount; ++i) {
                posAvg += touchPoints[i].pos();
                lastAvg += touchPoints[i].lastPos();
            }
            posAvg /= useCount;
            lastAvg /= useCount;

            float dx = static_cast<float>(posAvg.x() - lastAvg.x());
            float dy = static_cast<float>(posAvg.y() - lastAvg.y());

            float translateSpeed = 0.002f * std::abs(m_translateZ) / m_scale;
            m_translateX += dx * translateSpeed;
            m_translateY -= dy * translateSpeed; // 屏幕Y向下，场景Y向上

            LOG_INFO(QString("触摸平移点云 - dx: %1, dy: %2, speed: %3").arg(dx).arg(dy).arg(translateSpeed));
            update();
        }
    }

    event->accept();
}

void PointCloudGLWidget::touchEndEvent(QTouchEvent *event)
{
    m_touchPoints = event->touchPoints().size();
    LOG_INFO(QString("触摸结束 - 触控点数量: %1").arg(m_touchPoints));
    event->accept();
}

bool PointCloudGLWidget::gestureEvent(QGestureEvent *event)
{
    if (QGesture *pinch = event->gesture(Qt::PinchGesture)) {
        QPinchGesture *pinchGesture = static_cast<QPinchGesture *>(pinch);
        
        // 处理捏合事件的比例变化
        QPinchGesture::ChangeFlags changeFlags = pinchGesture->changeFlags();
        if (changeFlags & QPinchGesture::ScaleFactorChanged) {
            // 捏合比例因子，小于1为缩小，大于1为放大
            qreal scaleFactor = pinchGesture->scaleFactor();
            
            // 调整缩放 - 增大系数，从0.5f改为2.0f，使缩放变化更明显
            if (scaleFactor > 1.0) {
                // 放大
                m_scale *= 1.0f + (scaleFactor - 1.0f) * 2.0f;
            } else {
                // 缩小
                m_scale *= 1.0f - (1.0f - scaleFactor) * 2.0f;
            }
            
            // 限制缩放范围，扩大最大值从10.0f到20.0f
            m_scale = qBound(0.1f, m_scale, 20.0f);
            LOG_INFO(QString("调整缩放: %1").arg(m_scale));
            update();
        }
        
        // 标记事件已处理
        event->accept();
        return true;
    }
    
    return false;
}

void PointCloudGLWidget::setRotation(float x, float y, float z)
{
    // 这个函数现在意义不大了，如果外部需要设置特定角度，
    // 需要转换为四元数或提供新的基于四元数的接口。
    // 暂时注释掉，或根据需要修改。
    // m_rotationX = x;
    // m_rotationY = y;
    // update();
    LOG_WARNING("setRotation(float, float, float) is deprecated. Use quaternion-based methods if needed.");
}

void PointCloudGLWidget::setTranslation(float x, float y, float z)
{
    m_translateX = x;
    m_translateY = y;
    m_translateZ = z;
    update();
}

void PointCloudGLWidget::setScale(float scale)
{
    m_scale = scale;
    update();
}

void PointCloudGLWidget::setPointSize(float size)
{
    m_pointSize = size;
    update();
}

void PointCloudGLWidget::resetView()
{
    LOG_INFO("重置点云视图到Z轴负方向视角");

    // ---------> 重置四元数 <---------
    m_rotationQuaternion = QQuaternion(); // 重置为单位四元数
    // ---------> 重置结束 <---------

    // 设置默认缩放和平移
    m_scale = 1.5f;
    m_translateX = 0.0f;
    m_translateY = 0.0f;

    // 根据点云大小计算适当的视距（+Z 为前方）
    if (!m_points.empty() && m_boundingBoxSize > 0.0f) {
        m_translateZ = m_boundingBoxSize * 3.0f;
    } else {
        m_translateZ = 15.0f;
    }

    // 禁用坐标轴
    m_showAxes = false;

    // 禁用自动调整视图
    m_autoAdjustOnNextPaint = false;

    // 立即更新
    update();

    LOG_INFO(QString("重置视图完成 - 缩放: %1, 视距: %2").arg(m_scale).arg(m_translateZ));
}

void PointCloudGLWidget::calculateBoundingBox()
{
    if (m_points.empty()) {
        LOG_WARNING("计算包围盒失败：点云为空");
        return;
    }
    
    // 初始化包围盒
    m_boundingBoxMin = QVector3D(std::numeric_limits<float>::max(), 
                                std::numeric_limits<float>::max(), 
                                std::numeric_limits<float>::max());
    m_boundingBoxMax = QVector3D(-std::numeric_limits<float>::max(), 
                                -std::numeric_limits<float>::max(), 
                                -std::numeric_limits<float>::max());
    
    // 计算包围盒
    for (const QVector3D& point : m_points) {
        m_boundingBoxMin.setX(qMin(m_boundingBoxMin.x(), point.x()));
        m_boundingBoxMin.setY(qMin(m_boundingBoxMin.y(), point.y()));
        m_boundingBoxMin.setZ(qMin(m_boundingBoxMin.z(), point.z()));
        
        m_boundingBoxMax.setX(qMax(m_boundingBoxMax.x(), point.x()));
        m_boundingBoxMax.setY(qMax(m_boundingBoxMax.y(), point.y()));
        m_boundingBoxMax.setZ(qMax(m_boundingBoxMax.z(), point.z()));
    }
    
    // 计算包围盒中心 - 确保使用准确的坐标计算
    m_boundingBoxCenter = QVector3D(
        (m_boundingBoxMin.x() + m_boundingBoxMax.x()) * 0.5f,
        (m_boundingBoxMin.y() + m_boundingBoxMax.y()) * 0.5f,
        (m_boundingBoxMin.z() + m_boundingBoxMax.z()) * 0.5f
    );
    
    // 计算包围盒大小（最大边长）
    float sizeX = qAbs(m_boundingBoxMax.x() - m_boundingBoxMin.x());
    float sizeY = qAbs(m_boundingBoxMax.y() - m_boundingBoxMin.y());
    float sizeZ = qAbs(m_boundingBoxMax.z() - m_boundingBoxMin.z());
    m_boundingBoxSize = qMax(qMax(sizeX, sizeY), sizeZ);
    
    LOG_INFO(QString("计算点云包围盒 - 点数: %1, 最小点: (%2, %3, %4), 最大点: (%5, %6, %7), 中心: (%8, %9, %10), 大小: %11")
           .arg(m_points.size())
           .arg(m_boundingBoxMin.x(), 0, 'f', 4).arg(m_boundingBoxMin.y(), 0, 'f', 4).arg(m_boundingBoxMin.z(), 0, 'f', 4)
           .arg(m_boundingBoxMax.x(), 0, 'f', 4).arg(m_boundingBoxMax.y(), 0, 'f', 4).arg(m_boundingBoxMax.z(), 0, 'f', 4)
           .arg(m_boundingBoxCenter.x(), 0, 'f', 4).arg(m_boundingBoxCenter.y(), 0, 'f', 4).arg(m_boundingBoxCenter.z(), 0, 'f', 4)
           .arg(m_boundingBoxSize, 0, 'f', 4));
}

void PointCloudGLWidget::autoAdjustView()
{
    if (m_points.empty() || m_boundingBoxSize <= 0.0f) {
        LOG_WARNING("自动调整视图失败：点云为空或包围盒大小无效");
        return;
    }
    
    // 记录调整前的视角参数
    LOG_INFO(QString("自动调整视图前 - 缩放: %1, 平移: (%2, %3, %4)")
           .arg(m_scale)
           .arg(m_translateX).arg(m_translateY).arg(m_translateZ));
    
    // 根据包围盒大小计算合适的相机距离
    float aspectRatio = float(width()) / float(height() ? height() : 1);
    float fovRadians = 45.0f * M_PI / 180.0f;
    float distance = m_boundingBoxSize / (2.0f * tanf(fovRadians / 2.0f));
    
    // 设置适当的视角 - 显著增大距离倍数，确保相机在点云之外
    m_translateZ = -distance * 3.0f;  // 显著增大倍数，从1.2倍增加到3.0倍
    
    // 增大缩放比例，确保点云依然清晰可见
    m_scale = 1.5f;  // 增大默认缩放比例
    
    // 重置XY平移，因为点云中心已在模型矩阵中对齐
    m_translateX = 0.0f;
    m_translateY = 0.0f;
    
    LOG_INFO(QString("自动调整视图后 - 点云包围盒大小: %1, 视野: %2°, 缩放: %3, 距离: %4, 平移: (%5, %6, %7)")
           .arg(m_boundingBoxSize, 0, 'f', 4)
           .arg(45.0f)
           .arg(m_scale)
           .arg(distance, 0, 'f', 4)
           .arg(m_translateX, 0, 'f', 4)
           .arg(m_translateY, 0, 'f', 4)
           .arg(m_translateZ, 0, 'f', 4));
    
    update();
}

void PointCloudGLWidget::clearPointCloud()
{
    LOG_INFO("清空点云数据");
    m_points.clear();
    m_colors.clear();
    
    // 重置包围盒
    m_boundingBoxMin = QVector3D(std::numeric_limits<float>::max(), 
                                std::numeric_limits<float>::max(), 
                                std::numeric_limits<float>::max());
    m_boundingBoxMax = QVector3D(-std::numeric_limits<float>::max(), 
                                -std::numeric_limits<float>::max(), 
                                -std::numeric_limits<float>::max());
    m_boundingBoxCenter = QVector3D(0.0f, 0.0f, 0.0f);
    m_boundingBoxSize = 0.0f;
    
    // 重置视图
    resetView();
    
    // 更新显示
    update();
}

void PointCloudGLWidget::initAxes()
{
    const float axisLength = 0.15f;
    const float arrowSize = 0.015f;
    
    // 创建坐标轴顶点数据
    QVector<QVector3D> axesVertices = {
        // X轴 (红色)
        QVector3D(0, 0, 0), QVector3D(axisLength, 0, 0),
        // Y轴 (绿色)
        QVector3D(0, 0, 0), QVector3D(0, axisLength, 0),
        // Z轴 (蓝色)
        QVector3D(0, 0, 0), QVector3D(0, 0, axisLength),
        // X轴箭头
        QVector3D(axisLength, 0, 0), QVector3D(axisLength-arrowSize, arrowSize, arrowSize),
        QVector3D(axisLength, 0, 0), QVector3D(axisLength-arrowSize, -arrowSize, arrowSize),
        QVector3D(axisLength, 0, 0), QVector3D(axisLength-arrowSize, arrowSize, -arrowSize),
        QVector3D(axisLength, 0, 0), QVector3D(axisLength-arrowSize, -arrowSize, -arrowSize),
        // Y轴箭头
        QVector3D(0, axisLength, 0), QVector3D(arrowSize, axisLength-arrowSize, arrowSize),
        QVector3D(0, axisLength, 0), QVector3D(-arrowSize, axisLength-arrowSize, arrowSize),
        QVector3D(0, axisLength, 0), QVector3D(arrowSize, axisLength-arrowSize, -arrowSize),
        QVector3D(0, axisLength, 0), QVector3D(-arrowSize, axisLength-arrowSize, -arrowSize),
        // Z轴箭头
        QVector3D(0, 0, axisLength), QVector3D(arrowSize, arrowSize, axisLength-arrowSize),
        QVector3D(0, 0, axisLength), QVector3D(-arrowSize, arrowSize, axisLength-arrowSize),
        QVector3D(0, 0, axisLength), QVector3D(arrowSize, -arrowSize, axisLength-arrowSize),
        QVector3D(0, 0, axisLength), QVector3D(-arrowSize, -arrowSize, axisLength-arrowSize)
    };
    
    // 创建坐标轴颜色数据
    QVector<QVector3D> axesColors;
    
    // X轴 (红色)
    axesColors << QVector3D(1, 0, 0) << QVector3D(1, 0, 0);
    // Y轴 (绿色)
    axesColors << QVector3D(0, 1, 0) << QVector3D(0, 1, 0);
    // Z轴 (蓝色)
    axesColors << QVector3D(0, 0, 1) << QVector3D(0, 0, 1);
    // X轴箭头 (红色)
    for (int i = 0; i < 8; ++i) {
        axesColors << QVector3D(1, 0, 0);
    }
    // Y轴箭头 (绿色)
    for (int i = 0; i < 8; ++i) {
        axesColors << QVector3D(0, 1, 0);
    }
    // Z轴箭头 (蓝色)
    for (int i = 0; i < 8; ++i) {
        axesColors << QVector3D(0, 0, 1);
    }
    
    // 清理并重新创建着色器程序
    if (m_axesProgram.isLinked()) {
        m_axesProgram.removeAllShaders();
    }
    
    // 静态定义着色器源码
    static const char* axesVertexShaderSource = R"(
        #version 100
        attribute vec3 position;
        attribute vec3 color;
        varying vec3 fragColor;
        uniform mat4 mvpMatrix;
        void main() {
            gl_Position = mvpMatrix * vec4(position, 1.0);
            fragColor = color;
        }
    )";
    
    static const char* axesFragmentShaderSource = R"(
        #version 100
        precision mediump float;
        varying vec3 fragColor;
        void main() {
            gl_FragColor = vec4(fragColor, 1.0);
        }
    )";
    
    // 添加顶点着色器
    if (!m_axesProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, axesVertexShaderSource)) {
        LOG_ERROR("坐标轴顶点着色器加载失败");
        return;
    }
    
    // 添加片段着色器
    if (!m_axesProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, axesFragmentShaderSource)) {
        LOG_ERROR("坐标轴片段着色器加载失败");
        return;
    }
    
    // 链接着色器程序
    if (!m_axesProgram.link()) {
        LOG_ERROR("坐标轴着色器程序链接失败");
        return;
    }
    
    if (!m_axesProgram.bind()) {
        LOG_ERROR("坐标轴着色器程序绑定失败");
        return;
    }
    
    // 初始化坐标轴VAO
    if (!m_axesVAO.isCreated()) {
        m_axesVAO.create();
    }
    m_axesVAO.bind();
    
    // 初始化坐标轴顶点VBO
    if (!m_axesVBO.isCreated()) {
        m_axesVBO.create();
    }
    m_axesVBO.bind();
    m_axesVBO.allocate(axesVertices.constData(), axesVertices.size() * sizeof(QVector3D));
    
    // 设置顶点属性
    m_axesProgram.enableAttributeArray(0);  // position属性
    m_axesProgram.setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(QVector3D));
    
    // 初始化坐标轴颜色VBO
    if (!m_axesColorVBO.isCreated()) {
        m_axesColorVBO.create();
    }
    m_axesColorVBO.bind();
    m_axesColorVBO.allocate(axesColors.constData(), axesColors.size() * sizeof(QVector3D));
    
    // 设置颜色属性
    m_axesProgram.enableAttributeArray(1);  // color属性
    m_axesProgram.setAttributeBuffer(1, GL_FLOAT, 0, 3, sizeof(QVector3D));
    
    // 解绑
    m_axesProgram.release();
    m_axesColorVBO.release();
    m_axesVBO.release();
    m_axesVAO.release();
    
    LOG_INFO("坐标轴初始化完成");
}

void PointCloudGLWidget::drawAxes()
{
    if (!m_axesProgram.isLinked()) {
        LOG_ERROR("坐标轴着色器程序未链接，无法绘制坐标轴");
        return;
    }
    
    // 创建视口左下角的投影矩阵
    QMatrix4x4 axesProjection;
    axesProjection.ortho(-0.2f, 0.2f, -0.2f, 0.2f, -10.0f, 10.0f);
    
    // 创建视图矩阵
    QMatrix4x4 axesView;
    axesView.setToIdentity();
    axesView.translate(-0.15f, -0.15f, 0.0f);  // 移动到视口左下角
    
    // 创建模型矩阵，与主视图使用相同的旋转
    QMatrix4x4 axesModel;
    axesModel.setToIdentity();
    
    // 强制使用固定角度，确保坐标轴方向明确
    // 不直接采用点云的旋转角度，而是使用固定的角度使坐标轴一直保持标准方向
    axesModel.rotate(30.0f, 1.0f, 0.0f, 0.0f);  // X轴倾斜角度
    axesModel.rotate(45.0f, 0.0f, 1.0f, 0.0f);  // Y轴旋转角度
    
    // 设置视口 - 增大坐标轴区域，并将位置调整到左下角且离边缘有一定距离
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int axesSize = qMin(viewport[2], viewport[3]) / 2.5;  // 更大的坐标轴区域
    glViewport(30, 30, axesSize, axesSize);  // 偏移量增大
    
    // 绑定坐标轴VAO和着色器程序
    m_axesVAO.bind();
    m_axesProgram.bind();
    
    // 设置统一变量
    m_axesProgram.setUniformValue("mvpMatrix", axesProjection * axesView * axesModel);
    
    // 禁用深度测试，确保坐标轴始终可见
    glDisable(GL_DEPTH_TEST);
    
    // 绘制坐标轴（加粗线宽）
    glLineWidth(4.0f);  // 更加加粗线宽
    glDrawArrays(GL_LINES, 0, 38);  // 38 = 坐标轴顶点数量
    glLineWidth(1.0f);  // 恢复默认线宽
    
    // 重新启用深度测试
    glEnable(GL_DEPTH_TEST);
    
    // 解除绑定
    m_axesProgram.release();
    m_axesVAO.release();
    
    // 恢复原始视口
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    
    // 记录日志
    LOG_INFO("绘制坐标轴 - 尺寸:" + QString::number(axesSize));
}

void PointCloudGLWidget::set2DImageView()
{
    LOG_INFO("设置点云视图为2D图像视角（俯视）");

    // ---------> 设置俯视四元数 <---------
    // 俯视 XY 平面，不需要旋转
    m_rotationQuaternion = QQuaternion();
    // 如果需要特定角度，例如从X轴负方向看：
    // m_rotationQuaternion = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, -90.0f);
    // ---------> 设置结束 <---------

    // 根据点云大小计算适当的视距和缩放
    if (!m_points.empty() && m_boundingBoxSize > 0.0f) {
        // 使点云大致填满视图，但确保足够远离
        float distance = m_boundingBoxSize / (2.0f * tan(qDegreesToRadians(45.0f / 2.0f)));
        m_translateZ = distance * 1.5f; // +Z 为前方
        m_scale = 1.5f; // 保持缩放
    } else {
        // 默认值
        m_translateZ = 7.5f; // +Z 为前方
        m_scale = 1.5f;
    }

    m_translateX = 0.0f; // 水平居中
    m_translateY = 0.0f; // 垂直居中
    
    m_autoAdjustOnNextPaint = false; // 禁用自动调整
    
    update();
    
    LOG_INFO(QString("设置2D图像视角完成 - 缩放: %1, 视距: %2").arg(m_scale).arg(m_translateZ));
}

// 添加球体
void PointCloudGLWidget::addSphere(const QVector3D& position, float radius, const QColor& color)
{
    LOG_INFO(QString("添加球体到点云 - 位置: (%1, %2, %3), 半径: %4")
           .arg(position.x(), 0, 'f', 2)
           .arg(position.y(), 0, 'f', 2)
           .arg(position.z(), 0, 'f', 2)
           .arg(radius, 0, 'f', 2));
    
    PointCloudSphere sphere;
    
    // 应用与点云相同的坐标偏移，确保球体与点云正确对齐
    // 点云在updatePointCloud中被移动到原点，这里也需要同样处理
    sphere.position = position - m_boundingBoxCenter;
    sphere.radius = radius;
    sphere.color = color;
    
    LOG_INFO(QString("球体坐标应用偏移 - 原始: (%1, %2, %3), 偏移: (%4, %5, %6), 结果: (%7, %8, %9)")
           .arg(position.x(), 0, 'f', 4)
           .arg(position.y(), 0, 'f', 4)
           .arg(position.z(), 0, 'f', 4)
           .arg(m_boundingBoxCenter.x(), 0, 'f', 4)
           .arg(m_boundingBoxCenter.y(), 0, 'f', 4)
           .arg(m_boundingBoxCenter.z(), 0, 'f', 4)
           .arg(sphere.position.x(), 0, 'f', 4)
           .arg(sphere.position.y(), 0, 'f', 4)
           .arg(sphere.position.z(), 0, 'f', 4));
    
    m_spheres.push_back(sphere);
    
    // 更新显示
    update();
}

// 添加线段
void PointCloudGLWidget::addLine(const QVector3D& start, const QVector3D& end, const QColor& color)
{
    LOG_INFO(QString("添加线段到点云 - 起点: (%1, %2, %3), 终点: (%4, %5, %6)")
           .arg(start.x(), 0, 'f', 4)
           .arg(start.y(), 0, 'f', 4)
           .arg(start.z(), 0, 'f', 4)
           .arg(end.x(), 0, 'f', 4)
           .arg(end.y(), 0, 'f', 4)
           .arg(end.z(), 0, 'f', 4));
    
    PointCloudLine line;
    // 应用与点云相同的坐标偏移
    line.start = start - m_boundingBoxCenter;
    line.end = end - m_boundingBoxCenter;
    line.color = color;
    
    LOG_INFO(QString("线段坐标应用偏移 - 偏移量: (%1, %2, %3), 结果起点: (%4, %5, %6), 结果终点: (%7, %8, %9)")
           .arg(m_boundingBoxCenter.x(), 0, 'f', 4)
           .arg(m_boundingBoxCenter.y(), 0, 'f', 4)
           .arg(m_boundingBoxCenter.z(), 0, 'f', 4)
           .arg(line.start.x(), 0, 'f', 4)
           .arg(line.start.y(), 0, 'f', 4)
           .arg(line.start.z(), 0, 'f', 4)
           .arg(line.end.x(), 0, 'f', 4)
           .arg(line.end.y(), 0, 'f', 4)
           .arg(line.end.z(), 0, 'f', 4));
    
    m_lines.push_back(line);
    
    // 更新显示
    update();
}

// 添加文本标签
void PointCloudGLWidget::addText(const QVector3D& position, const QString& text, const QColor& color)
{
    LOG_INFO(QString("添加文本到点云 - 位置: (%1, %2, %3), 文本: %4")
           .arg(position.x(), 0, 'f', 2)
           .arg(position.y(), 0, 'f', 2)
           .arg(position.z(), 0, 'f', 2)
           .arg(text));
    
    PointCloudText textObj;
    // 应用与点云相同的坐标偏移
    textObj.position = position - m_boundingBoxCenter;
    textObj.text = text;
    textObj.color = color;
    
    LOG_INFO(QString("文本坐标应用偏移 - 偏移量: (%1, %2, %3)")
           .arg(m_boundingBoxCenter.x(), 0, 'f', 4)
           .arg(m_boundingBoxCenter.y(), 0, 'f', 4)
           .arg(m_boundingBoxCenter.z(), 0, 'f', 4));
    
    m_texts.push_back(textObj);
    
    // 更新显示
    update();
}

// 清除所有几何对象
void PointCloudGLWidget::clearGeometryObjects()
{
    LOG_INFO("清除点云中的所有几何对象");
    
    m_spheres.clear();
    m_lines.clear();
    m_texts.clear();
    
    // 更新显示
    update();
}

// 世界坐标转换为屏幕坐标
QVector3D PointCloudGLWidget::worldToScreen(const QVector3D& worldPos)
{
    // 应用模型-视图-投影变换
    QVector4D clipPos = m_projection * m_view * m_model * QVector4D(worldPos, 1.0f);
    
    // 执行透视除法
    QVector3D ndcPos;
    if (qAbs(clipPos.w()) > 0.0001f) {
        ndcPos = QVector3D(clipPos.x() / clipPos.w(), clipPos.y() / clipPos.w(), clipPos.z() / clipPos.w());
    } else {
        ndcPos = QVector3D(clipPos.x(), clipPos.y(), clipPos.z());
    }
    
    // 变换到窗口坐标
    float screenX = (ndcPos.x() + 1.0f) * 0.5f * width();
    float screenY = (1.0f - ndcPos.y()) * 0.5f * height();  // 翻转Y轴
    
    // 判断点是否在视锥体内
    bool inFrustum = ndcPos.x() >= -1.0f && ndcPos.x() <= 1.0f &&
                    ndcPos.y() >= -1.0f && ndcPos.y() <= 1.0f &&
                    ndcPos.z() >= -1.0f && ndcPos.z() <= 1.0f;
    
    // 返回结果（x和y是屏幕坐标，z用于深度测试 - 正值表示在视口内）
    // 我们将z设置为1.0表示在视口内，-1.0表示在视口外
    float depthValue = inFrustum ? 1.0f : -1.0f;
    
    LOG_INFO(QString("世界坐标转屏幕: 世界(%1, %2, %3) -> NDC(%4, %5, %6) -> 屏幕(%7, %8, %9), 视口内: %10")
           .arg(worldPos.x(), 0, 'f', 2).arg(worldPos.y(), 0, 'f', 2).arg(worldPos.z(), 0, 'f', 2)
           .arg(ndcPos.x(), 0, 'f', 2).arg(ndcPos.y(), 0, 'f', 2).arg(ndcPos.z(), 0, 'f', 2)
           .arg(screenX, 0, 'f', 0).arg(screenY, 0, 'f', 0).arg(depthValue)
           .arg(inFrustum ? "是" : "否"));
    
    return QVector3D(screenX, screenY, depthValue);
}

QVector3D PointCloudGLWidget::screenToWorld(const QPoint& screenPos)
{
    // 获取OpenGL视口
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    // 从屏幕坐标转换为归一化设备坐标(NDC)
    float x = 2.0f * screenPos.x() / width() - 1.0f;
    float y = 1.0f - 2.0f * screenPos.y() / height(); // 翻转Y轴
    
    // 读取当前像素的深度值 - 使用OpenGL的深度缓冲区
    float depth = 0.5f; // 默认中间深度值
    // 注意：在实际应用中，我们应该读取深度缓冲区来获取准确的深度值
    // 但这里使用一个近似值，稍后我们会进行射线与点云的相交测试
    
    // 计算有效投影深度
    float z = 2.0f * depth - 1.0f;
    
    // 创建NDC坐标
    QVector4D ndcPos(x, y, z, 1.0f);
    
    // 获取各个变换矩阵的逆矩阵
    QMatrix4x4 invProjection = m_projection.inverted();
    QMatrix4x4 invView = m_view.inverted();
    QMatrix4x4 invModel = m_model.inverted();
    
    // 反向应用投影变换
    QVector4D clipPos = invProjection * ndcPos;
    clipPos = QVector4D(clipPos.x(), clipPos.y(), -1.0f, 0.0f); // 让z指向相机前方
    
    // 反向应用视图变换
    QVector4D viewPos = invView * clipPos;
    
    // 反向应用模型变换
    QVector4D worldPos = invModel * viewPos;
    
    // 计算相机位置
    QVector3D cameraPos = invView.map(QVector3D(0, 0, 0));
    
    // 计算从相机指向点击位置的射线方向
    QVector3D rayDirection = QVector3D(worldPos.x(), worldPos.y(), worldPos.z()).normalized();
    
    // 尝试在点云中找到最近的点
    QVector3D closestPoint;
    float minDistance = std::numeric_limits<float>::max();
    bool foundPoint = false;
    
    // 只有在有点云数据时才进行
    if (!m_points.empty()) {
        // 屏幕选择容差（像素）
        const float screenTolerance = 20.0f;
        
        // 遍历所有点云点，找到屏幕上最接近点击位置的点
        for (const QVector3D& point : m_points) {
            // 将点投影到屏幕上
            QVector3D screenPoint = worldToScreen(point);
            
            // 只考虑视口内的点
            if (screenPoint.z() > 0) {
                // 计算屏幕距离
                float dx = screenPoint.x() - screenPos.x();
                float dy = screenPoint.y() - screenPos.y();
                float screenDistance = std::sqrt(dx*dx + dy*dy);
                
                // 如果在容差范围内且比之前找到的点更近
                if (screenDistance < screenTolerance && screenDistance < minDistance) {
                    minDistance = screenDistance;
                    closestPoint = point;
                    foundPoint = true;
                }
            }
        }
        
        LOG_INFO(QString("在点云中搜索最近点 - 找到点: %1, 屏幕距离: %2 像素")
               .arg(foundPoint ? "是" : "否")
               .arg(minDistance, 0, 'f', 2));
    }
    
    // 如果找到了点云中的最近点，则返回它
    if (foundPoint) {
        LOG_INFO(QString("返回点云中的最近点 - 世界坐标: (%1, %2, %3)")
               .arg(closestPoint.x(), 0, 'f', 4)
               .arg(closestPoint.y(), 0, 'f', 4)
               .arg(closestPoint.z(), 0, 'f', 4));
        return closestPoint;
    }
    
    // 否则，进行射线与平面的相交计算
    // 找到点云的平均z值或使用预定义的z平面值
    float targetZ = 0.0f;
    if (!m_points.empty()) {
        // 使用包围盒中心的z值
        targetZ = m_boundingBoxCenter.z();
    }
    
    // 计算射线与z=targetZ平面的交点
    // 相机位置 + t * 射线方向 = 交点
    // 假设我们找到一个t，使得交点的z坐标等于targetZ
    float t = 0.0f;
    if (qAbs(rayDirection.z()) > 0.0001f) {
        t = (targetZ - cameraPos.z()) / rayDirection.z();
    }
    
    // 计算交点
    QVector3D intersectionPoint = cameraPos + t * rayDirection;
    
    LOG_INFO(QString("屏幕坐标转世界: 屏幕(%1, %2) -> 世界(%3, %4, %5)")
           .arg(screenPos.x()).arg(screenPos.y())
           .arg(intersectionPoint.x(), 0, 'f', 4)
           .arg(intersectionPoint.y(), 0, 'f', 4)
           .arg(intersectionPoint.z(), 0, 'f', 4));
    
    return intersectionPoint;
}

// 键盘按键事件
void PointCloudGLWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_R:  // 按R键重置视图
            LOG_INFO("用户按下R键，重置点云视图");
            resetView();
            break;
        
        case Qt::Key_A:  // 按A键自动调整视图
            LOG_INFO("用户按下A键，自动调整点云视图");
            autoAdjustView();
            break;
        
        case Qt::Key_Plus:  // 按+键增大点大小
        case Qt::Key_Equal:  // 或按=键
            m_pointSize += 0.5f;
            m_pointSize = qMin(m_pointSize, 10.0f);  // 限制最大值
            LOG_INFO(QString("增大点大小: %1").arg(m_pointSize));
            update();
            break;
        
        case Qt::Key_Minus:  // 按-键减小点大小
            m_pointSize -= 0.5f;
            m_pointSize = qMax(m_pointSize, 1.0f);  // 限制最小值
            LOG_INFO(QString("减小点大小: %1").arg(m_pointSize));
            update();
            break;
        
        default:
            QOpenGLWidget::keyPressEvent(event);
            break;
    }
} 