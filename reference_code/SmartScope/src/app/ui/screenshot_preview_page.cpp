#include "app/ui/screenshot_preview_page.h"
#include <QDebug>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFileInfoList>
#include <QImageReader>
#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QHideEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QPushButton>
#include <QToolButton>
#include <QStyle>
#include <QTouchEvent>
#include <QScroller>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QCloseEvent>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QDesktopWidget>
#include <QGraphicsOpacityEffect>
#include <QAbstractAnimation>
#include <QTimer>
#include "infrastructure/logging/logger.h"
#include "app/ui/toast_notification.h"
#include <QMenu>
#include <QAction>
#include <QFile>
#include "app/ui/utils/dialog_utils.h"

// 静态成员变量定义
QPointer<ScreenshotImagePreviewDialog> ScreenshotImagePreviewDialog::m_instance = nullptr;

// 使用日志宏
#define LOG_INFO(message) Logger::instance().info(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARNING(message) Logger::instance().warning(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(message) Logger::instance().error(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_DEBUG(message) Logger::instance().debug(message, __FILE__, __LINE__, __FUNCTION__)

// 截屏图片卡片实现
ScreenshotImageCard::ScreenshotImageCard(const QString &filePath, QWidget *parent)
    : QWidget(parent),
      m_filePath(filePath),
      m_fileInfo(filePath),
      m_imageLabel(nullptr),
      m_nameLabel(nullptr),
      m_infoLabel(nullptr)
{
    // 设置固定大小 - 调整卡片尺寸适合一行5个
    setFixedSize(260, 320);
    
    // 设置鼠标追踪
    setMouseTracking(true);
    
    // 设置焦点策略
    setFocusPolicy(Qt::StrongFocus);
    
    // 创建布局
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);  // 减小边距
    layout->setSpacing(6);
    
    // 创建图片标签 - 调整图片标签尺寸
    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(240, 200);  // 调整尺寸
    m_imageLabel->setMaximumSize(240, 200);  // 调整尺寸
    m_imageLabel->setScaledContents(false);
    m_imageLabel->setStyleSheet("background-color: #2A2A2A; border-radius: 5px;");
    layout->addWidget(m_imageLabel);
    
    // 创建名称标签
    m_nameLabel = new QLabel(this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setStyleSheet("color: white; font-size: 28px; font-weight: bold;");
    layout->addWidget(m_nameLabel);
    
    // 创建信息标签
    m_infoLabel = new QLabel(this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setStyleSheet("color: #AAAAAA; font-size: 24px;");
    layout->addWidget(m_infoLabel);
    
    // 设置样式表
    setStyleSheet(
        "ScreenshotImageCard {"
        "   background-color: #333333;"
        "   border-radius: 10px;"
        "   border: 1px solid #444444;"
        "}"
        "ScreenshotImageCard:hover {"
        "   background-color: #444444;"
        "   border: 1px solid #666666;"
        "}"
    );
    
    // 添加阴影效果（与拍照预览页面保持一致）
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(15);
    shadowEffect->setColor(QColor(0, 0, 0, 100));
    shadowEffect->setOffset(0, 2);
    setGraphicsEffect(shadowEffect);

    // 创建悬停动画属性（与拍照预览页面保持一致）
    setProperty("hovered", false);

    // 加载缩略图
    loadThumbnail();
}

QString ScreenshotImageCard::getFilePath() const
{
    return m_filePath;
}

void ScreenshotImageCard::loadThumbnail()
{
    // 检查文件是否存在
    if (!m_fileInfo.exists()) {
        LOG_WARNING(QString("截屏文件不存在: %1").arg(m_filePath));
        return;
    }
    
    // 获取文件名
    QString displayName = m_fileInfo.fileName();
    
    // 设置名称标签
    m_nameLabel->setText(displayName);
    
    // 设置信息标签
    QString sizeText = QString("%1 KB").arg(m_fileInfo.size() / 1024);
    QString dateText = m_fileInfo.lastModified().toString("yyyy-MM-dd HH:mm");
    m_infoLabel->setText(QString("%1 | %2").arg(sizeText).arg(dateText));
    
    // 加载图片
    QImageReader reader(m_filePath);
    reader.setAutoTransform(true);
    
    // 获取图片尺寸
    QSize imageSize = reader.size();
    if (!imageSize.isValid()) {
        LOG_WARNING(QString("无法获取截屏图片尺寸: %1").arg(m_filePath));
        return;
    }
    
    // 计算缩放比例 - 使用新的目标尺寸
    QSize targetSize(240, 200);  // 调整为与图片标签相同的尺寸
    QSize scaledSize = imageSize.scaled(targetSize, Qt::KeepAspectRatio);
    
    // 设置缩放尺寸
    reader.setScaledSize(scaledSize);
    
    // 读取图片
    QImage image = reader.read();
    if (image.isNull()) {
        LOG_WARNING(QString("无法读取截屏图片: %1, 错误: %2").arg(m_filePath).arg(reader.errorString()));
        return;
    }
    
    // 创建缩略图
    m_thumbnail = QPixmap::fromImage(image);
    
    // 设置图片标签
    m_imageLabel->setPixmap(m_thumbnail);
}

void ScreenshotImageCard::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked(m_filePath);
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ScreenshotImageCard::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    // 绘制边框（与拍照预览页面完全一致）
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 如果有焦点或被选中，绘制高亮边框
    if (hasFocus() || property("selected").toBool()) {
        // 使用蓝色高亮边框
        painter.setPen(QPen(QColor(0, 120, 215), 3));
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10);

        // 在高亮边框外绘制更亮的边缘
        painter.setPen(QPen(QColor(100, 180, 255, 150), 1));
        painter.drawRoundedRect(rect().adjusted(0, 0, 0, 0), 10, 10);
    } else if (property("hovered").toBool()) {
        // 悬停状态绘制淡蓝色边框
        painter.setPen(QPen(QColor(80, 150, 255, 100), 2));
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10);
    } else {
        // 普通状态下的边框
        painter.setPen(QPen(QColor(100, 100, 100, 100), 1));
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10);
    }
}

