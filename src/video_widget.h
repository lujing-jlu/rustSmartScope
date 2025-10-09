#ifndef VIDEO_WIDGET_H
#define VIDEO_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>
#include <QResizeEvent>

/**
 * @brief 原生Qt视频显示Widget，使用QPixmap直接渲染
 *
 * 这个widget提供高性能的视频帧显示，支持自动缩放和保持宽高比
 */
class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

    /**
     * @brief 更新显示的视频帧
     * @param pixmap 要显示的QPixmap
     */
    void setFrame(const QPixmap& pixmap);

    /**
     * @brief 清空显示
     */
    void clear();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateDisplay();

    QLabel* m_displayLabel;        // 用于显示QPixmap的QLabel
    QPixmap m_currentFrame;        // 当前帧
    QVBoxLayout* m_layout;         // 布局
};

#endif // VIDEO_WIDGET_H
