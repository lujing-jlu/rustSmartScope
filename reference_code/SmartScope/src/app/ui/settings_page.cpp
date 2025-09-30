#include "app/ui/settings_page.h"
#include "infrastructure/logging/logger.h"
#include "infrastructure/config/config_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QSpacerItem>

using namespace SmartScope::Infrastructure;

SettingsPage::SettingsPage(QWidget *parent)
    : BasePage("参数设置", parent)
{
    LOG_INFO("创建参数设置页面");
    
    // 初始化内容
    initContent();
}

SettingsPage::~SettingsPage()
{
    LOG_INFO("销毁参数设置页面");
}

void SettingsPage::initContent()
{
    LOG_INFO("初始化参数设置页面内容");

    // 获取BasePage的内容布局
    QVBoxLayout* contentLayout = getContentLayout();
    if (!contentLayout) {
        LOG_ERROR("无法获取内容布局");
        return;
    }

    // 设置内容区域的布局参数，顶部预留状态栏空间（80px + 额外间距）
    contentLayout->setContentsMargins(60, 140, 60, 60);  // 增大边距，适配7寸屏
    contentLayout->setSpacing(40);  // 增大间距
    
    // 创建设置组容器
    QWidget* settingsGroup = new QWidget(getContentWidget());
    settingsGroup->setObjectName("settingsGroup");
    settingsGroup->setStyleSheet(
        "QWidget#settingsGroup {"
        "    background-color: #2D2D2D;"
        "    border-radius: 20px;"  // 增大圆角
        "    padding: 40px;"        // 增大内边距
        "}"
        "QLabel {"
        "    color: #E0E0E0;"
        "    font-size: 36px;"       // 从24px增大到36px，适配7寸屏
        "    font-weight: bold;"     // 加粗字体
        "}"
        "QCheckBox {"
        "    color: #E0E0E0;"
        "    font-size: 32px;"       // 从24px增大到32px
        "    spacing: 25px;"         // 增大文字与复选框的间距
        "    padding: 10px 0px;"     // 增加上下内边距
        "}"
        "QCheckBox::indicator {"
        "    width: 45px;"           // 从30px增大到45px
        "    height: 45px;"          // 从30px增大到45px
        "    border-radius: 6px;"    // 增大圆角
        "    border: 3px solid #555555;"  // 增大边框
        "}"
        "QCheckBox::indicator:checked {"
        "    background-color: #4CAF50;"
        "    border: 3px solid #4CAF50;"  // 增大边框
        "    image: url(:/icons/check.svg);"
        "}"
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 12px;"   // 增大圆角
        "    padding: 20px 40px;"    // 增大内边距
        "    font-size: 32px;"       // 从24px增大到32px
        "    font-weight: bold;"     // 加粗字体
        "    min-width: 200px;"      // 增大最小宽度
        "    min-height: 80px;"      // 增加最小高度
        "}"
        "QPushButton:hover {"
        "    background-color: #45A049;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #3D8B40;"
        "}"
    );
    
    // 创建设置组布局
    QVBoxLayout* groupLayout = new QVBoxLayout(settingsGroup);
    groupLayout->setSpacing(40);  // 增大组件间距，适配7寸屏
    
    // 创建显示设置标题
    QLabel* displaySettingsLabel = new QLabel("显示设置", settingsGroup);
    // 移除额外的样式设置，使用主样式表中的设置
    groupLayout->addWidget(displaySettingsLabel);
    
    // 创建帧率显示开关
    m_showFpsCheckBox = new QCheckBox("显示相机帧率", settingsGroup);
    bool showFps = ConfigManager::instance().getValue("ui/show_fps", false).toBool();
    m_showFpsCheckBox->setChecked(showFps);
    connect(m_showFpsCheckBox, &QCheckBox::toggled, this, &SettingsPage::onShowFpsToggled);
    groupLayout->addWidget(m_showFpsCheckBox);
    
    // 创建调试模式开关
    m_debugModeCheckBox = new QCheckBox("启用调试模式（开放3D测量调试入口）", settingsGroup);
    bool debugMode = ConfigManager::instance().getValue("ui/debug_mode", false).toBool();
    m_debugModeCheckBox->setChecked(debugMode);
    connect(m_debugModeCheckBox, &QCheckBox::toggled, this, &SettingsPage::onDebugModeToggled);
    groupLayout->addWidget(m_debugModeCheckBox);
    
    // 添加垂直弹簧
    groupLayout->addStretch();
    
    // 添加设置组到内容布局
    contentLayout->addWidget(settingsGroup);
    
    // 添加垂直弹簧
    contentLayout->addStretch();
    
    LOG_INFO("参数设置页面内容初始化完成");
}

void SettingsPage::onShowFpsToggled(bool checked)
{
    LOG_INFO(QString("帧率显示设置已更改: %1").arg(checked ? "显示" : "隐藏"));
    
    // 保存设置
    ConfigManager::instance().setValue("ui/show_fps", checked);
    ConfigManager::instance().saveConfig();
    
    // 发送设置变更信号
    emit showFpsSettingChanged(checked);
}

void SettingsPage::onDebugModeToggled(bool checked)
{
    LOG_INFO(QString("调试模式设置已更改: %1").arg(checked ? "启用" : "禁用"));
    ConfigManager::instance().setValue("ui/debug_mode", checked);
    ConfigManager::instance().saveConfig();
    emit debugModeSettingChanged(checked);
} 