void ScreenshotImageCard::enterEvent(QEvent *event)
{
    // 设置悬停状态（与拍照预览页面完全一致）
    setProperty("hovered", true);

    // 创建上浮动画
    QPropertyAnimation *animation = new QPropertyAnimation(this, "pos");
    animation->setDuration(150);
    animation->setStartValue(pos());
    animation->setEndValue(pos() - QPoint(0, 5));
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    // 创建阴影增强动画
    QGraphicsDropShadowEffect *currentShadow = static_cast<QGraphicsDropShadowEffect*>(graphicsEffect());
    if (currentShadow) {
        QPropertyAnimation *shadowAnimation = new QPropertyAnimation(currentShadow, "blurRadius");
        shadowAnimation->setDuration(150);
        shadowAnimation->setStartValue(15);
        shadowAnimation->setEndValue(25);
        shadowAnimation->setEasingCurve(QEasingCurve::OutCubic);
        shadowAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    }

    update();
    QWidget::enterEvent(event);
}

void ScreenshotImageCard::leaveEvent(QEvent *event)
{
    // 取消悬停状态（与拍照预览页面完全一致）
    setProperty("hovered", false);

    // 创建下降动画
    QPropertyAnimation *animation = new QPropertyAnimation(this, "pos");
    animation->setDuration(150);
    animation->setStartValue(pos());
    animation->setEndValue(pos() + QPoint(0, 5));
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    // 创建阴影减弱动画
    QGraphicsDropShadowEffect *currentShadow = static_cast<QGraphicsDropShadowEffect*>(graphicsEffect());
    if (currentShadow) {
        QPropertyAnimation *shadowAnimation = new QPropertyAnimation(currentShadow, "blurRadius");
        shadowAnimation->setDuration(150);
        shadowAnimation->setStartValue(25);
        shadowAnimation->setEndValue(15);
        shadowAnimation->setEasingCurve(QEasingCurve::OutCubic);
        shadowAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    }

    update();
    QWidget::leaveEvent(event);
}

// 截屏预览对话框静态实例


