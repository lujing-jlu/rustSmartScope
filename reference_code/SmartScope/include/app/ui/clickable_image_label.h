#ifndef CLICKABLE_IMAGE_LABEL_H
#define CLICKABLE_IMAGE_LABEL_H

#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include "infrastructure/logging/logger.h"

/**
 * @brief 保持长宽比的标签
 */
class AspectRatioLabel : public QLabel
{
    Q_OBJECT
    
public:
    explicit AspectRatioLabel(QWidget* parent = nullptr, qreal ratio = 9.0/16.0) 
        : QLabel(parent), m_aspectRatio(ratio) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAlignment(Qt::AlignCenter);
    }
    
    void setAspectRatio(qreal ratio) {
        m_aspectRatio = ratio;
        updateGeometry();
    }
    
    qreal aspectRatio() const {
        return m_aspectRatio;
    }
    
    int heightForWidth(int width) const override {
        return static_cast<int>(width * m_aspectRatio);
    }
    
    QSize sizeHint() const override {
        int w = width();
        return QSize(w, heightForWidth(w));
    }
    
protected:
    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        QPixmap currentPixmap = pixmap(Qt::ReturnByValue);
        if (currentPixmap.isNull())
            return;
            
        setPixmap(currentPixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    
private:
    qreal m_aspectRatio; // 高宽比 (height/width)
};

/**
 * @brief 保持长宽比的可点击图像标签
 */
class ClickableImageLabel : public QLabel
{
    Q_OBJECT
    
public:
    explicit ClickableImageLabel(QWidget* parent = nullptr, qreal ratio = 9.0/16.0);
    
    void setAspectRatio(qreal ratio);
    qreal aspectRatio() const;
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    
    // 设置是否启用点击
    void setClickEnabled(bool enabled);
    
    // 获取是否启用点击
    bool isClickEnabled() const;
    
    // 设置图像原始尺寸
    void setOriginalImageSize(const QSize& size);
    
    // 计算原始图像中的坐标
    QPoint mapToImageCoords(const QPoint& labelPoint) const;

signals:
    // 点击信号：传递原始图像中的坐标和标签中的点击位置
    void clicked(int imageX, int imageY, const QPoint& labelPoint);
    
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
private:
    qreal m_aspectRatio;            ///< 高宽比 (height/width)
    bool m_clickEnabled;            ///< 是否启用点击
    bool m_isPressing;              ///< 是否正在按下鼠标
    QPoint m_currentPos;            ///< 当前鼠标位置
    QSize m_originalImageSize;      ///< 原始图像尺寸
};

#endif // CLICKABLE_IMAGE_LABEL_H 