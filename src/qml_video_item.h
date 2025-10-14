#ifndef QML_VIDEO_ITEM_H
#define QML_VIDEO_ITEM_H

#include <QQuickPaintedItem>
#include <QPixmap>
#include <QPainter>
#include <QMutex>

/**
 * @brief QML中可用的视频显示Item，直接渲染QPixmap
 *
 * 这个类继承自QQuickPaintedItem，可以在QML中直接使用
 * 支持高性能的QPixmap渲染
 */
class QmlVideoItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)
    Q_PROPERTY(QVariantList detections READ detections WRITE setDetections NOTIFY detectionsChanged)
    Q_PROPERTY(int modelInputSize READ modelInputSize WRITE setModelInputSize NOTIFY modelInputSizeChanged)

public:
    explicit QmlVideoItem(QQuickItem *parent = nullptr);
    ~QmlVideoItem();

    void paint(QPainter *painter) override;

    bool hasFrame() const { return !m_currentFrame.isNull(); }

    QVariantList detections() const { return m_detections; }
    Q_INVOKABLE void setDetections(const QVariantList& dets);

    int modelInputSize() const { return m_modelInputSize; }
    void setModelInputSize(int v) { if (m_modelInputSize != v) { m_modelInputSize = v; emit modelInputSizeChanged(); update(); } }

public slots:
    /**
     * @brief 更新显示的视频帧
     * @param pixmap 要显示的QPixmap
     */
    void updateFrame(const QPixmap& pixmap);

    /**
     * @brief 清空显示
     */
    void clear();

signals:
    void hasFrameChanged();
    void detectionsChanged();
    void modelInputSizeChanged();

private:
    QPixmap m_currentFrame;
    QMutex m_mutex;
    QVariantList m_detections;
    int m_modelInputSize {640};
};

#endif // QML_VIDEO_ITEM_H