// 截屏预览对话框实现
ScreenshotImagePreviewDialog::ScreenshotImagePreviewDialog(QWidget *parent)
    : QDialog(parent),
      m_imageLabel(nullptr),
      m_infoLabel(nullptr),
      m_imagePath(""),
      m_zoomFactor(1.0),
      m_userZoomed(false)
{
    // 设置对话框属性（与拍照预览对话框保持一致）
    setWindowTitle("截屏预览");
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);

    // 设置最小尺寸
    setMinimumSize(800, 600);

    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 设置对话框样式（与拍照预览对话框保持一致）
    setStyleSheet(
        "QDialog {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "       stop:0 rgba(30, 30, 30, 240), stop:1 rgba(20, 20, 20, 240));"
        "   border-radius: 15px;"
        "   border: 2px solid rgba(100, 100, 100, 100);"
        "}"
        "QLabel#titleLabel {"
        "   color: #FFFFFF;"
        "   font-size: 28px;"
        "   font-weight: bold;"
        "   padding: 0px;"
        "   margin: 0px;"
        "}"
    );

    // 创建容器
    QWidget *container = new QWidget(this);
    container->setObjectName("container");
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(15, 15, 15, 15);
    containerLayout->setSpacing(15);

    // 创建标题栏
    QWidget *titleBar = new QWidget(container);
    titleBar->setObjectName("titleBar");
    titleBar->setFixedHeight(60);
    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(20, 0, 20, 0);
    titleLayout->setSpacing(10);

    // 创建标题标签
    QLabel *titleLabel = new QLabel("截屏预览", titleBar);
    titleLabel->setObjectName("titleLabel");

    // 创建关闭按钮
    QPushButton *closeButton = new QPushButton(titleBar);
    closeButton->setObjectName("closeButton");
    closeButton->setIcon(QIcon(":/icons/close.svg"));
    closeButton->setIconSize(QSize(30, 30));
    closeButton->setFixedSize(60, 60);
    closeButton->setCursor(Qt::PointingHandCursor);

    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(closeButton);

    // 更新关闭按钮样式
    closeButton->setStyleSheet(
        "QPushButton#closeButton {"
        "   background-color: #D9534F;"
        "   border-radius: 20px;"
        "   border: none;"
        "   padding: 0px;"
        "   margin: 10px;"
        "}"
        "QPushButton#closeButton:hover {"
        "   background-color: #C9302C;"
        "}"
        "QPushButton#closeButton:pressed {"
        "   background-color: #B92C28;"
        "}"
    );

    // 创建工具栏
    QWidget *toolBar = new QWidget(container);
    toolBar->setObjectName("toolBar");
    toolBar->setFixedHeight(80);
    QHBoxLayout *toolLayout = new QHBoxLayout(toolBar);
    toolLayout->setContentsMargins(20, 0, 20, 0);
    toolLayout->setSpacing(20);

    // 创建缩小按钮
    QToolButton *zoomOutButton = new QToolButton(toolBar);
    zoomOutButton->setIcon(QIcon(":/icons/zoom_out.svg"));
    zoomOutButton->setIconSize(QSize(24, 24));
    zoomOutButton->setToolTip("缩小 (Ctrl+-)");
    zoomOutButton->setFixedSize(50, 50);

    QToolButton *resetZoomButton = new QToolButton(toolBar);
    resetZoomButton->setIcon(QIcon(":/icons/zoom_reset.svg"));
    resetZoomButton->setIconSize(QSize(24, 24));
    resetZoomButton->setToolTip("重置缩放 (Ctrl+0)");
    resetZoomButton->setFixedSize(50, 50);

    QToolButton *zoomInButton = new QToolButton(toolBar);
    zoomInButton->setIcon(QIcon(":/icons/zoom_in.svg"));
    zoomInButton->setIconSize(QSize(24, 24));
    zoomInButton->setToolTip("放大 (Ctrl++)");
    zoomInButton->setFixedSize(50, 50);

    // 添加按钮到工具栏布局
    toolLayout->addStretch();
    toolLayout->addWidget(zoomOutButton);
    toolLayout->addWidget(resetZoomButton);
    toolLayout->addWidget(zoomInButton);
    toolLayout->addStretch();

    // 设置工具按钮样式
    QString toolButtonStyle =
        "QToolButton {"
        "   background-color: #555555;"
        "   border-radius: 25px;"
        "   padding: 8px;"
        "}"
        "QToolButton:hover {"
        "   background-color: #666666;"
        "}"
        "QToolButton:pressed {"
        "   background-color: #444444;"
        "}";

    zoomInButton->setStyleSheet(toolButtonStyle);
    zoomOutButton->setStyleSheet(toolButtonStyle);
    resetZoomButton->setStyleSheet(toolButtonStyle);

    // 创建图片预览区域
    QScrollArea *scrollArea = new QScrollArea(container);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setStyleSheet(
        "QScrollArea {"
        "   background-color: rgba(20, 20, 20, 100);"
        "   border-radius: 10px;"
        "}"
        "QScrollBar:horizontal, QScrollBar:vertical {"
        "   background: rgba(40, 40, 40, 100);"
        "   height: 12px;"
        "   width: 12px;"
        "   border-radius: 6px;"
        "   margin: 0px;"
        "}"
        "QScrollBar::handle:horizontal, QScrollBar::handle:vertical {"
        "   background: rgba(100, 100, 100, 150);"
        "   border-radius: 5px;"
        "   min-width: 30px;"
        "   min-height: 30px;"
        "}"
        "QScrollBar::handle:horizontal:hover, QScrollBar::handle:vertical:hover {"
        "   background: rgba(120, 120, 120, 200);"
        "}"
        "QScrollBar::add-line, QScrollBar::sub-line {"
        "   width: 0px;"
        "   height: 0px;"
        "}"
        "QScrollBar::add-page, QScrollBar::sub-page {"
        "   background: none;"
        "}"
    );

    // 创建图片标签
    m_imageLabel = new QLabel(scrollArea);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(400, 300);
    m_imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_imageLabel->setStyleSheet("background-color: transparent; border-radius: 5px;");
    scrollArea->setWidget(m_imageLabel);

    // 创建信息标签
    m_infoLabel = new QLabel(container);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setStyleSheet("color: #CCCCCC; font-size: 22px; padding: 8px; background-color: rgba(40, 40, 40, 100); border-radius: 8px;");

    // 添加组件到容器布局
    containerLayout->addWidget(titleBar);
    containerLayout->addWidget(scrollArea, 1);
    containerLayout->addWidget(m_infoLabel);
    containerLayout->addWidget(toolBar);

    // 添加容器到主布局
    mainLayout->addWidget(container);

    // 连接信号槽
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(zoomInButton, &QToolButton::clicked, this, &ScreenshotImagePreviewDialog::zoomIn);
    connect(zoomOutButton, &QToolButton::clicked, this, &ScreenshotImagePreviewDialog::zoomOut);
    connect(resetZoomButton, &QToolButton::clicked, this, &ScreenshotImagePreviewDialog::resetZoom);

    // 安装事件过滤器，用于拖动窗口
    titleBar->installEventFilter(this);

    // 记录当前对话框，用于在外部访问
    m_instance = this;

    LOG_INFO("截屏图片预览对话框初始化完成");
}

