#include "app/ui/clickable_image_label.h"
#include "infrastructure/logging/logger.h"
#include <QResizeEvent>

ClickableImageLabel::ClickableImageLabel(QWidget* parent, qreal ratio) 
    : QLabel(parent), 
      m_aspectRatio(ratio), 
      m_clickEnabled(false),
      m_isPressing(false) 
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAlignment(Qt::AlignCenter);
    setMouseTracking(true);
}

void ClickableImageLabel::setAspectRatio(qreal ratio) 
{
    m_aspectRatio = ratio;
    updateGeometry();
}

qreal ClickableImageLabel::aspectRatio() const 
{
    return m_aspectRatio;
}

int ClickableImageLabel::heightForWidth(int width) const 
{
    return static_cast<int>(width * m_aspectRatio);
}

QSize ClickableImageLabel::sizeHint() const 
{
    int w = width();
    return QSize(w, heightForWidth(w));
}

void ClickableImageLabel::setClickEnabled(bool enabled) 
{
    m_clickEnabled = enabled;
    if (m_clickEnabled) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

bool ClickableImageLabel::isClickEnabled() const 
{
    return m_clickEnabled;
}

void ClickableImageLabel::setOriginalImageSize(const QSize& size) 
{
    m_originalImageSize = size;
}

QPoint ClickableImageLabel::mapToImageCoords(const QPoint& labelPoint) const 
{
    // 如果没有原始图像尺寸，返回标签中的坐标
    if (m_originalImageSize.isEmpty()) {
        return labelPoint;
    }
    
    // 获取当前显示的图像大小
    QPixmap currentPixmap = pixmap(Qt::ReturnByValue);
    if (currentPixmap.isNull()) {
        return labelPoint;
    }
    
    // 计算图像在标签中的位置和大小
    QSize pixmapSize = currentPixmap.size();
    QSize labelSize = size();
    
    // 确定图像缩放后的尺寸，保持宽高比
    QSize scaledSize = pixmapSize.scaled(labelSize, Qt::KeepAspectRatio);
    
    // 计算图像在标签中的偏移（居中显示）
    int offsetX = (labelSize.width() - scaledSize.width()) / 2;
    int offsetY = (labelSize.height() - scaledSize.height()) / 2;
    
    // 计算点击点相对于图像左上角的位置
    int imageRelativeX = labelPoint.x() - offsetX;
    int imageRelativeY = labelPoint.y() - offsetY;
    
    // 计算在原始图像中的坐标（比例换算）
    double ratioX = static_cast<double>(m_originalImageSize.width()) / scaledSize.width();
    double ratioY = static_cast<double>(m_originalImageSize.height()) / scaledSize.height();
    
    int originalX = static_cast<int>(imageRelativeX * ratioX);
    int originalY = static_cast<int>(imageRelativeY * ratioY);
    
    // 确保坐标在有效范围内
    originalX = qBound(0, originalX, m_originalImageSize.width() - 1);
    originalY = qBound(0, originalY, m_originalImageSize.height() - 1);
    
    return QPoint(originalX, originalY);
}

void ClickableImageLabel::mousePressEvent(QMouseEvent* event) 
{
    if (event->button() == Qt::LeftButton && m_clickEnabled) {
        m_isPressing = true;
        m_currentPos = event->pos();
    }
    QLabel::mousePressEvent(event);
}

void ClickableImageLabel::mouseMoveEvent(QMouseEvent* event) 
{
    if (m_isPressing && m_clickEnabled) {
        m_currentPos = event->pos();
    }
    QLabel::mouseMoveEvent(event);
}

void ClickableImageLabel::mouseReleaseEvent(QMouseEvent* event) 
{
    if (event->button() == Qt::LeftButton && m_clickEnabled && m_isPressing) {
        QPoint labelPoint = event->pos();
        
        // 检查点击是否在实际图像区域内
        QPixmap currentPixmap = pixmap(Qt::ReturnByValue);
        if (!currentPixmap.isNull()) {
            QSize pixmapSize = currentPixmap.size();
            QSize labelSize = size();
            QSize scaledSize = pixmapSize.scaled(labelSize, Qt::KeepAspectRatio);
            
            int offsetX = 0;
            int offsetY = 0;
            
            // 根据对齐方式计算偏移量
            if (alignment() & Qt::AlignLeft) {
                offsetX = 0;
            } else if (alignment() & Qt::AlignRight) {
                offsetX = labelSize.width() - scaledSize.width();
            } else { // 默认居中
                offsetX = (labelSize.width() - scaledSize.width()) / 2;
            }
            
            if (alignment() & Qt::AlignTop) {
                offsetY = 0;
            } else if (alignment() & Qt::AlignBottom) {
                offsetY = labelSize.height() - scaledSize.height();
            } else { // 默认居中
                offsetY = (labelSize.height() - scaledSize.height()) / 2;
            }
            
            QRect imageRect(offsetX, offsetY, scaledSize.width(), scaledSize.height());
            
            // 只有当点击在图像区域内时才处理
            if (imageRect.contains(labelPoint)) {
                QPoint imagePoint = mapToImageCoords(labelPoint);
                // 再次检查映射后的坐标是否有效（mapToImageCoords内部已有检查，但双重检查更安全）
                if (imagePoint.x() >= 0 && imagePoint.y() >= 0) {
                    emit clicked(imagePoint.x(), imagePoint.y(), labelPoint);
                    LOG_DEBUG(QString("Image Clicked inside rect: Label(%1,%2) -> Image(%3,%4)")
                              .arg(labelPoint.x()).arg(labelPoint.y())
                              .arg(imagePoint.x()).arg(imagePoint.y()));
                } else {
                     LOG_DEBUG(QString("Image Clicked inside rect but mapped coords invalid: Label(%1,%2) -> Image(%3,%4)")
                              .arg(labelPoint.x()).arg(labelPoint.y())
                              .arg(imagePoint.x()).arg(imagePoint.y()));
                }
            } else {
                 LOG_DEBUG(QString("Image Clicked outside image rect: Label(%1,%2)").arg(labelPoint.x()).arg(labelPoint.y()));
            }
        } else {
            LOG_WARNING("ClickableImageLabel: No pixmap set, cannot process click.");
        }
        
        m_isPressing = false;
    }
    QLabel::mouseReleaseEvent(event);
}

void ClickableImageLabel::resizeEvent(QResizeEvent *event) 
{
    QLabel::resizeEvent(event);
    QPixmap currentPixmap = pixmap(Qt::ReturnByValue);
    if (currentPixmap.isNull())
        return;
        
    setPixmap(currentPixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
} 