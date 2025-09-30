#include "app/ui/preview_page.h"
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
#include "infrastructure/logging/logger.h"
#include "app/ui/toast_notification.h"
#include <QMenu>
#include <QAction>
#include <QFile>
#include "app/ui/utils/dialog_utils.h"

// 使用日志宏
#define LOG_INFO(message) Logger::instance().info(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARNING(message) Logger::instance().warning(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(message) Logger::instance().error(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_DEBUG(message) Logger::instance().debug(message, __FILE__, __LINE__, __FUNCTION__)

// 图片卡片实现
ImageCard::ImageCard(const QString &filePath, QWidget *parent)
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
        "ImageCard {"
        "   background-color: #333333;"
        "   border-radius: 10px;"
        "   border: 1px solid #444444;"
        "}"
        "ImageCard:hover {"
        "   background-color: #444444;"
        "   border: 1px solid #666666;"
        "}"
    );
    
    // 添加阴影效果
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(15);
    shadowEffect->setColor(QColor(0, 0, 0, 100));
    shadowEffect->setOffset(0, 2);
    setGraphicsEffect(shadowEffect);
    
    // 创建悬停动画属性
    setProperty("hovered", false);
    
    // 更新缩略图
    updateThumbnail();
}

QString ImageCard::getFilePath() const
{
    return m_filePath;
}

QFileInfo ImageCard::getFileInfo() const
{
    return m_fileInfo;
}

QDateTime ImageCard::getModifiedTime() const
{
    return m_fileInfo.lastModified();
}

void ImageCard::updateThumbnail()
{
    // 检查文件是否存在
    if (!m_fileInfo.exists()) {
        LOG_WARNING(QString("图片文件不存在: %1").arg(m_filePath));
        return;
    }
    
    // 获取文件名并移除"左相机"字样
    QString displayName = m_fileInfo.fileName();
    displayName.replace("左相机", "").replace("_左相机", "").replace("左相机_", ""); // 移除各种可能的"左相机"字样
    
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
        LOG_WARNING(QString("无法获取图片尺寸: %1").arg(m_filePath));
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
        LOG_WARNING(QString("无法读取图片: %1, 错误: %2").arg(m_filePath).arg(reader.errorString()));
        return;
    }
    
    // 创建缩略图
    m_thumbnail = QPixmap::fromImage(image);
    
    // 设置图片标签
    m_imageLabel->setPixmap(m_thumbnail);
}

void ImageCard::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked(m_filePath);
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ImageCard::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    
    // 绘制边框
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