void ScreenshotImagePreviewDialog::setImage(const QString &imagePath)
{
    m_imagePath = imagePath;

    // 重置用户缩放标记
    m_userZoomed = false;

    // 检查文件是否存在
    QFileInfo fileInfo(imagePath);
    if (!fileInfo.exists()) {
        LOG_WARNING(QString("截屏图片文件不存在: %1").arg(imagePath));
        m_imageLabel->setText("<p style='color:white; font-size:16px;'>图片文件不存在</p>");
        m_infoLabel->setText(imagePath);
        return;
    }

    // 加载图片
    QImageReader reader(imagePath);
    reader.setAutoTransform(true);

    // 获取图片尺寸
    QSize imageSize = reader.size();
    if (!imageSize.isValid()) {
        LOG_WARNING(QString("无法获取截屏图片尺寸: %1").arg(imagePath));
        m_imageLabel->setText("<p style='color:white; font-size:16px;'>无法获取图片尺寸</p>");
        m_infoLabel->setText(imagePath);
        return;
    }

    // 读取图片
    QImage image = reader.read();
    if (image.isNull()) {
        LOG_WARNING(QString("无法读取截屏图片: %1, 错误: %2").arg(imagePath).arg(reader.errorString()));
        m_imageLabel->setText("<p style='color:white; font-size:16px;'>无法读取图片</p>");
        m_infoLabel->setText(imagePath);
        return;
    }

    // 保存原始图像
    m_originalImage = image;

    // 计算合适的缩放比例，使图像适应窗口大小
    QSize scrollAreaSize = this->size() - QSize(100, 200); // 减去边距和其他控件的空间
    double widthRatio = scrollAreaSize.width() / (double)imageSize.width();
    double heightRatio = scrollAreaSize.height() / (double)imageSize.height();
    m_zoomFactor = qMin(widthRatio, heightRatio);

    // 如果图像比窗口小，则不放大
    if (m_zoomFactor > 1.0) {
        m_zoomFactor = 1.0;
    }

    // 获取显示名称
    QString displayName = fileInfo.fileName();

    // 更新图像显示
    updateImageDisplay();

    // 设置信息标签
    QString sizeText = QString("%1x%2").arg(imageSize.width()).arg(imageSize.height());
    QString fileSizeText = QString("%1 KB").arg(fileInfo.size() / 1024);
    QString dateText = fileInfo.lastModified().toString("yyyy-MM-dd HH:mm:ss");
    m_infoLabel->setText(QString("<span style='color:#FFFFFF;'>%1</span> | %2 | %3 | %4").arg(
        displayName).arg(sizeText).arg(fileSizeText).arg(dateText));

    // 查找标题标签并更新
    QList<QLabel*> labels = findChildren<QLabel*>("titleLabel");
    if (!labels.isEmpty()) {
        labels.first()->setText(QString("截屏预览 - %1").arg(displayName));
    }

    // 显示对话框并添加淡入动画
    setWindowOpacity(0.0);
    show();

    // 显示后再次调用图像居中，确保滚动条初始位置正确
    QTimer::singleShot(50, this, [this]() {
        // 再次调用更新图像显示，确保初始显示时图片居中
        updateImageDisplay();
    });

    // 淡入动画
    QPropertyAnimation *animation = new QPropertyAnimation(this, "windowOpacity");
    animation->setDuration(200);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void ScreenshotImagePreviewDialog::closeIfOpen()
{
    if (m_instance && m_instance->isVisible()) {
        m_instance->close();
    }
}

void ScreenshotImagePreviewDialog::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Escape:
            close();
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomIn();
            break;
        case Qt::Key_Minus:
            zoomOut();
            break;
        case Qt::Key_0:
            resetZoom();
            break;
        default:
            QDialog::keyPressEvent(event);
            break;
    }
}

void ScreenshotImagePreviewDialog::zoomIn()
{
    m_userZoomed = true;  // 标记用户手动缩放
    m_zoomFactor *= 1.2;
    updateImageDisplay();
}

void ScreenshotImagePreviewDialog::zoomOut()
{
    m_userZoomed = true;  // 标记用户手动缩放
    m_zoomFactor /= 1.2;
    if (m_zoomFactor < 0.1) m_zoomFactor = 0.1;
    updateImageDisplay();
}

void ScreenshotImagePreviewDialog::resetZoom()
{
    m_userZoomed = false;  // 重置标记，表示使用自动缩放
    m_zoomFactor = 1.0;
    updateImageDisplay();
}

