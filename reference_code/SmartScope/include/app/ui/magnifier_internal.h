#ifndef MAGNIFIER_INTERNAL_H
#define MAGNIFIER_INTERNAL_H

#include <QWidget>
#include <QLabel>
#include <QPoint>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QBitmap>
#include <QLinearGradient>
#include <QDebug>

/**
 * @brief 放大镜创建器类
 * 
 * 负责创建放大镜UI组件
 */
class MagnifierCreator
{
public:
    explicit MagnifierCreator();
    ~MagnifierCreator();

    /**
     * @brief 创建放大镜组件
     */
    bool create(QWidget* contentWidget, QLabel* leftImageLabel, 
                float leftAreaRatio, QWidget** outContainer, 
                QLabel** outLabel, QSize magnifierSize);

    /**
     * @brief 销毁放大镜组件
     */
    void destroy(QWidget* magnifierContainer);
};

/**
 * @brief 放大镜渲染器类
 * 
 * 负责渲染放大镜内容
 */
class MagnifierRenderer
{
public:
    explicit MagnifierRenderer();
    ~MagnifierRenderer();

    /**
     * @brief 更新放大镜内容
     */
    void updateContent(QLabel* leftImageLabel, QLabel* magnifierLabel, 
                      QWidget* magnifierContainer, double zoom,
                      QSize magnifierSize);

private:
    /**
     * @brief 处理超出边界的区域
     */
    void handleOutOfBoundaries(QPainter& painter, int sourceX, int sourceY, 
                               int sourceSize, const QSize& scaledSize);

    /**
     * @brief 绘制放大镜边框和十字线
     */
    void drawBorderAndCrosshair(QPainter& painter, int magnifierSize);
};

/**
 * @brief 放大镜控制器类
 * 
 * 负责控制放大镜的显示和隐藏
 */
class MagnifierController
{
public:
    explicit MagnifierController();
    ~MagnifierController();

    /**
     * @brief 显示放大镜
     */
    void show(QWidget* magnifierContainer);

    /**
     * @brief 隐藏放大镜
     */
    void hide(QWidget* magnifierContainer);
};

#endif // MAGNIFIER_INTERNAL_H 