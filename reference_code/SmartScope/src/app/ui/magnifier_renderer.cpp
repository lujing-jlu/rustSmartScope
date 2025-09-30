#include "app/ui/magnifier_internal.h"

MagnifierRenderer::MagnifierRenderer()
{
}

MagnifierRenderer::~MagnifierRenderer()
{
}

void MagnifierRenderer::updateContent(QLabel* leftImageLabel, QLabel* magnifierLabel, 
                                     QWidget* magnifierContainer, double zoom,
                                     QSize magnifierSize)
{
    // 确保组件已创建
    if (!magnifierContainer || !magnifierLabel || !leftImageLabel) {
        return;
    }
    
    // 获取当前左图像
    QPixmap leftPixmap = leftImageLabel->pixmap(Qt::ReturnByValue);
    if (leftPixmap.isNull()) {
        qDebug() << "左图像为空，放大镜不显示内容";
        return;
    }
    
    // 获取鼠标在屏幕上的位置
    QPoint globalMousePos = QCursor::pos();
    
    // 将全局鼠标位置转换为左图像标签的局部坐标
    QPoint localMousePos = leftImageLabel->mapFromGlobal(globalMousePos);
    
    // 计算图像在标签中的位置和大小
    QSize labelSize = leftImageLabel->size();
    QSize pixmapSize = leftPixmap.size();
    QSize scaledSize = pixmapSize.scaled(labelSize, Qt::KeepAspectRatio);
    
    // 图像在标签中的偏移（居中显示）
    int offsetX = (labelSize.width() - scaledSize.width()) / 2;
    int offsetY = (labelSize.height() - scaledSize.height()) / 2;
    
    // 图像区域
    QRect imgRect(offsetX, offsetY, scaledSize.width(), scaledSize.height());
    
    // 计算放大区域尺寸
    int magnifierSizeVal = magnifierSize.width();
    int sourceSize = qRound(magnifierSizeVal / zoom);
    
    // 鼠标相对于图像左上角的坐标，即使鼠标在图像区域外
    int imageRelativeX = localMousePos.x() - offsetX;
    int imageRelativeY = localMousePos.y() - offsetY;
    
    // 将坐标限制在图像区域附近，允许一定的超出
    int boundaryExtension = sourceSize / 2; // 允许超出的范围
    imageRelativeX = qBound(-boundaryExtension, imageRelativeX, scaledSize.width() + boundaryExtension - 1);
    imageRelativeY = qBound(-boundaryExtension, imageRelativeY, scaledSize.height() + boundaryExtension - 1);
    
    // 计算源区域左上角坐标
    int halfSourceSize = sourceSize / 2;
    int sourceX = imageRelativeX - halfSourceSize;
    int sourceY = imageRelativeY - halfSourceSize;
    
    // 获取缩放后的图像
    QPixmap scaledImage = leftPixmap.scaled(
        scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation
    );
    
    // 创建一个透明的背景图像，大小为源区域大小，稍后会将实际图像内容复制到这个背景上
    QPixmap paddedSourcePixmap(sourceSize, sourceSize);
    paddedSourcePixmap.fill(Qt::transparent); // 填充透明背景
    
    // 创建画家来绘制到这个背景上
    QPainter sourcePainter(&paddedSourcePixmap);
    
    // 计算裁剪区域相对于图像的坐标
    QRect sourceRect = QRect(sourceX, sourceY, sourceSize, sourceSize);
    
    // 计算实际有效的源区域（与图像的交集）
    QRect validSourceRect = sourceRect.intersected(QRect(0, 0, scaledSize.width(), scaledSize.height()));
    
    // 只有在有效区域非空时才进行绘制
    if (!validSourceRect.isEmpty()) {
        // 计算有效区域在paddedSourcePixmap中的位置偏移
        int offsetInPaddedX = validSourceRect.left() - sourceRect.left();
        int offsetInPaddedY = validSourceRect.top() - sourceRect.top();
        
        // 从scaledImage中提取有效区域并绘制到paddedSourcePixmap中
        sourcePainter.drawPixmap(
            offsetInPaddedX, offsetInPaddedY,
            scaledImage.copy(validSourceRect)
        );
    }
    
    // 处理超出边界的区域
    handleOutOfBoundaries(sourcePainter, sourceX, sourceY, sourceSize, scaledSize);
    
    // 完成绘制
    sourcePainter.end();
    
    // 创建放大后的图像，保持纵横比
    QPixmap magnifiedPixmap = paddedSourcePixmap.scaled(
        magnifierSizeVal, magnifierSizeVal, 
        Qt::KeepAspectRatio, Qt::SmoothTransformation
    );
    
    // 创建带有透明蒙版的图像
    QPixmap finalPixmap(magnifierSizeVal, magnifierSizeVal);
    finalPixmap.fill(Qt::transparent);
    
    // 使用QPainter绘制圆形放大镜
    QPainter painter(&finalPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 创建圆形区域
    QPainterPath path;
    path.addEllipse(0, 0, magnifierSizeVal, magnifierSizeVal);
    
    // 填充圆形区域
    painter.setClipPath(path);
    
    // 居中绘制放大后的图像
    int drawX = (magnifierSizeVal - magnifiedPixmap.width()) / 2;
    int drawY = (magnifierSizeVal - magnifiedPixmap.height()) / 2;
    painter.drawPixmap(drawX, drawY, magnifiedPixmap);
    
    // 绘制边框和十字线
    drawBorderAndCrosshair(painter, magnifierSizeVal);
    
    // 结束绘制
    painter.end();
    
    // 设置放大镜标签的图像
    magnifierLabel->setPixmap(finalPixmap);
    
    // 刷新窗口掩码，确保边缘圆滑
    QBitmap mask = finalPixmap.mask();
    if (mask.isNull()) {
        // 如果没有内置掩码，从透明色创建掩码
        mask = finalPixmap.createMaskFromColor(Qt::transparent, Qt::MaskOutColor);
    }
    
    // 只有在掩码有效时才应用
    if (!mask.isNull()) {
        magnifierContainer->setMask(mask);
    }
}

void MagnifierRenderer::handleOutOfBoundaries(QPainter& painter, int sourceX, int sourceY, 
                                              int sourceSize, const QSize& scaledSize)
{
    // 添加黑色背景填充超出图像的区域
    QColor backgroundColor(30, 30, 30, 200); // 半透明暗灰色背景
    
    // 定义超出图像的区域
    if (sourceX < 0) {
        // 左边超出
        QRect leftRect(0, 0, -sourceX, sourceSize);
        painter.fillRect(leftRect, backgroundColor);
    }
    if (sourceY < 0) {
        // 上边超出
        QRect topRect(qMax(0, -sourceX), 0, sourceSize - qMax(0, -sourceX), -sourceY);
        painter.fillRect(topRect, backgroundColor);
    }
    if (sourceX + sourceSize > scaledSize.width()) {
        // 右边超出
        int rightOverflow = sourceX + sourceSize - scaledSize.width();
        QRect rightRect(sourceSize - rightOverflow, 0, rightOverflow, sourceSize);
        painter.fillRect(rightRect, backgroundColor);
    }
    if (sourceY + sourceSize > scaledSize.height()) {
        // 下边超出
        int bottomOverflow = sourceY + sourceSize - scaledSize.height();
        QRect bottomRect(0, sourceSize - bottomOverflow, sourceSize, bottomOverflow);
        painter.fillRect(bottomRect, backgroundColor);
    }
}

void MagnifierRenderer::drawBorderAndCrosshair(QPainter& painter, int magnifierSize)
{
    // 绘制边框（没有裁剪）
    painter.setClipping(false);
    QLinearGradient gradient(0, 0, magnifierSize, magnifierSize);
    gradient.setColorAt(0, QColor(255, 255, 255, 220));
    gradient.setColorAt(1, QColor(200, 200, 200, 220));
    
    QPen pen(QBrush(gradient), 4);  // 增大边框宽度以便更明显
    painter.setPen(pen);
    painter.drawEllipse(2, 2, magnifierSize-4, magnifierSize-4);
    
    // 绘制十字线
    painter.setPen(QPen(QColor(255, 0, 0, 200), 3));  // 加粗十字线
    int crossSize = 20;
    painter.drawLine(magnifierSize/2 - crossSize, magnifierSize/2, 
                     magnifierSize/2 + crossSize, magnifierSize/2);
    painter.drawLine(magnifierSize/2, magnifierSize/2 - crossSize, 
                     magnifierSize/2, magnifierSize/2 + crossSize);
} 