void ScreenshotImagePreviewDialog::updateImageDisplay()
{
    if (m_originalImage.isNull()) return;

    // 计算新尺寸
    QSize newSize = QSize(m_originalImage.width() * m_zoomFactor, m_originalImage.height() * m_zoomFactor);

    // 创建缩放图片
    QPixmap pixmap = QPixmap::fromImage(m_originalImage).scaled(
        newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 设置图片标签
    m_imageLabel->setPixmap(pixmap);

    // 重新设置图片标签尺寸，以适应缩放后的图片
    m_imageLabel->setFixedSize(newSize);

    // 确保图片在滚动区域中居中显示
    QScrollArea* scrollArea = qobject_cast<QScrollArea*>(m_imageLabel->parent());
    if (scrollArea) {
        // 计算滚动条位置，使图片居中
        int hValue = (newSize.width() - scrollArea->viewport()->width()) / 2;
        int vValue = (newSize.height() - scrollArea->viewport()->height()) / 2;

        // 设置滚动条位置
        if (hValue > 0) scrollArea->horizontalScrollBar()->setValue(hValue);
        if (vValue > 0) scrollArea->verticalScrollBar()->setValue(vValue);
    }
}

void ScreenshotImagePreviewDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);

    // 如果用户没有手动缩放，自动调整缩放比例以适应窗口
    if (!m_userZoomed && !m_originalImage.isNull()) {
        // 获取可用空间（减去工具栏高度）
        QSize availableSize = size() - QSize(40, 100); // 留出边距和工具栏空间

        // 计算适合的缩放比例
        double scaleX = double(availableSize.width()) / m_originalImage.width();
        double scaleY = double(availableSize.height()) / m_originalImage.height();
        double oldZoomFactor = m_zoomFactor;
        m_zoomFactor = qMin(scaleX, scaleY);

        // 限制最小缩放比例
        if (m_zoomFactor < 0.1) {
            m_zoomFactor = 0.1;
        }

        // 如果缩放因子发生变化，更新图像显示
        if (qAbs(oldZoomFactor - m_zoomFactor) > 0.001) {
            // 使用单次定时器延迟更新，确保窗口调整完成后再居中
            QTimer::singleShot(10, this, [this]() {
                updateImageDisplay();
            });
        }
    }
}

bool ScreenshotImagePreviewDialog::eventFilter(QObject *watched, QEvent *event)
{
    static QPoint dragPosition;

    // 处理标题栏的鼠标事件，实现拖动功能
    if (watched->objectName() == "titleBar" || watched->parent()->objectName() == "titleBar") {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                dragPosition = mouseEvent->globalPos() - frameGeometry().topLeft();
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                move(mouseEvent->globalPos() - dragPosition);
                event->accept();
                return true;
            }
        }
    }

    return QDialog::eventFilter(watched, event);
}

void ScreenshotImagePreviewDialog::closeEvent(QCloseEvent *event)
{
    // 创建淡出动画
    QPropertyAnimation *animation = new QPropertyAnimation(this, "windowOpacity");
    animation->setDuration(200);
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    // 连接动画完成信号，关闭窗口
    connect(animation, &QPropertyAnimation::finished, this, &QDialog::accept);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    event->ignore(); // 忽略关闭事件，让动画完成后再关闭
}

// 截屏预览页面实现
ScreenshotPreviewPage::ScreenshotPreviewPage(QWidget *parent)
    : BasePage("截屏预览", parent),
      m_currentWorkPath(""),
      m_fileWatcher(nullptr),
      m_reloadTimer(nullptr),
      m_scrollArea(nullptr),
      m_scrollContent(nullptr),
      m_gridLayout(nullptr),
      m_emptyLabel(nullptr),
      m_columnsCount(5),  // 固定设置列数为5
      m_cardSpacing(15),  // 减小间距使每行能显示5个
      m_cardWidth(260),   // 稍微减小卡片宽度，确保一行能显示5个
      m_isLoading(false),
      m_previewDialog(nullptr),
      m_isScrolling(false),
      m_lastMousePos(),
      m_pressPos(),
      m_scrollSpeed(2),
      m_scrollThreshold(10),
      m_lastClickTime(0),
      m_lastClickedCard(nullptr),
      m_selectedCard(nullptr)
{
    // 初始化内容
    initContent();

    // 创建文件系统监视器
    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, &ScreenshotPreviewPage::handleDirectoryChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &ScreenshotPreviewPage::handleFileChanged);

    // 创建重新加载定时器
    m_reloadTimer = new QTimer(this);
    m_reloadTimer->setSingleShot(true);
    connect(m_reloadTimer, &QTimer::timeout, this, &ScreenshotPreviewPage::loadImages);

    // 创建图片预览对话框
    m_previewDialog = new ScreenshotImagePreviewDialog(this);

    // 长按计时器
    m_longPressTimer = new QTimer(this);
    m_longPressTimer->setSingleShot(true);
    connect(m_longPressTimer, &QTimer::timeout, this, &ScreenshotPreviewPage::handleLongPress);

    // 安装事件过滤器到滚动区域视口
    m_scrollArea->viewport()->installEventFilter(this);

    LOG_INFO("截屏预览页面构造完成");
}

ScreenshotPreviewPage::~ScreenshotPreviewPage()
{
    // 清除图片卡片
    clearImageCards();
}

