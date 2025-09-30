#include "app/ui/navigation_bar.h"
#include <QDebug>
#include <QStyle>
#include "infrastructure/logging/logger.h"
#include <QMessageBox>
#include <QTimer>
#include <QApplication>
#include "app/ui/utils/dialog_utils.h"
#include "app/ui/measurement_page.h"
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>

NavigationBar::NavigationBar(QWidget *parent)
    : QWidget(parent),
      m_layout(nullptr),
      m_pageManager(nullptr)
{
    // 设置独立组件所需的属性
    setObjectName("navigationBar");
    
    // 确保导航栏可以接收鼠标事件
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setMouseTracking(true);
    
    // 设置固定高度
    setFixedHeight(120);
    
    // 设置透明背景
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    
    // 确保导航栏在最上层
    setStyleSheet(
        "QWidget#navigationBar {"
        "   background-color: rgba(30, 30, 30, 220);"
        "   border-radius: 40px;"
        "   border: 1px solid #444444;"
        "}"
    );
    
    setupUi();
    
    // 确保初始大小适合内容
    adjustSize();
    
    LOG_INFO("导航栏构造完成");
}

NavigationBar::~NavigationBar()
{
}

void NavigationBar::setPageManager(PageManager *pageManager)
{
    m_pageManager = pageManager;
    
    if (m_pageManager) {
        // 连接页面变化信号
        connect(m_pageManager, &PageManager::pageChanged, 
                this, &NavigationBar::onPageChanged);
        
        // 初始化按钮状态
        updateButtonStates();
    }
}

void NavigationBar::setupUi()
{
    // 创建水平布局
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(20, 0, 20, 0);
    m_layout->setSpacing(15);  // 增加按钮间距
    m_layout->setAlignment(Qt::AlignCenter);
    
    // 创建半透明背景面板
    QWidget *backgroundPanel = new QWidget(this);
    backgroundPanel->setObjectName("navBackgroundPanel");
    backgroundPanel->setStyleSheet("background-color: rgba(30, 30, 30, 150); border-radius: 40px; border: 1px solid rgba(80, 80, 80, 200);");
    backgroundPanel->setAutoFillBackground(false);
    backgroundPanel->setAttribute(Qt::WA_TranslucentBackground, true);
    backgroundPanel->setFixedHeight(110);  // 略小于导航栏高度
    
    // 将背景面板添加到布局中
    m_layout->addWidget(backgroundPanel);
    
    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout(backgroundPanel);
    buttonLayout->setContentsMargins(20, 0, 20, 0);
    buttonLayout->setSpacing(15);
    buttonLayout->setAlignment(Qt::AlignCenter);
    
    // 创建主页按钮 - 不显示文本
    NavigationButton *homeButton = createNavButton(":/icons/home.svg", "", PageType::Home);
    buttonLayout->addWidget(homeButton);
    m_navButtons[PageType::Home] = homeButton;
    
    // 创建返回按钮 - 不显示文本
    NavigationButton *backButton = createNavButton(":/icons/back.svg", "", PageType::Back);
    backButton->setVisible(false); // 初始化时隐藏返回按钮
    buttonLayout->addWidget(backButton);
    m_navButtons[PageType::Back] = backButton;
    
    // 创建预览按钮
    NavigationButton *previewButton = createNavButton(":/icons/preview.svg", "预览", PageType::Preview);
    buttonLayout->addWidget(previewButton);
    m_navButtons[PageType::Preview] = previewButton;
    
    // 创建报告按钮
    NavigationButton *reportButton = createNavButton(":/icons/report.svg", "报告", PageType::Report);
    buttonLayout->addWidget(reportButton);
    m_navButtons[PageType::Report] = reportButton;
    
    // 注释按钮已移除
    
    // 创建参数设置按钮
    NavigationButton *settingsButton = createNavButton(":/icons/setting.svg", "设置", PageType::Settings);
    buttonLayout->addWidget(settingsButton);
    m_navButtons[PageType::Settings] = settingsButton;
    
    // 创建3D测量按钮
    NavigationButton *measurementButton = createNavButton(":/icons/3D.svg", "3D测量", PageType::Measurement);
    buttonLayout->addWidget(measurementButton);
    m_navButtons[PageType::Measurement] = measurementButton;
    
    // 创建退出按钮
    NavigationButton *exitButton = createNavButton(":/icons/close.svg", "退出", PageType::Exit);
    buttonLayout->addWidget(exitButton);
    m_navButtons[PageType::Exit] = exitButton;
    
    // 设置默认状态
    homeButton->setActive(true);
    
    LOG_INFO("导航栏UI设置完成");
}

