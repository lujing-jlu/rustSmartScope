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

    // 绘制AI检测框（将模型输入坐标系映射回原始帧大小，再映射到显示矩形）
    if (!m_detections.isEmpty()) {
        auto classColor = [](int cls)->QColor{
            static const QColor colors[] = {
                QColor(255,59,48), QColor(52,199,89), QColor(0,122,255), QColor(255,149,0),
                QColor(175,82,222), QColor(90,200,250), QColor(255,204,0), QColor(255,45,85),
                QColor(48,209,88), QColor(100,210,255)
            }; return colors[cls % (int)(sizeof(colors)/sizeof(colors[0]))];
        };

        const int ow = m_currentFrame.width();
        const int oh = m_currentFrame.height();
        const qreal modelW = qMax(1, m_modelInputSize);
        const qreal modelH = modelW; // 目前假设正方形 640x640
        const qreal scale = qMin(modelW / ow, modelH / oh);
        const qreal newW = ow * scale;
        const qreal newH = oh * scale;
        const qreal xOff = (modelW - newW) / 2.0;
        const qreal yOff = (modelH - newH) / 2.0;

        // 显示缩放
        const qreal dispScaleX = destRect.width() / ow;
        const qreal dispScaleY = destRect.height() / oh;

        painter->setRenderHint(QPainter::Antialiasing, true);

        for (const QVariant& v : m_detections) {
            const QVariantMap m = v.toMap();
            const int cls = m.value("class_id").toInt();
            const QString label = m.value("label").toString();

            // 模型坐标
            qreal ml = m.value("left").toReal();
            qreal mt = m.value("top").toReal();
            qreal mr = m.value("right").toReal();
            qreal mb = m.value("bottom").toReal();

            // 反映射到原始帧坐标
            qreal xl = (ml - xOff) / scale; qreal yt = (mt - yOff) / scale;
            qreal xr = (mr - xOff) / scale; qreal yb = (mb - yOff) / scale;

            // 裁剪
            xl = qBound(0.0, xl, (qreal)ow); xr = qBound(0.0, xr, (qreal)ow);
            yt = qBound(0.0, yt, (qreal)oh); yb = qBound(0.0, yb, (qreal)oh);

            // 转到显示坐标
            QRectF r(destRect.left() + xl * dispScaleX,
                     destRect.top() + yt * dispScaleY,
                     (xr - xl) * dispScaleX,
                     (yb - yt) * dispScaleY);

            // 美化绘制：阴影 + 圆角框 + 半透明边
            QColor base = classColor(cls);
            QColor edge = base; edge.setAlpha(220);
            QColor glow = base; glow.setAlpha(90);

            QPen pen(edge, 3.0);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            // 外层光晕
            QPen glowPen(glow, 8.0);
            painter->setPen(glowPen);
            painter->drawRoundedRect(r, 6, 6);
            // 主边框
            painter->setPen(pen);
            painter->drawRoundedRect(r, 6, 6);

            // 标签绘制（框内显示，字号放大两倍，不显示置信度）
            QString text = label.isEmpty() ? QString("%1").arg(cls) : label;
            QFont f = painter->font(); f.setPixelSize(36); f.setBold(true); painter->setFont(f);
            QFontMetrics fm(f);
            int tw = fm.horizontalAdvance(text) + 12;
            int th = fm.height() + 6;
            QRectF tb(r.left() + 2, r.top() + 2, tw, th);
            QColor bg(0,0,0,160);
            painter->setPen(Qt::NoPen);
            painter->setBrush(bg);
            painter->drawRoundedRect(tb, 4, 4);
            painter->setPen(QPen(Qt::white));
            painter->drawText(tb.adjusted(6, 2, -6, -2), Qt::AlignVCenter|Qt::AlignLeft, text);
        }
    }

    // 绘制视窗白框（当前主画面可见区域）
    if (m_frameWidth > 0 && m_frameHeight > 0 && m_viewWindow.width() > 0 && m_viewWindow.height() > 0) {
        const qreal sx = destRect.width() / m_frameWidth;
        const qreal sy = destRect.height() / m_frameHeight;
        QRectF vr(destRect.left() + m_viewWindow.left() * sx,
                  destRect.top() + m_viewWindow.top() * sy,
                  m_viewWindow.width() * sx,
                  m_viewWindow.height() * sy);
        // 当视窗等同于显示区域（或非常接近）时，不绘制内部白框，避免与外层边框重叠产生双线
        const qreal tol = 4.0; // 放宽容差
        // 判定1：比较映射后矩形与显示矩形
        bool nearFullDest = (qAbs(vr.left()   - destRect.left())   <= tol &&
                             qAbs(vr.top()    - destRect.top())    <= tol &&
                             qAbs(vr.right()  - destRect.right())  <= tol &&
                             qAbs(vr.bottom() - destRect.bottom()) <= tol);
        // 判定2：直接比较视窗（帧坐标系）是否覆盖完整帧
        bool nearFullFrame = (qAbs(m_viewWindow.left())   <= 1.0 &&
                              qAbs(m_viewWindow.top())    <= 1.0 &&
                              qAbs(m_viewWindow.width()  - m_frameWidth)  <= 1.0 &&
                              qAbs(m_viewWindow.height() - m_frameHeight) <= 1.0);
        if (!(nearFullDest || nearFullFrame)) {
            QPen pen(Qt::white, 3.0);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(vr, 4, 4);
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
        m_frameWidth = m_currentFrame.width();
        m_frameHeight = m_currentFrame.height();

        if (!hadFrame) {
            emit hasFrameChanged();
        }
    }

    emit frameSizeChanged();
    // 触发重绘
    update();
}

void QmlVideoItem::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        bool hadFrame = !m_currentFrame.isNull();
        m_currentFrame = QPixmap();
        m_frameWidth = 0;
        m_frameHeight = 0;

        if (hadFrame) {
            emit hasFrameChanged();
        }
    }

    emit frameSizeChanged();
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