void ScreenshotPreviewPage::setCurrentWorkPath(const QString &path)
{
    // 构建截屏文件路径（统一到 root/Screenshots）
    QString screenshotPath = path;
    if (!screenshotPath.endsWith("/Screenshots")) {
        // 允许上层传入 root，强制指向 root/Screenshots
        int idx = screenshotPath.lastIndexOf("/");
        if (idx > 0) {
            screenshotPath = screenshotPath.left(idx);
        }
        screenshotPath = screenshotPath + "/Screenshots";
    }

    if (m_currentWorkPath != screenshotPath) {
        // 如果之前有监视的路径，先移除
        if (!m_currentWorkPath.isEmpty() && m_fileWatcher->directories().contains(m_currentWorkPath)) {
            m_fileWatcher->removePath(m_currentWorkPath);
        }

        // 设置新路径
        m_currentWorkPath = screenshotPath;
        LOG_INFO(QString("截屏预览页面设置当前工作路径: %1").arg(screenshotPath));

        // 添加新路径到监视器
        if (!m_currentWorkPath.isEmpty()) {
            m_fileWatcher->addPath(m_currentWorkPath);
        }

        // 加载图片
        loadImages();
    }
}

void ScreenshotPreviewPage::handleDirectoryChanged(const QString &path)
{
    Q_UNUSED(path)
    LOG_DEBUG("截屏目录发生变化，延迟重新加载");

    // 使用定时器延迟重新加载，避免频繁刷新
    m_reloadTimer->start(500);
}

void ScreenshotPreviewPage::handleFileChanged(const QString &path)
{
    Q_UNUSED(path)
    LOG_DEBUG("截屏文件发生变化，延迟重新加载");

    // 使用定时器延迟重新加载，避免频繁刷新
    m_reloadTimer->start(500);
}

void ScreenshotPreviewPage::initContent()
{
    // 设置内容区域的边距，避开状态栏和菜单栏（与拍照预览页面保持一致）
    m_contentWidget->setContentsMargins(80, STATUS_BAR_HEIGHT, 80, 160);  // 左80px、上边距避开状态栏，右80px，下160px避开菜单栏

    // 创建滚动区域
    m_scrollArea = new QScrollArea(m_contentWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 禁用水平滚动条
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet("background-color: #1E1E1E;");

    // 设置滚动区域属性（与拍照预览页面保持一致）
    m_scrollArea->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical {"
        "   background-color: rgba(40, 40, 40, 100);"
        "   width: 12px;"
        "   margin: 0px;"
        "   border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background-color: rgba(80, 80, 80, 200);"
        "   min-height: 30px;"
        "   border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "   background-color: rgba(100, 100, 100, 250);"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "   background: none;"
        "}"
    );

    // 创建滚动内容区域
    m_scrollContent = new QWidget(m_scrollArea);
    m_scrollContent->setStyleSheet("background-color: transparent;");

    // 创建网格布局
    m_gridLayout = new QGridLayout(m_scrollContent);
    m_gridLayout->setContentsMargins(15, 15, 15, 15);  // 减小边距（与拍照预览页面保持一致）
    m_gridLayout->setSpacing(m_cardSpacing);
    m_gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);  // 修改为左对齐（与拍照预览页面保持一致）

    // 设置滚动内容
    m_scrollArea->setWidget(m_scrollContent);

    // 创建空目录提示标签
    m_emptyLabel = new QLabel(m_scrollArea);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: #AAAAAA; font-size: 36px; background-color: transparent;");  // 放大空目录提示字体（与拍照预览页面保持一致）
    m_emptyLabel->setText("当前目录没有截屏文件");
    m_emptyLabel->setVisible(false);

    // 添加滚动区域到内容布局
    m_contentLayout->addWidget(m_scrollArea);

    LOG_INFO("截屏预览页面内容初始化完成");
}

void ScreenshotPreviewPage::loadImages()
{
    // 防止重复加载
    if (m_isLoading) {
        return;
    }

    m_isLoading = true;
    LOG_INFO(QString("开始加载截屏图片，路径: %1").arg(m_currentWorkPath));

    // 清除现有图片卡片
    clearImageCards();

    // 检查路径是否有效
    if (m_currentWorkPath.isEmpty() || !QDir(m_currentWorkPath).exists()) {
        LOG_WARNING(QString("截屏工作路径无效: %1").arg(m_currentWorkPath));
        if (m_emptyLabel) {
            m_emptyLabel->setText("截屏目录不存在");
            m_emptyLabel->show();
        }
        m_isLoading = false;
        return;
    }

    // 获取目录中的图片文件（与拍照预览页面保持一致的文件过滤器顺序）
    QDir dir(m_currentWorkPath);
    QStringList filters;
    filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif";
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::Time); // 按修改时间排序，最新的在前面

    QFileInfoList fileList = dir.entryInfoList();

    // 如果没有图片，显示空目录提示
    if (fileList.isEmpty()) {
        LOG_INFO(QString("截屏目录中没有图片: %1").arg(m_currentWorkPath));
        m_emptyLabel->setText("当前目录没有截屏文件");
        m_emptyLabel->show();
        m_isLoading = false;
        return;
    }

    // 隐藏空目录提示（与拍照预览页面保持一致）
    m_emptyLabel->hide();

    // 为每个截屏文件创建图片卡片
    for (const QFileInfo &fileInfo : fileList) {
        ScreenshotImageCard *card = createImageCard(fileInfo.absoluteFilePath());
        if (card) {
            m_imageCards.append(card);
            LOG_DEBUG(QString("创建截屏图片卡片: %1").arg(fileInfo.fileName()));
        }
    }

    // 更新布局
    updateLayout();

    // 隐藏加载提示
    if (m_emptyLabel) {
        if (m_imageCards.isEmpty()) {
            m_emptyLabel->setText("当前目录没有截屏文件");
            m_emptyLabel->show();
        } else {
            m_emptyLabel->hide();
        }
    }

    LOG_INFO(QString("截屏图片加载完成，共 %1 张图片").arg(m_imageCards.size()));
    m_isLoading = false;
}