NavigationButton* NavigationBar::createNavButton(const QString &iconPath, const QString &text, PageType pageType)
{
    NavigationButton *button = new NavigationButton(iconPath, text, pageType, this);
    
    // 连接点击信号
    connect(button, &NavigationButton::clicked, this, &NavigationBar::onNavigationButtonClicked);
    
    return button;
}

void NavigationBar::onNavigationButtonClicked()
{
    if (!m_pageManager) {
        LOG_ERROR("错误: 页面管理器未设置");
        return;
    }
    
    // 获取发送信号的按钮
    NavigationButton *button = qobject_cast<NavigationButton*>(sender());
    if (!button) {
        return;
    }
    
    // 获取页面类型
    PageType pageType = button->getPageType();
    
    // 处理退出按钮点击
    if (pageType == PageType::Exit) {
        // 使用统一的对话框工具类显示确认对话框
        QMessageBox::StandardButton result = SmartScope::App::Ui::DialogUtils::showStyledConfirmationDialog(
            this,               // 父窗口
            "确认退出",          // 标题
            "确定要退出程序吗？", // 提示文本
            "确定",             // 确认按钮文本
            "取消"              // 取消按钮文本
        );

        // 如果用户确认退出
        if (result == QMessageBox::Yes) {
            LOG_INFO("用户确认退出程序");
            // 获取主窗口并设置退出确认标志，避免closeEvent中重复确认
            QWidget* mainWindow = this->window();
            if (mainWindow) {
                // 设置属性标记已经确认退出，避免closeEvent重复询问
                mainWindow->setProperty("exitConfirmed", true);
                LOG_INFO("关闭主窗口");
                mainWindow->close();
            } else {
                LOG_WARNING("无法获取主窗口指针，使用QApplication::quit()退出");
                QApplication::quit();
            }
        } else {
            LOG_INFO("用户取消退出程序");
        }

        return;
    }
    
    // 处理返回按钮点击
    if (pageType == PageType::Back) {
        LOG_INFO("用户点击返回按钮");
        
        // 获取当前页面类型
        PageType currentPageType = m_pageManager->getCurrentPageType();
        
        // 根据当前页面决定要返回的页面
        PageType targetPageType = PageType::Home;  // 默认返回主页
        
        // 从不同页面返回的逻辑
        switch (currentPageType) {
            case PageType::Preview:
            case PageType::Report:
            case PageType::Settings:
            case PageType::Measurement:
                targetPageType = PageType::Home;  // 从这些页面返回到主页
                break;
            default:
                // 其他页面默认返回主页
                targetPageType = PageType::Home;
                break;
        }
        
        // 在3D测量页面点击Back时，弹出确认对话框，复用测量页逻辑
        if (currentPageType == PageType::Measurement) {
            if (auto mp = m_pageManager->getMeasurementPage()) {
                // 直接在测量页内部处理：有测量数据则确认清空，无则确认返回主页
                mp->invokeBackConfirmationFromNav();
                return;
            }
        }
        
        // 切换到目标页面
        m_pageManager->switchToPage(targetPageType);
        return;
    }
    
    // Home 按钮在 3D测量页需确认
    if (pageType == PageType::Home) {
        PageType currentPageType = m_pageManager->getCurrentPageType();
        if (currentPageType == PageType::Measurement) {
            if (auto mp = m_pageManager->getMeasurementPage()) {
                mp->invokeBackConfirmationFromNav();
                return;
            }
        }
    }

    // 直接切换页面，不再预先检查条件
    // 页面管理器内部会处理异步加载逻辑
    m_pageManager->switchToPage(pageType);
}

