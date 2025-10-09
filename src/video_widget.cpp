#include "video_widget.h"
#include <QDebug>

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
    , m_displayLabel(new QLabel(this))
    , m_layout(new QVBoxLayout(this))
{
    // 设置布局
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_layout->addWidget(m_displayLabel);
    setLayout(m_layout);

    // 配置QLabel用于显示视频
    m_displayLabel->setAlignment(Qt::AlignCenter);
    m_displayLabel->setScaledContents(false);  // 手动控制缩放以保持宽高比
    m_displayLabel->setStyleSheet("QLabel { background-color: #2E3440; }");

    // 设置最小尺寸
    setMinimumSize(320, 240);

    qDebug() << "VideoWidget created";
}

VideoWidget::~VideoWidget()
{
    qDebug() << "VideoWidget destroyed";
}

void VideoWidget::setFrame(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        return;
    }

    m_currentFrame = pixmap;
    updateDisplay();
}

void VideoWidget::clear()
{
    m_currentFrame = QPixmap();
    m_displayLabel->clear();
    m_displayLabel->setText("No Video");
}

void VideoWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateDisplay();
}

void VideoWidget::updateDisplay()
{
    if (m_currentFrame.isNull()) {
        return;
    }

    // 获取当前widget尺寸
    QSize widgetSize = m_displayLabel->size();

    // 缩放QPixmap以适应widget，保持宽高比
    QPixmap scaledPixmap = m_currentFrame.scaled(
        widgetSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation  // 使用平滑变换提高质量
    );

    m_displayLabel->setPixmap(scaledPixmap);
}
