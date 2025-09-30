#include "app/ui/preview_selection_page.h"
#include "infrastructure/logging/logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpacerItem>
#include <QFont>
#include <QPainter>
#include <QPixmap>

PreviewSelectionPage::PreviewSelectionPage(QWidget *parent)
    : BasePage("预览选择", parent),
      m_titleLabel(nullptr),
      m_photoButton(nullptr),
      m_screenshotButton(nullptr)
{
    initContent();
    LOG_INFO("预览选择页面构造完成");
}

PreviewSelectionPage::~PreviewSelectionPage()
{
    LOG_INFO("预览选择页面析构");
}

void PreviewSelectionPage::initContent()
{
    // 获取BasePage提供的内容区域
    QWidget *contentWidget = getContentWidget();
    QVBoxLayout *contentLayout = getContentLayout();

    // 清除现有内容
    QLayoutItem *item;
    while ((item = contentLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    // 创建居中容器
    QWidget *centerWidget = new QWidget(contentWidget);
    centerWidget->setStyleSheet("background-color: transparent;");

    // 创建居中容器的布局
    QVBoxLayout *centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(50, 50, 50, 50);
    centerLayout->setSpacing(40);
    centerLayout->setAlignment(Qt::AlignCenter);

    // 创建标题
    m_titleLabel = new QLabel("选择预览类型", centerWidget);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 36px;"
        "   font-weight: bold;"
        "   margin-bottom: 30px;"
        "}"
    );
    centerLayout->addWidget(m_titleLabel, 0, Qt::AlignCenter);

    // 创建按钮容器
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(80);
    buttonLayout->setAlignment(Qt::AlignCenter);

    // 创建拍照预览按钮
    m_photoButton = createSelectionButton(":/icons/camera.svg", "拍照预览", "查看相机拍摄的照片");
    connect(m_photoButton, &QPushButton::clicked, this, &PreviewSelectionPage::onPhotoPreviewClicked);
    buttonLayout->addWidget(m_photoButton);

    // 创建截屏预览按钮
    m_screenshotButton = createSelectionButton(":/icons/screenshot.svg", "截屏预览", "查看屏幕截图文件");
    connect(m_screenshotButton, &QPushButton::clicked, this, &PreviewSelectionPage::onScreenshotPreviewClicked);
    buttonLayout->addWidget(m_screenshotButton);

    // 创建视频预览按钮（使用录制图标）
    m_videoButton = createSelectionButton(":/icons/record_start.svg", "视频预览", "查看录制的视频文件");
    connect(m_videoButton, &QPushButton::clicked, this, &PreviewSelectionPage::onVideoPreviewClicked);
    buttonLayout->addWidget(m_videoButton);

    centerLayout->addLayout(buttonLayout);

    // 将居中容器添加到内容布局，并设置为居中
    contentLayout->addWidget(centerWidget, 1, Qt::AlignCenter);

    LOG_INFO("预览选择页面内容初始化完成");
}

QPushButton* PreviewSelectionPage::createSelectionButton(const QString &iconPath, const QString &title, const QString &description)
{
    QPushButton *button = new QPushButton(this);
    button->setFixedSize(350, 250);
    button->setCursor(Qt::PointingHandCursor);

    QVBoxLayout *layout = new QVBoxLayout(button);
    layout->setContentsMargins(25, 25, 25, 25);
    layout->setSpacing(20);
    layout->setAlignment(Qt::AlignCenter);

    QLabel *iconLabel = new QLabel(button);
    iconLabel->setFixedSize(100, 100);
    iconLabel->setAlignment(Qt::AlignCenter);

    QPixmap iconPixmap(iconPath);
    if (!iconPixmap.isNull()) {
        QPixmap scaledIcon = iconPixmap.scaled(90, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap whiteIcon(scaledIcon.size());
        whiteIcon.fill(Qt::transparent);
        QPainter painter(&whiteIcon);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawPixmap(0, 0, scaledIcon);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(whiteIcon.rect(), QColor(255, 255, 255, 255));
        painter.end();
        iconLabel->setPixmap(whiteIcon);
    }

    layout->addWidget(iconLabel, 0, Qt::AlignCenter);

    QLabel *titleLabel = new QLabel(title, button);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 30px;"
        "   font-weight: bold;"
        "   background: transparent;"
        "   border: none;"
        "   margin: 10px 0px;"
        "}"
    );
    layout->addWidget(titleLabel, 0, Qt::AlignCenter);

    QLabel *descLabel = new QLabel(description, button);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(
        "QLabel {"
        "   color: rgba(255, 255, 255, 180);"
        "   font-size: 20px;"
        "   background: transparent;"
        "   border: none;"
        "   line-height: 1.4;"
        "}"
    );
    layout->addWidget(descLabel, 0, Qt::AlignCenter);

    button->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(50, 50, 50, 180);"
        "   border: 3px solid rgba(80, 80, 80, 200);"
        "   border-radius: 25px;"
        "   color: white;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(70, 70, 70, 220);"
        "   border: 3px solid rgba(120, 120, 120, 255);"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(90, 90, 90, 250);"
        "   border: 3px solid rgba(140, 140, 140, 255);"
        "}"
    );

    return button;
}

void PreviewSelectionPage::onPhotoPreviewClicked()
{
    LOG_INFO("用户选择拍照预览");
    emit photoPreviewSelected();
}

void PreviewSelectionPage::onScreenshotPreviewClicked()
{
    LOG_INFO("用户选择截屏预览");
    emit screenshotPreviewSelected();
}

void PreviewSelectionPage::onVideoPreviewClicked()
{
    LOG_INFO("用户选择视频预览");
    emit videoPreviewSelected();
}