ScreenshotImageCard* ScreenshotPreviewPage::createImageCard(const QString &filePath)
{
    // 创建图片卡片
    ScreenshotImageCard *card = new ScreenshotImageCard(filePath, m_scrollContent);

    // 确保图片卡片能接收鼠标事件（与拍照预览页面保持一致）
    card->setMouseTracking(true);
    card->setAttribute(Qt::WA_Hover, true);

    // 初始化选中状态
    card->setProperty("selected", false);

    LOG_DEBUG(QString("创建截屏图片卡片: %1").arg(filePath));
    return card;
}

void ScreenshotPreviewPage::clearImageCards()
{
    // 从布局中移除所有卡片（与拍照预览页面保持一致）
    if (m_gridLayout) {
        QLayoutItem *item;
        while ((item = m_gridLayout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->hide();
                item->widget()->deleteLater();
            }
            delete item;
        }
    }

    // 清空卡片列表
    m_imageCards.clear();

    // 重置选中卡片指针（与拍照预览页面保持一致）
    m_selectedCard = nullptr;
    m_lastClickedCard = nullptr;

    LOG_INFO("清除所有截屏图片卡片");
}

void ScreenshotPreviewPage::updateLayout()
{
    if (!m_gridLayout || !m_scrollContent || m_imageCards.isEmpty()) {
        return;
    }

    LOG_DEBUG(QString("更新布局，固定列数: %1").arg(m_columnsCount));

    // 从布局中移除所有卡片
    QLayoutItem *item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item;
    }

    // 重新添加卡片到布局，从左到右排列（与拍照预览页面保持一致）
    for (int i = 0; i < m_imageCards.size(); ++i) {
        int row = i / m_columnsCount;
        int col = i % m_columnsCount;
        m_gridLayout->addWidget(m_imageCards[i], row, col, Qt::AlignLeft);  // 左对齐
        m_imageCards[i]->show();
    }

    // 不再额外调整滚动内容的宽度，让其自然适应网格布局

    LOG_INFO(QString("布局更新完成，共 %1 张截屏图片，固定 %2 列").arg(m_imageCards.size()).arg(m_columnsCount));
}

void ScreenshotPreviewPage::showImagePreview(const QString &imagePath)
{
    if (m_previewDialog) {
        // 获取屏幕尺寸
        QScreen *screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->geometry();
        QSize screenSize = screenGeometry.size();

        // 计算对话框尺寸（屏幕的80%）
        QSize dialogSize(screenSize.width() * 0.8, screenSize.height() * 0.8);
        m_previewDialog->resize(dialogSize);

        // 计算顶部偏移（为状态栏留出空间）
        int topOffset = 80; // 状态栏高度
        int availableHeight = screenSize.height() - topOffset;

        // 设置对话框位置
        int x = (screenSize.width() - dialogSize.width()) / 2;
        int y = topOffset + (availableHeight - dialogSize.height()) / 2;
        m_previewDialog->move(x, y);

        // 设置图片并显示对话框
        m_previewDialog->setImage(imagePath);
        m_previewDialog->exec();
    }
}

void ScreenshotPreviewPage::handleLongPress()
{
    QPoint cursorPos = m_scrollArea->viewport()->mapFromGlobal(QCursor::pos());
    QPoint contentPos = cursorPos + QPoint(m_scrollArea->horizontalScrollBar()->value(), m_scrollArea->verticalScrollBar()->value());
    QWidget *clickedWidget = m_scrollContent->childAt(contentPos);
    ScreenshotImageCard *imageCard = nullptr;
    while (clickedWidget && !imageCard) {
        imageCard = qobject_cast<ScreenshotImageCard*>(clickedWidget);
        if (!imageCard) {
            clickedWidget = clickedWidget->parentWidget();
            if (clickedWidget == m_scrollContent) break;
        }
    }
    if (!imageCard) return;
    m_longPressTriggered = true;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu {"
        "  background-color: #2B2B2B;"
        "  border: 2px solid #666666;"
        "  padding: 18px;"
        "}"
        "QMenu::item {"
        "  color: #FFFFFF;"
        "  padding: 24px 48px;"
        "  font-size: 36px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #3D3D3D;"
        "}"
        "QMenu::separator {"
        "  height: 2px;"
        "  background: #555555;"
        "  margin: 12px 6px;"
        "}"
    );
    QAction *deleteAction = menu.addAction("删除");
    QAction *chosen = menu.exec(QCursor::pos());
    if (chosen == deleteAction) {
        using SmartScope::App::Ui::DialogUtils;
        QMessageBox::StandardButton reply = DialogUtils::showStyledConfirmationDialog(
            this,
            "确认删除",
            QString("确定要删除该文件吗？\n%1").arg(imageCard->getFilePath()),
            "删除",
            "取消"
        );
        if (reply == QMessageBox::Yes) {
            if (QFile::remove(imageCard->getFilePath())) {
                showToast(this, "文件已删除", 1500);
                loadImages();
            } else {
                showToast(this, "删除失败", 2000, ToastNotification::BottomCenter, ToastNotification::Error);
            }
        }
    }
}

