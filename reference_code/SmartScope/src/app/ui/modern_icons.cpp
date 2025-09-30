#include "app/ui/modern_icons.h"
#include <functional>
#include <QApplication>
#include <QStyle>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SmartScope {

QIcon ModernIcons::createScreenshotIcon(int size, const QColor& color)
{
    return createIcon(size, color, drawScreenshotIcon);
}

QIcon ModernIcons::createLedBrightnessIcon(int size, const QColor& color)
{
    return createIcon(size, color, drawLedBrightnessIcon);
}

QIcon ModernIcons::createAiDetectionIcon(int size, const QColor& color)
{
    return createIcon(size, color, drawAiDetectionIcon);
}

QIcon ModernIcons::createCameraAdjustIcon(int size, const QColor& color)
{
    return createIcon(size, color, drawCameraAdjustIcon);
}

void ModernIcons::drawScreenshotIcon(QPainter& painter, const QRect& rect, const QColor& color)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    
    int margin = rect.width() * 0.15;
    QRect screenRect = rect.adjusted(margin, margin, -margin, -margin);
    
    // 绘制屏幕外框（圆角矩形）
    painter.drawRoundedRect(screenRect, 4, 4);
    
    // 绘制闪光效果（对角线）
    int flashSize = rect.width() * 0.2;
    QPoint center = rect.center();
    
    // 绘制闪光线条
    painter.setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center.x() - flashSize, center.y() - flashSize, 
                     center.x() + flashSize, center.y() + flashSize);
    painter.drawLine(center.x() - flashSize, center.y() + flashSize, 
                     center.x() + flashSize, center.y() - flashSize);
    
    // 绘制小圆点表示快门
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    int dotSize = rect.width() * 0.08;
    painter.drawEllipse(center.x() - dotSize/2, center.y() - dotSize/2, dotSize, dotSize);
}

void ModernIcons::drawLedBrightnessIcon(QPainter& painter, const QRect& rect, const QColor& color)
{
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPoint center = rect.center();
    int radius = rect.width() * 0.15;
    
    // 绘制中心太阳
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(center.x() - radius, center.y() - radius, radius * 2, radius * 2);
    
    // 绘制光线（8条）
    painter.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap));
    int rayLength = rect.width() * 0.25;
    int rayStart = rect.width() * 0.2;
    
    for (int i = 0; i < 8; i++) {
        double angle = i * M_PI / 4.0;
        int x1 = center.x() + rayStart * cos(angle);
        int y1 = center.y() + rayStart * sin(angle);
        int x2 = center.x() + (rayStart + rayLength) * cos(angle);
        int y2 = center.y() + (rayStart + rayLength) * sin(angle);
        
        painter.drawLine(x1, y1, x2, y2);
    }
    
    // 绘制亮度调节弧线
    painter.setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    int arcRadius = rect.width() * 0.35;
    QRect arcRect(center.x() - arcRadius, center.y() - arcRadius, 
                  arcRadius * 2, arcRadius * 2);
    painter.drawArc(arcRect, 45 * 16, 90 * 16); // 从45度到135度的弧
}

void ModernIcons::drawAiDetectionIcon(QPainter& painter, const QRect& rect, const QColor& color)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    
    QPoint center = rect.center();
    int size = rect.width() * 0.6;
    
    // 绘制AI大脑轮廓
    QRect brainRect(center.x() - size/2, center.y() - size/2, size, size);
    painter.drawEllipse(brainRect);
    
    // 绘制神经网络连接线
    painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap));
    
    // 绘制内部网络节点
    int nodeSize = rect.width() * 0.06;
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    
    // 中心节点
    painter.drawEllipse(center.x() - nodeSize/2, center.y() - nodeSize/2, nodeSize, nodeSize);
    
    // 周围节点
    for (int i = 0; i < 6; i++) {
        double angle = i * M_PI / 3.0;
        int nodeRadius = size * 0.25;
        int x = center.x() + nodeRadius * cos(angle);
        int y = center.y() + nodeRadius * sin(angle);
        painter.drawEllipse(x - nodeSize/2, y - nodeSize/2, nodeSize, nodeSize);
        
        // 连接线
        painter.setPen(QPen(color, 1, Qt::SolidLine));
        painter.drawLine(center, QPoint(x, y));
        painter.setPen(Qt::NoPen);
    }
    
    // 绘制扫描线效果
    painter.setPen(QPen(color, 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    int scanRadius = size * 0.4;
    QRect scanRect(center.x() - scanRadius, center.y() - scanRadius, 
                   scanRadius * 2, scanRadius * 2);
    painter.drawEllipse(scanRect);
}

void ModernIcons::drawCameraAdjustIcon(QPainter& painter, const QRect& rect, const QColor& color)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    
    int margin = rect.width() * 0.15;
    QRect cameraRect = rect.adjusted(margin, margin * 1.5, -margin, -margin);
    
    // 绘制相机主体
    painter.drawRoundedRect(cameraRect, 4, 4);
    
    // 绘制镜头
    QPoint center = cameraRect.center();
    int lensRadius = cameraRect.width() * 0.25;
    painter.drawEllipse(center.x() - lensRadius, center.y() - lensRadius, 
                        lensRadius * 2, lensRadius * 2);
    
    // 绘制内圈镜头
    int innerRadius = lensRadius * 0.6;
    painter.drawEllipse(center.x() - innerRadius, center.y() - innerRadius, 
                        innerRadius * 2, innerRadius * 2);
    
    // 绘制调节旋钮
    painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap));
    for (int i = 0; i < 8; i++) {
        double angle = i * M_PI / 4.0;
        int x1 = center.x() + (lensRadius + 3) * cos(angle);
        int y1 = center.y() + (lensRadius + 3) * sin(angle);
        int x2 = center.x() + (lensRadius + 8) * cos(angle);
        int y2 = center.y() + (lensRadius + 8) * sin(angle);
        painter.drawLine(x1, y1, x2, y2);
    }
    
    // 绘制顶部闪光灯
    painter.setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(color);
    int flashSize = cameraRect.width() * 0.08;
    QRect flashRect(cameraRect.right() - flashSize - 5, cameraRect.top() - flashSize/2, 
                    flashSize, flashSize);
    painter.drawRoundedRect(flashRect, 2, 2);
}

QIcon ModernIcons::createIcon(int size, const QColor& color, 
                             std::function<void(QPainter&, const QRect&, const QColor&)> drawFunc)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    QRect rect(0, 0, size, size);
    drawFunc(painter, rect, color);
    
    return QIcon(pixmap);
}

} // namespace SmartScope