void NavigationBar::onPageChanged(PageType pageType)
{
    // 更新按钮状态
    for (auto it = m_navButtons.begin(); it != m_navButtons.end(); ++it) {
        if (it.key() == pageType) {
            it.value()->setActive(true);
        } else {
            it.value()->setActive(false);
        }
    }

    // 处理返回按钮的显示/隐藏
    if (auto backButton = m_navButtons.value(PageType::Back)) {
        // 只在3D测量页面显示返回按钮
        backButton->setVisible(pageType == PageType::Measurement);
    }

    LOG_INFO(QString("页面切换到: %1").arg(static_cast<int>(pageType)));
}

void NavigationBar::updateButtonStates()
{
    if (!m_pageManager) {
        return;
    }
    
    // 获取当前页面类型
    PageType currentPageType = m_pageManager->getCurrentPageType();
    
    // 更新所有按钮状态
    for (auto it = m_navButtons.begin(); it != m_navButtons.end(); ++it) {
        PageType buttonPageType = it.key();
        NavigationButton *button = it.value();
        
        bool isActive = (buttonPageType == currentPageType);
        button->setActive(isActive);
    }
}

int NavigationBar::calculateOptimalWidth() const
{
    // 计算所有导航按钮的总宽度
    int totalButtonWidth = 0;
    int buttonSpacing = 15; // 按钮间距
    int buttonCount = 0;
    
    for (auto it = m_navButtons.begin(); it != m_navButtons.end(); ++it) {
        if (it.value() && it.value()->isVisible()) {
            totalButtonWidth += it.value()->width();
            buttonCount++;
        }
    }
    
    // 计算总宽度 = 按钮总宽度 + (按钮数-1)*间距 + 左右边距
    int optimalWidth = totalButtonWidth;
    if (buttonCount > 1) {
        optimalWidth += (buttonCount - 1) * buttonSpacing;
    }
    
    // 添加左右边距和一些额外空间
    optimalWidth += 40 + 40; // 左右各20像素边距 + 一些额外空间
    
    LOG_INFO(QString("计算导航栏最佳宽度: %1").arg(optimalWidth));
    
    return optimalWidth;
}

void NavigationBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    
    // 当导航栏显示时，调整大小并更新位置
    adjustSizeToContent();
    
    // 确保导航栏可见
    raise();
    
    LOG_INFO("导航栏显示事件处理");
}

void NavigationBar::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    
    LOG_INFO("导航栏隐藏事件处理");
}

void NavigationBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // 导航栏大小变化时，可能需要调整内部布局
    LOG_INFO(QString("导航栏大小变化: %1x%2").arg(width()).arg(height()));
}

void NavigationBar::adjustSizeToContent()
{
    // 根据按钮数量和大小调整导航栏宽度
    int optimalWidth = calculateOptimalWidth();

    // 设置合适的宽度
    setFixedWidth(optimalWidth);

    LOG_INFO(QString("调整导航栏大小: %1x%2").arg(width()).arg(height()));

    // 发射大小改变信号，通知主窗口更新位置
    emit sizeChanged();
}

void NavigationBar::updateMeasurementButtonVisibility(bool isSingleCameraMode)
{
    // 获取3D测量按钮
    NavigationButton* measurementButton = m_navButtons.value(PageType::Measurement);
    if (measurementButton) {
        if (isSingleCameraMode) {
            // 单相机模式下隐藏3D测量按钮
            measurementButton->setVisible(false);
            measurementButton->hide();
            LOG_INFO("单相机模式：隐藏3D测量按钮");
        } else {
            // 双相机模式下显示3D测量按钮
            measurementButton->setVisible(true);
            measurementButton->show();
            LOG_INFO("双相机模式：显示3D测量按钮");
        }

        // 重新调整导航栏大小以适应按钮变化
        adjustSizeToContent();
    } else {
        LOG_WARNING("无法找到3D测量按钮，无法更新可见性");
    }
}