bool ScreenshotPreviewPage::eventFilter(QObject *watched, QEvent *event)
{
    static QPoint dragPosition;

    // 处理标题栏的鼠标事件，实现拖动功能
    if (watched == m_scrollArea->viewport()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_isScrolling = true;
                m_lastMousePos = mouseEvent->pos();
                m_pressPos = mouseEvent->pos();
                m_scrollArea->viewport()->setCursor(Qt::ClosedHandCursor);

                // 启动长按计时器
                m_longPressTriggered = false;
                if (!m_longPressTimer) {
                    m_longPressTimer = new QTimer(this);
                    m_longPressTimer->setSingleShot(true);
                    connect(m_longPressTimer, &QTimer::timeout, this, &ScreenshotPreviewPage::handleLongPress);
                }
                m_longPressTimer->start(600);

                // 记录时间戳，用于后续双击判断
                m_lastClickTime = QDateTime::currentMSecsSinceEpoch();
            }
            break; }
        case QEvent::MouseMove: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (m_isScrolling) {
                int deltaY = m_lastMousePos.y() - mouseEvent->pos().y();
                QScrollBar *vScrollBar = m_scrollArea->verticalScrollBar();
                if (vScrollBar && qAbs(deltaY) > 2) {
                    vScrollBar->setValue(vScrollBar->value() + deltaY);
                    if (m_longPressTimer && m_longPressTimer->isActive()) m_longPressTimer->stop();
                }
                m_lastMousePos = mouseEvent->pos();
            }
            break; }
        case QEvent::MouseButtonRelease: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && m_isScrolling) {
                if (m_longPressTimer && m_longPressTimer->isActive()) m_longPressTimer->stop();
                if (m_longPressTriggered) {
                    m_isScrolling = false;
                    m_scrollArea->viewport()->setCursor(Qt::ArrowCursor);
                    return true;
                }

                // 与拍照预览一致：小位移+短时间窗口 -> 单击/双击判定
                QPoint moveDelta = m_pressPos - mouseEvent->pos();
                qint64 timeDelta = QDateTime::currentMSecsSinceEpoch() - m_lastClickTime;
                if (moveDelta.manhattanLength() < 10 && timeDelta < 300) {
                    QPoint viewportPos = mouseEvent->pos();
                    QPoint contentPos = viewportPos + QPoint(m_scrollArea->horizontalScrollBar()->value(),
                                                           m_scrollArea->verticalScrollBar()->value());
                    QWidget *clickedWidget = m_scrollContent->childAt(contentPos);
                    ScreenshotImageCard *imageCard = nullptr;
                    while (clickedWidget && !imageCard) {
                        imageCard = qobject_cast<ScreenshotImageCard*>(clickedWidget);
                        if (!imageCard) {
                            clickedWidget = clickedWidget->parentWidget();
                            if (clickedWidget == m_scrollContent) break;
                        }
                    }

                    if (imageCard) {
                        if (m_selectedCard) {
                            m_selectedCard->setFocus(Qt::NoFocusReason);
                            m_selectedCard->setProperty("selected", false);
                            m_selectedCard->update();
                        }
                        imageCard->setFocus(Qt::MouseFocusReason);
                        imageCard->setProperty("selected", true);
                        imageCard->update();
                        m_selectedCard = imageCard;

                        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
                        if (m_lastClickedCard == imageCard && currentTime - m_lastClickTime < 500) {
                            LOG_INFO(QString("双击截屏图片: %1").arg(imageCard->getFilePath()));
                            showImagePreview(imageCard->getFilePath());
                        }
                        m_lastClickedCard = imageCard;
                        m_lastClickTime = currentTime;
                    } else {
                        if (m_selectedCard) {
                            m_selectedCard->setFocus(Qt::NoFocusReason);
                            m_selectedCard->setProperty("selected", false);
                            m_selectedCard->update();
                            m_selectedCard = nullptr;
                        }
                    }
                }

                m_isScrolling = false;
                m_scrollArea->viewport()->setCursor(Qt::ArrowCursor);
            }
            break; }
        default: break;
        }
    }

    // 保留原有的标题栏拖动逻辑（若无则不会命中）
    if (watched->objectName() == "titleBar" || watched->parent()->objectName() == "titleBar") {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                dragPosition = mouseEvent->globalPos() - frameGeometry().topLeft();
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                move(mouseEvent->globalPos() - dragPosition);
                event->accept();
                return true;
            }
        }
    }

    return BasePage::eventFilter(watched, event);
}