// 添加鼠标进入和离开事件处理
void ImageCard::enterEvent(QEvent *event)
{
    // 设置悬停状态
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

void ImageCard::leaveEvent(QEvent *event)
{
    // 取消悬停状态
    setProperty("hovered", false);
    
    // 创建下沉动画
    QPropertyAnimation *animation = new QPropertyAnimation(this, "pos");
    animation->setDuration(150);
    animation->setStartValue(pos());
    animation->setEndValue(pos() + QPoint(0, 5));
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    
    // 创建阴影恢复动画
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

// 图片预览对话框实现
ImagePreviewDialog::ImagePreviewDialog(QWidget *parent)
    : QDialog(parent),
      m_imageLabel(nullptr),
      m_infoLabel(nullptr),
      m_imagePath(""),
      m_zoomFactor(1.0),
      m_userZoomed(false)
{
    // 设置无边框窗口
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowOpacity(0.0); // 初始透明度为0，用于淡入效果
    
    // 设置窗口大小为屏幕的80%
    QScreen *screen = QApplication::primaryScreen();
    QSize screenSize = screen->availableSize();
    int width = screenSize.width() * 0.8;
    int height = screenSize.height() * 0.8;
    resize(width, height);
    
    // 计算窗口位置，避开状态栏、工具栏和菜单栏
    // 状态栏和工具栏位于顶部，菜单栏位于底部，左侧有工具栏
    int leftOffset = 80; // 避开左侧工具栏
    int topOffset = 80;  // 避开顶部状态栏和工具栏
    int availableWidth = screenSize.width() - leftOffset * 2;
    int availableHeight = screenSize.height() - topOffset - 160; // 底部保留160像素避开菜单栏
    
    // 调整窗口大小，确保在可用区域内
    if (width > availableWidth) width = availableWidth;
    if (height > availableHeight) height = availableHeight;
    resize(width, height);
    
    // 设置窗口位置居中但偏上，避开菜单栏
    int x = (screenSize.width() - width) / 2;
    int y = topOffset + (availableHeight - height) / 2;
    move(x, y);
    
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20); // 增加边距以容纳阴影
    mainLayout->setSpacing(0);
    
    // 创建主容器，用于设置圆角和背景
    QWidget *container = new QWidget(this);
    container->setObjectName("previewContainer");
    container->setStyleSheet(
        "QWidget#previewContainer {"
        "    background-color: #252526;"
        "    border-radius: 12px;"
        "    border: 1px solid #444;"
        "    padding: 25px;"
        "}"
        "QLabel {"
        "    color: #E0E0E0;"
        "    background-color: transparent;"
        "    padding: 5px;"
        "    font-size: 20pt;"
        "}"
        "QPushButton#closeButton {"
        "    background-color: #D9534F;"
        "    color: white;"
        "    padding: 10px 25px;"
        "    border-radius: 8px;"
        "    border: none;"
        "    min-height: 45px;"
        "    min-width: 160px;"
        "    font-size: 18pt;"
        "    margin: 10px 15px;"
        "}"
        "QPushButton#closeButton:hover { background-color: #C9302C; }"
        "QPushButton#closeButton:pressed { background-color: #AC2925; }"
        "QToolButton {"
        "    background-color: #555555;"
        "    color: white;"
        "    padding: 10px 25px;"
        "    border-radius: 8px;"
        "    border: none;"
        "    min-height: 45px;"
        "    min-width: 160px;"
        "    font-size: 18pt;"
        "    margin: 10px 15px;"
        "}"
        "QToolButton:hover { background-color: #666666; }"
        "QToolButton:pressed { background-color: #444444; }"
        "QScrollArea {"
        "    border: none;"
        "    background-color: #333333;"
        "    border-radius: 5px;"
        "}"
        "QScrollBar:vertical {"
        "    border: none;"
        "    background: #333333;"
        "    width: 18px;"
        "    margin: 0px 0px 0px 0px;"
        "    border-radius: 9px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #555555;"
        "    border-radius: 9px;"
        "    min-height: 40px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: #666666; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    border: none;"
        "    background: none;"
        "    height: 0px;"
        "}"
        "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical {"
        "    background: none;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "    background: none;"
        "}"
    );
    
    // 添加阴影效果
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(20);
    shadowEffect->setColor(QColor(0, 0, 0, 180));
    shadowEffect->setOffset(0, 0);
    container->setGraphicsEffect(shadowEffect);
    
    // 创建容器布局
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
    QLabel *titleLabel = new QLabel("图片预览", titleBar);
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
    connect(zoomInButton, &QToolButton::clicked, this, &ImagePreviewDialog::zoomIn);
    connect(zoomOutButton, &QToolButton::clicked, this, &ImagePreviewDialog::zoomOut);
    connect(resetZoomButton, &QToolButton::clicked, this, &ImagePreviewDialog::resetZoom);
    
    // 安装事件过滤器，用于拖动窗口
    titleBar->installEventFilter(this);
    
    // 记录当前对话框，用于在外部访问
    m_instance = this;
    
    LOG_INFO("图片预览对话框初始化完成");
}

void ImagePreviewDialog::setImage(const QString &imagePath)
{
    m_imagePath = imagePath;
    
    // 重置用户缩放标记
    m_userZoomed = false;
    
    // 检查文件是否存在
    QFileInfo fileInfo(imagePath);
    if (!fileInfo.exists()) {
        LOG_WARNING(QString("图片文件不存在: %1").arg(imagePath));
        m_imageLabel->setText("<p style='color:white; font-size:16px;'>图片文件不存在</p>");
        m_infoLabel->setText(imagePath);
        return;
    }
    
    // 获取文件名并移除"左相机"字样
    QString displayName = fileInfo.fileName();
    displayName.replace("左相机", "").replace("_左相机", "").replace("左相机_", ""); // 移除各种可能的"左相机"字样
    
    // 加载图片
    QImageReader reader(imagePath);
    reader.setAutoTransform(true);
    
    // 获取图片尺寸
    QSize imageSize = reader.size();
    if (!imageSize.isValid()) {
        LOG_WARNING(QString("无法获取图片尺寸: %1").arg(imagePath));
        m_imageLabel->setText("<p style='color:white; font-size:16px;'>无法获取图片尺寸</p>");
        m_infoLabel->setText(imagePath);
        return;
    }
    
    // 读取图片
    QImage image = reader.read();
    if (image.isNull()) {
        LOG_WARNING(QString("无法读取图片: %1, 错误: %2").arg(imagePath).arg(reader.errorString()));
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
        labels.first()->setText(QString("图片预览 - %1").arg(displayName));
    }
    
    // 显示对话框并添加淡入动画
    setWindowOpacity(0.0);
    show();
    
    // 显示后再次调用图像居中，确保滚动条初始位置正确
    QTimer::singleShot(50, this, [this]() {
        // 再次调用更新图像显示，确保初始显示时图片居中
        updateImageDisplay();
    });
    
    QPropertyAnimation *animation = new QPropertyAnimation(this, "windowOpacity");
    animation->setDuration(300);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void ImagePreviewDialog::zoomIn()
{
    m_userZoomed = true;  // 标记用户手动缩放
    m_zoomFactor *= 1.2;
    updateImageDisplay();
}

void ImagePreviewDialog::zoomOut()
{
    m_userZoomed = true;  // 标记用户手动缩放
    m_zoomFactor /= 1.2;
    if (m_zoomFactor < 0.1) m_zoomFactor = 0.1;
    updateImageDisplay();
}

void ImagePreviewDialog::resetZoom()
{
    m_userZoomed = false;  // 重置标记，表示使用自动缩放
    m_zoomFactor = 1.0;
    updateImageDisplay();
}

void ImagePreviewDialog::updateImageDisplay()
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

bool ImagePreviewDialog::eventFilter(QObject *watched, QEvent *event)
{
    static QPoint dragPosition;
    
    // 处理标题栏的鼠标事件，实现拖动功能
    if (watched->objectName() == "titleLabel" || watched->parent()->objectName() == "titleLabel") {
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

void ImagePreviewDialog::closeEvent(QCloseEvent *event)
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

QPointer<ImagePreviewDialog> ImagePreviewDialog::m_instance;

void ImagePreviewDialog::closeIfOpen()
{
    if (m_instance && m_instance->isVisible()) {
        LOG_INFO("通过静态方法关闭图片预览对话框");
        m_instance->close();
    }
}

void ImagePreviewDialog::keyPressEvent(QKeyEvent *event)
{
    // 添加键盘快捷键
    if (event->modifiers() & Qt::ControlModifier) {
        switch (event->key()) {
            case Qt::Key_Plus:
            case Qt::Key_Equal:
                zoomIn();
                return;
            case Qt::Key_Minus:
                zoomOut();
                return;
            case Qt::Key_0:
                resetZoom();
                return;
        }
    }
    
    // ESC键关闭对话框
    if (event->key() == Qt::Key_Escape) {
        close(); // 使用close()触发closeEvent
        return;
    }
    
    QDialog::keyPressEvent(event);
}

void ImagePreviewDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    
    // 当窗口大小改变时，自动调整图像大小并保持居中
    if (!m_originalImage.isNull()) {
        // 调整缩放比例以适应新窗口大小
        QSize scrollAreaSize = this->size() - QSize(100, 200);
        double widthRatio = scrollAreaSize.width() / (double)m_originalImage.width();
        double heightRatio = scrollAreaSize.height() / (double)m_originalImage.height();
        double oldZoomFactor = m_zoomFactor;
        double newZoomFactor = qMin(widthRatio, heightRatio);
        
        // 如果是用户手动缩放过的，保持当前缩放比例
        // 但如果窗口变小导致图片太大，则缩小至适应窗口
        if (m_userZoomed) {
            if (m_zoomFactor > newZoomFactor && newZoomFactor < 1.0) {
                m_zoomFactor = newZoomFactor;
            }
        } else {
            // 如果用户未手动缩放，则自动适应窗口大小
            m_zoomFactor = newZoomFactor > 1.0 ? 1.0 : newZoomFactor;
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

// 预览页面实现
PreviewPage::PreviewPage(QWidget *parent)
    : BasePage("图片预览", parent),
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
      m_scrollThreshold(10)
{
    // 初始化内容
    initContent();
    
    // 创建文件系统监视器
    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, &PreviewPage::handleDirectoryChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &PreviewPage::handleFileChanged);
    
    // 创建重新加载定时器
    m_reloadTimer = new QTimer(this);
    m_reloadTimer->setSingleShot(true);
    connect(m_reloadTimer, &QTimer::timeout, this, &PreviewPage::loadImages);
    
    // 创建图片预览对话框
    m_previewDialog = new ImagePreviewDialog(this);

    // 长按计时器
    m_longPressTimer = new QTimer(this);
    m_longPressTimer->setSingleShot(true);
    connect(m_longPressTimer, &QTimer::timeout, this, &PreviewPage::handleLongPress);
    
    // 安装事件过滤器到滚动区域视口
    m_scrollArea->viewport()->installEventFilter(this);
    
    LOG_INFO("预览页面构造完成");
}

PreviewPage::~PreviewPage()
{
    // 清除图片卡片
    clearImageCards();
}

void PreviewPage::setCurrentWorkPath(const QString &path)
{
    if (m_currentWorkPath != path) {
        // 如果之前有监视的路径，先移除
        if (!m_currentWorkPath.isEmpty() && m_fileWatcher->directories().contains(m_currentWorkPath)) {
            m_fileWatcher->removePath(m_currentWorkPath);
        }
        
        // 设置新路径
        m_currentWorkPath = path;
        LOG_INFO(QString("预览页面设置当前工作路径: %1").arg(path));
        
        // 添加新路径到监视器
        if (!m_currentWorkPath.isEmpty()) {
            m_fileWatcher->addPath(m_currentWorkPath);
        }
        
        // 加载图片
        loadImages();
    }
}

void PreviewPage::initContent()
{
    // 设置内容区域的边距，避开状态栏和菜单栏
    m_contentWidget->setContentsMargins(80, STATUS_BAR_HEIGHT, 80, 160);  // 左80px、上边距避开状态栏，右80px，下160px避开菜单栏
    
    // 创建滚动区域
    m_scrollArea = new QScrollArea(m_contentWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 禁用水平滚动条
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet("background-color: #1E1E1E;");
    
    // 设置滚动区域属性
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
    m_gridLayout->setContentsMargins(15, 15, 15, 15);  // 减小边距
    m_gridLayout->setSpacing(m_cardSpacing);
    m_gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);  // 修改为左对齐
    
    // 设置滚动内容
    m_scrollArea->setWidget(m_scrollContent);
    
    // 创建空目录提示标签
    m_emptyLabel = new QLabel(m_scrollArea);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: #AAAAAA; font-size: 36px; background-color: transparent;");  // 放大空目录提示字体
    m_emptyLabel->setText("当前目录没有图片");
    m_emptyLabel->setVisible(false);
    
    // 添加滚动区域到内容布局
    m_contentLayout->addWidget(m_scrollArea);
    
    LOG_INFO("预览页面内容初始化完成");
}

void PreviewPage::showEvent(QShowEvent *event)
{
    BasePage::showEvent(event);

    // 如果当前工作路径为空，尝试从主页获取
    if (m_currentWorkPath.isEmpty()) {
        // 查找主页
        QWidget* mainWindow = this->window();
        if (mainWindow) {
            QList<QWidget*> children = mainWindow->findChildren<QWidget*>("HomePage");
            for (QWidget* child : children) {
                // 尝试调用getCurrentWorkPath方法
                QVariant path = child->property("currentWorkPath");
                if (path.isValid()) {
                    setCurrentWorkPath(path.toString());
                    break;
                }
            }
        }
    }

    // 显示加载提示
    if (m_emptyLabel) {
        m_emptyLabel->setText("正在加载图片...");
        m_emptyLabel->show();
    }

    // 异步加载图片，避免阻塞页面切换
    QTimer::singleShot(0, this, [this]() {
        loadImages();
    });

    LOG_INFO("预览页面显示事件处理完成");
}

void PreviewPage::hideEvent(QHideEvent *event)
{
    BasePage::hideEvent(event);
    
    LOG_INFO("预览页面隐藏事件处理完成");
}

void PreviewPage::resizeEvent(QResizeEvent *event)
{
    BasePage::resizeEvent(event);
    
    // 固定为5列，不再根据窗口大小动态调整
    // 如果布局存在，确保更新
    if (m_scrollArea && m_gridLayout) {
        updateLayout();
    }
    
    // 调整空标签位置
    if (m_emptyLabel && m_emptyLabel->isVisible()) {
        m_emptyLabel->setGeometry(0, 0, m_scrollArea->viewport()->width(), m_scrollArea->viewport()->height());
    }
}

/**
 * @brief 解析图像文件名
 * 解析文件名，提取时间戳部分作为组名和相机类型（左/右）
 * 文件名格式：yyyyMMdd_HHmmss_相机名.jpg
 * 
 * @param filename 文件名
 * @return QPair<QString, QString> 第一个元素是组名（时间戳），第二个元素是相机类型（"left"或"right"）
 */
QPair<QString, QString> PreviewPage::parseImageFilename(const QString &filename)
{
    QString groupName;
    QString cameraType;
    
    // 尝试从文件名中提取时间戳和相机名称
    // 格式：yyyyMMdd_HHmmss_相机名.jpg
    QStringList parts = filename.split("_");
    if (parts.size() >= 3) {
        // 前两部分组成时间戳
        groupName = parts[0] + "_" + parts[1];
        
        // 最后一部分是相机名（可能包含扩展名）
        QString cameraNameWithExt = parts.mid(2).join("_"); // 处理相机名中可能包含的下划线
        
        // 去掉可能的扩展名
        int dotIndex = cameraNameWithExt.lastIndexOf('.');
        QString cameraName = (dotIndex != -1) ? cameraNameWithExt.left(dotIndex) : cameraNameWithExt;
        
        // 识别相机类型
        if (cameraName.contains("左相机", Qt::CaseInsensitive)) {
            cameraType = "left";
        } else if (cameraName.contains("右相机", Qt::CaseInsensitive)) {
            cameraType = "right";
        } else {
            // 默认为未知类型
            cameraType = "unknown";
        }
    } else {
        // 对于不符合规则的文件名，使用文件名本身作为组
        groupName = filename;
        
        // 去掉可能的扩展名
        int dotIndex = groupName.lastIndexOf('.');
        if (dotIndex != -1) {
            groupName = groupName.left(dotIndex);
        }
        
        cameraType = "unknown";
    }
    
    LOG_DEBUG(QString("解析文件名 %1: 组名=%2, 相机类型=%3").arg(filename).arg(groupName).arg(cameraType));
    
    return QPair<QString, QString>(groupName, cameraType);
}

// 替换原有的loadImages()方法
void PreviewPage::loadImages()
{
    // 防止重复加载
    if (m_isLoading) {
        return;
    }
    
    m_isLoading = true;
    LOG_INFO(QString("开始加载图片，路径: %1").arg(m_currentWorkPath));
    
    // 清除现有图片卡片
    clearImageCards();
    
    // 检查路径是否有效
    if (m_currentWorkPath.isEmpty() || !QDir(m_currentWorkPath).exists()) {
        LOG_WARNING(QString("工作路径无效: %1").arg(m_currentWorkPath));
        if (m_emptyLabel) {
            m_emptyLabel->setText("工作路径无效");
            m_emptyLabel->show();
        }
        m_isLoading = false;
        return;
    }
    
    // 获取目录中的图片文件
    QDir dir(m_currentWorkPath);
    QStringList filters;
    filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif";
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::Time); // 按修改时间排序，最新的在前面
    
    QFileInfoList fileList = dir.entryInfoList();
    
    // 如果没有图片，显示空目录提示
    if (fileList.isEmpty()) {
        LOG_INFO(QString("目录中没有图片: %1").arg(m_currentWorkPath));
        m_emptyLabel->setText("当前目录没有图片");
        m_emptyLabel->show();
        m_isLoading = false;
        return;
    }
    
    // 隐藏空目录提示
    m_emptyLabel->hide();
    
    // 按组名（时间戳）归类图像
    QMap<QString, QPair<QString, QString>> imageGroups; // 组名 -> <左相机图像路径, 右相机图像路径>
    
    for (const QFileInfo &fileInfo : fileList) {
        QString filename = fileInfo.fileName();
        QPair<QString, QString> parsedInfo = parseImageFilename(filename);
        QString groupName = parsedInfo.first;
        QString cameraType = parsedInfo.second;
        
        // 检查该组是否已存在
        if (!imageGroups.contains(groupName)) {
            imageGroups.insert(groupName, QPair<QString, QString>("", ""));
        }
        
        // 根据相机类型存储图像路径
        QPair<QString, QString> &paths = imageGroups[groupName];
        if (cameraType == "left") {
            paths.first = fileInfo.filePath();
        } else if (cameraType == "right") {
            paths.second = fileInfo.filePath();
        } else {
            // 对于不符合命名规则的图像，视为独立的组
            paths.first = fileInfo.filePath();
        }
    }
    
    // 为每个组创建图片卡片，只显示左相机的图像
    for (auto it = imageGroups.begin(); it != imageGroups.end(); ++it) {
        QString groupName = it.key();
        QString leftImagePath = it.value().first;
        
        // 如果该组有左相机图像，创建图片卡片
        if (!leftImagePath.isEmpty()) {
            ImageCard *card = createImageCard(leftImagePath);
            if (card) {
                m_imageCards.append(card);
                LOG_DEBUG(QString("创建左相机图片卡片: 组=%1").arg(groupName));
            }
        }
    }
    
    // 更新布局
    updateLayout();

    // 隐藏加载提示
    if (m_emptyLabel) {
        if (m_imageCards.isEmpty()) {
            m_emptyLabel->setText("当前目录没有图片");
            m_emptyLabel->show();
        } else {
            m_emptyLabel->hide();
        }
    }

    LOG_INFO(QString("图片加载完成，共 %1 组，显示 %2 张左相机图片").arg(imageGroups.size()).arg(m_imageCards.size()));
    m_isLoading = false;
}

void PreviewPage::handleDirectoryChanged(const QString &path)
{
    LOG_INFO(QString("目录变化: %1").arg(path));
    
    // 延迟重新加载，避免频繁刷新
    if (m_reloadTimer && !m_reloadTimer->isActive()) {
        m_reloadTimer->start(500);
    }
}

void PreviewPage::handleFileChanged(const QString &path)
{
    LOG_INFO(QString("文件变化: %1").arg(path));
    
    // 延迟重新加载，避免频繁刷新
    if (m_reloadTimer && !m_reloadTimer->isActive()) {
        m_reloadTimer->start(500);
    }
}

void PreviewPage::showImagePreview(const QString &imagePath)
{
    LOG_INFO(QString("显示图片预览: %1").arg(imagePath));
    
    // 设置图片并显示对话框
    if (m_previewDialog) {
        // 在显示对话框前设置其位置，确保不被状态栏、工具栏和菜单栏遮挡
        QScreen *screen = QApplication::primaryScreen();
        QSize screenSize = screen->availableSize();
        
        // 计算可用区域
        int leftOffset = 80; // 避开左侧工具栏
        int topOffset = 80;  // 避开顶部状态栏和工具栏
        int availableWidth = screenSize.width() - leftOffset * 2;
        int availableHeight = screenSize.height() - topOffset - 160; // 底部保留160像素避开菜单栏
        
        // 调整对话框大小
        QSize dialogSize = m_previewDialog->size();
        if (dialogSize.width() > availableWidth) dialogSize.setWidth(availableWidth);
        if (dialogSize.height() > availableHeight) dialogSize.setHeight(availableHeight);
        m_previewDialog->resize(dialogSize);
        
        // 设置对话框位置
        int x = (screenSize.width() - dialogSize.width()) / 2;
        int y = topOffset + (availableHeight - dialogSize.height()) / 2;
        m_previewDialog->move(x, y);
        
        // 设置图片并显示对话框
        m_previewDialog->setImage(imagePath);
        m_previewDialog->exec();
    }
}

ImageCard* PreviewPage::createImageCard(const QString &filePath)
{
    // 创建图片卡片
    ImageCard *card = new ImageCard(filePath, m_scrollContent);
    
    // 连接双击信号
    connect(card, &ImageCard::doubleClicked, this, &PreviewPage::showImagePreview);
    LOG_DEBUG(QString("创建图片卡片并连接双击信号: %1").arg(filePath));
    
    // 确保图片卡片能接收鼠标事件
    card->setMouseTracking(true);
    card->setAttribute(Qt::WA_Hover, true);
    
    // 初始化选中状态
    card->setProperty("selected", false);
    
    return card;
}

void PreviewPage::clearImageCards()
{
    // 从布局中移除所有卡片
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
    
    // 重置选中卡片指针
    m_selectedCard = nullptr;
    m_lastClickedCard = nullptr;
    
    LOG_INFO("清除所有图片卡片");
}

void PreviewPage::updateLayout()
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
    
    // 重新添加卡片到布局，从左到右排列
    for (int i = 0; i < m_imageCards.size(); ++i) {
        int row = i / m_columnsCount;
        int col = i % m_columnsCount;
        m_gridLayout->addWidget(m_imageCards[i], row, col, Qt::AlignLeft);  // 修改为左对齐
        m_imageCards[i]->show();
    }
    
    // 不再额外调整滚动内容的宽度，让其自然适应网格布局
    
    LOG_INFO(QString("布局更新完成，共 %1 张图片，固定 %2 列").arg(m_imageCards.size()).arg(m_columnsCount));
}

void PreviewPage::handleLongPress()
{
    // 识别当前指针下的卡片
    QPoint cursorPos = m_scrollArea->viewport()->mapFromGlobal(QCursor::pos());
    QPoint contentPos = cursorPos + QPoint(m_scrollArea->horizontalScrollBar()->value(), m_scrollArea->verticalScrollBar()->value());
    QWidget *clickedWidget = m_scrollContent->childAt(contentPos);
    ImageCard *imageCard = nullptr;
    while (clickedWidget && !imageCard) {
        imageCard = qobject_cast<ImageCard*>(clickedWidget);
        if (!imageCard) {
            clickedWidget = clickedWidget->parentWidget();
            if (clickedWidget == m_scrollContent) break;
        }
    }
    if (!imageCard) return;
    m_longPressTriggered = true;
    showContextTreeForFile(imageCard->getFilePath());
}

void PreviewPage::showContextTreeForFile(const QString &filePath)
{
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu {"
        "  background-color: #2B2B2B;"
        "  border: 2px solid #666666;"
        "  padding: 18px;"              /* 放大容器内边距 */
        "}"
        "QMenu::item {"
        "  color: #FFFFFF;"
        "  padding: 24px 48px;"        /* 放大条目高度与左右留白 */
        "  font-size: 36px;"           /* 放大字体约3倍 */
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
            QString("确定要删除该文件吗？\n%1").arg(filePath),
            "删除",
            "取消"
        );
        if (reply == QMessageBox::Yes) {
            if (QFile::remove(filePath)) {
                showToast(this, "文件已删除", 1500);
                loadImages();
            } else {
                showToast(this, "删除失败", 2000, ToastNotification::BottomCenter, ToastNotification::Error);
            }
        }
    }
}

// 修改事件过滤器，加入长按启动/取消
bool PreviewPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_scrollArea->viewport()) {
        switch (event->type()) {
            case QEvent::MouseButtonPress: {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    m_isScrolling = true;
                    m_lastMousePos = mouseEvent->pos();
                    m_pressPos = mouseEvent->pos();
                    m_scrollArea->viewport()->setCursor(Qt::ClosedHandCursor);

                    // 启动长按计时器（600ms）
                    m_longPressTriggered = false;
                    m_longPressTimer->start(600);

                    m_lastClickTime = QDateTime::currentMSecsSinceEpoch();
                    LOG_DEBUG("鼠标按下事件，开始滚动模式");
                }
                break;
            }
            case QEvent::MouseMove: {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                if (m_isScrolling) {
                    int deltaY = m_lastMousePos.y() - mouseEvent->pos().y();
                    QScrollBar *vScrollBar = m_scrollArea->verticalScrollBar();
                    if (vScrollBar && qAbs(deltaY) > 2) {
                        vScrollBar->setValue(vScrollBar->value() + deltaY);
                        // 移动则取消长按
                        if (m_longPressTimer->isActive()) m_longPressTimer->stop();
                        LOG_DEBUG(QString("滚动视图，偏移量: %1").arg(deltaY));
                    }
                    m_lastMousePos = mouseEvent->pos();
                }
                break;
            }
            case QEvent::MouseButtonRelease: {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton && m_isScrolling) {
                    if (m_longPressTimer->isActive()) m_longPressTimer->stop();
                    if (m_longPressTriggered) {
                        // 已触发长按，吞掉本次点击后续逻辑
                        m_isScrolling = false;
                        m_scrollArea->viewport()->setCursor(Qt::ArrowCursor);
                        return true;
                    }
                }
                break;
            }
            
            case QEvent::TouchBegin:
            case QEvent::TouchUpdate:
            case QEvent::TouchEnd: {
                QTouchEvent *touchEvent = static_cast<QTouchEvent*>(event);
                const QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
                
                if (touchPoints.count() == 1) {
                    const QTouchEvent::TouchPoint &touchPoint = touchPoints.first();
                    
                    if (touchEvent->type() == QEvent::TouchBegin) {
                        m_isScrolling = true;
                        m_lastMousePos = touchPoint.pos().toPoint();
                        m_pressPos = touchPoint.pos().toPoint();
                        LOG_DEBUG("触摸开始事件，开始滚动模式");
                    } 
                    else if (touchEvent->type() == QEvent::TouchUpdate && m_isScrolling) {
                        // 处理滚动手势
                        int deltaY = m_lastMousePos.y() - touchPoint.pos().toPoint().y();
                        QScrollBar *vScrollBar = m_scrollArea->verticalScrollBar();
                        if (vScrollBar && qAbs(deltaY) > 2) {
                            vScrollBar->setValue(vScrollBar->value() + deltaY);
                            LOG_DEBUG(QString("触摸滚动视图，偏移量: %1").arg(deltaY));
                        }
                        m_lastMousePos = touchPoint.pos().toPoint();
                    }
                    else if (touchEvent->type() == QEvent::TouchEnd) {
                        m_isScrolling = false;
                        LOG_DEBUG("触摸结束事件，结束滚动模式");
                        
                        // 计算从按下到释放的移动距离
                        QPoint moveDelta = m_pressPos - touchPoint.pos().toPoint();
                        
                        // 如果移动距离很小，可能是点击而非拖动
                        if (moveDelta.manhattanLength() < 10) {
                            // 查找点击位置下的图片卡片
                            QPoint viewportPos = touchPoint.pos().toPoint();
                            QPoint contentPos = viewportPos + QPoint(m_scrollArea->horizontalScrollBar()->value(),
                                                                   m_scrollArea->verticalScrollBar()->value());
                            
                            QWidget *clickedWidget = m_scrollContent->childAt(contentPos);
                            ImageCard *imageCard = nullptr;
                            
                            // 尝试查找点击的图片卡片
                            while (clickedWidget && !imageCard) {
                                imageCard = qobject_cast<ImageCard*>(clickedWidget);
                                if (!imageCard) {
                                    clickedWidget = clickedWidget->parentWidget();
                                    if (clickedWidget == m_scrollContent) break;
                                }
                            }
                            
                            if (imageCard) {
                                // 先清除之前选择的卡片的焦点
                                if (m_selectedCard) {
                                    m_selectedCard->setFocus(Qt::NoFocusReason);
                                    m_selectedCard->setProperty("selected", false);
                                    m_selectedCard->update(); // 强制重绘
                                }
                                
                                // 设置当前卡片为选中状态
                                imageCard->setFocus(Qt::MouseFocusReason);
                                imageCard->setProperty("selected", true);
                                imageCard->update(); // 强制重绘
                                
                                // 更新选中的卡片
                                m_selectedCard = imageCard;
                                LOG_DEBUG(QString("触摸选中图片: %1").arg(imageCard->getFilePath()));
                                
                                // 检查是否是双击
                                qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
                                if (m_lastClickedCard == imageCard && 
                                    currentTime - m_lastClickTime < 500) {
                                    LOG_INFO(QString("双击图片: %1").arg(imageCard->getFilePath()));
                                    showImagePreview(imageCard->getFilePath());
                                }
                                
                                // 记录最后点击的卡片和时间
                                m_lastClickedCard = imageCard;
                                m_lastClickTime = currentTime;
                            } else {
                                // 点击了空白区域，清除选中状态
                                if (m_selectedCard) {
                                    m_selectedCard->setFocus(Qt::NoFocusReason);
                                    m_selectedCard->setProperty("selected", false);
                                    m_selectedCard->update(); // 强制重绘
                                    m_selectedCard = nullptr;
                                }
                            }
                        }
                    }
                    
                    return true; // 阻止事件进一步传播
                }
                break;
            }
            
            default:
                break;
        }
    }
    
    // 其他事件传递给父类处理
    return BasePage::eventFilter(watched, event);
} 