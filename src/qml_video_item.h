#ifndef QML_VIDEO_ITEM_H
#define QML_VIDEO_ITEM_H

#include <QQuickPaintedItem>
#include <QPixmap>
#include <QPainter>
#include <QMutex>
#include <QRectF>

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
    Q_PROPERTY(int frameWidth READ frameWidth NOTIFY frameSizeChanged)
    Q_PROPERTY(int frameHeight READ frameHeight NOTIFY frameSizeChanged)
    Q_PROPERTY(QRectF viewWindow READ viewWindow WRITE setViewWindow NOTIFY viewWindowChanged)

public:
    explicit QmlVideoItem(QQuickItem *parent = nullptr);
    ~QmlVideoItem();

    void paint(QPainter *painter) override;

    bool hasFrame() const { return !m_currentFrame.isNull(); }

    QVariantList detections() const { return m_detections; }
    Q_INVOKABLE void setDetections(const QVariantList& dets);

    int modelInputSize() const { return m_modelInputSize; }
    void setModelInputSize(int v) { if (m_modelInputSize != v) { m_modelInputSize = v; emit modelInputSizeChanged(); update(); } }

    int frameWidth() const { return m_frameWidth; }
    int frameHeight() const { return m_frameHeight; }

    QRectF viewWindow() const { return m_viewWindow; }
    void setViewWindow(const QRectF& r) { m_viewWindow = r; emit viewWindowChanged(); update(); }

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
    void frameSizeChanged();
    void viewWindowChanged();

private:
    QPixmap m_currentFrame;
    QMutex m_mutex;
    QVariantList m_detections;
    int m_modelInputSize {640};
    int m_frameWidth {0};
    int m_frameHeight {0};
    QRectF m_viewWindow;
};

#endif // QML_VIDEO_ITEM_H
