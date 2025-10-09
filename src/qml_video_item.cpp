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
