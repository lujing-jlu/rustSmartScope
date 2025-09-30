#ifndef MODERN_ICONS_H
#define MODERN_ICONS_H

#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QColor>

namespace SmartScope {

/**
 * @brief 现代化图标生成器
 * 
 * 提供各种现代化风格的图标绘制功能
 */
class ModernIcons
{
public:
    /**
     * @brief 创建截屏图标
     * @param size 图标大小
     * @param color 图标颜色
     * @return 截屏图标
     */
    static QIcon createScreenshotIcon(int size = 64, const QColor& color = Qt::white);
    
    /**
     * @brief 创建LED亮度调整图标
     * @param size 图标大小
     * @param color 图标颜色
     * @return LED亮度调整图标
     */
    static QIcon createLedBrightnessIcon(int size = 64, const QColor& color = Qt::white);
    
    /**
     * @brief 创建AI检测图标
     * @param size 图标大小
     * @param color 图标颜色
     * @return AI检测图标
     */
    static QIcon createAiDetectionIcon(int size = 64, const QColor& color = Qt::white);
    
    /**
     * @brief 创建相机调节图标
     * @param size 图标大小
     * @param color 图标颜色
     * @return 相机调节图标
     */
    static QIcon createCameraAdjustIcon(int size = 64, const QColor& color = Qt::white);

private:
    /**
     * @brief 绘制截屏图标
     * @param painter 绘制器
     * @param rect 绘制区域
     * @param color 颜色
     */
    static void drawScreenshotIcon(QPainter& painter, const QRect& rect, const QColor& color);
    
    /**
     * @brief 绘制LED亮度图标
     * @param painter 绘制器
     * @param rect 绘制区域
     * @param color 颜色
     */
    static void drawLedBrightnessIcon(QPainter& painter, const QRect& rect, const QColor& color);
    
    /**
     * @brief 绘制AI检测图标
     * @param painter 绘制器
     * @param rect 绘制区域
     * @param color 颜色
     */
    static void drawAiDetectionIcon(QPainter& painter, const QRect& rect, const QColor& color);
    
    /**
     * @brief 绘制相机调节图标
     * @param painter 绘制器
     * @param rect 绘制区域
     * @param color 颜色
     */
    static void drawCameraAdjustIcon(QPainter& painter, const QRect& rect, const QColor& color);
    
    /**
     * @brief 创建基础图标
     * @param size 图标大小
     * @param color 图标颜色
     * @param drawFunc 绘制函数
     * @return 图标
     */
    static QIcon createIcon(int size, const QColor& color, 
                           std::function<void(QPainter&, const QRect&, const QColor&)> drawFunc);
};

} // namespace SmartScope

#endif // MODERN_ICONS_H
