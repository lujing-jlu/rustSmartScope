#include "qml_video_item.h"
#include <QMutexLocker>
#include <QDebug>

QmlVideoItem::QmlVideoItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    // 启用抗锯齿
    setAntialiasing(true);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

QmlVideoItem::~QmlVideoItem()
{
}

void QmlVideoItem::paint(QPainter *painter)
{
    QMutexLocker locker(&m_mutex);

    if (m_currentFrame.isNull()) {
        // 绘制占位符 - 纯黑色背景
        painter->fillRect(boundingRect(), QColor("#000000"));

        painter->setPen(QColor("#88C0D0"));
        QFont font = painter->font();
        font.setPixelSize(32);
        painter->setFont(font);
        painter->drawText(boundingRect(), Qt::AlignCenter, "相机未连接");
        return;
    }

    // 计算缩放后的矩形，保持宽高比
    QRectF targetRect = boundingRect();
    QSizeF pixmapSize = m_currentFrame.size();
    QSizeF scaledSize = pixmapSize.scaled(targetRect.size(), Qt::KeepAspectRatio);

    QRectF destRect;
    destRect.setSize(scaledSize);
    destRect.moveCenter(targetRect.center());

    // 绘制QPixmap
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawPixmap(destRect, m_currentFrame, m_currentFrame.rect());

    // 绘制AI检测框
    if (!m_detections.isEmpty()) {
        // 根据缩放比例变换检测框坐标
        const qreal scaleX = destRect.width() / m_currentFrame.width();
        const qreal scaleY = destRect.height() / m_currentFrame.height();

        QPen pen(QColor(14, 165, 233), 2);
        painter->setPen(pen);
        for (const QVariant& v : m_detections) {
            const QVariantMap m = v.toMap();
            int left = m.value("left").toInt();
            int top = m.value("top").toInt();
            int right = m.value("right").toInt();
            int bottom = m.value("bottom").toInt();

            QRectF r(destRect.left() + left * scaleX,
                     destRect.top() + top * scaleY,
                     (right - left) * scaleX,
                     (bottom - top) * scaleY);
            painter->drawRect(r);
        }
    }
}

void QmlVideoItem::updateFrame(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        bool hadFrame = !m_currentFrame.isNull();
        m_currentFrame = pixmap;

        if (!hadFrame) {
            emit hasFrameChanged();
        }
    }

    // 触发重绘
    update();
}

void QmlVideoItem::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        bool hadFrame = !m_currentFrame.isNull();
        m_currentFrame = QPixmap();

        if (hadFrame) {
            emit hasFrameChanged();
        }
    }

    update();
}

void QmlVideoItem::setDetections(const QVariantList& dets)
{
    {
        QMutexLocker locker(&m_mutex);
        m_detections = dets;
    }
    emit detectionsChanged();
    update();
}
