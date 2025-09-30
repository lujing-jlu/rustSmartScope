#include "app/ui/navigation_button.h"
#include <QStyle>
#include <QDebug>
#include "infrastructure/logging/logger.h"

NavigationButton::NavigationButton(const QString &iconPath, const QString &text, PageType pageType, QWidget *parent)
    : QPushButton(parent),
      m_pageType(pageType)
{
    // 设置图标
    QIcon icon(iconPath);
    setIcon(icon);
    setIconSize(QSize(40, 40)); 

    // 设置文本
    setText(text);

    // 初始化按钮
    initialize();
}

NavigationButton::~NavigationButton()
{
}

PageType NavigationButton::getPageType() const
{
    return m_pageType;
}

void NavigationButton::setActive(bool active)
{
    // 设置激活状态属性
    setProperty("active", active);
    
    // 强制更新样式
    style()->unpolish(this);
    style()->polish(this);
}

void NavigationButton::initialize()
{
    // 设置按钮尺寸
    if (text().isEmpty() || m_pageType == PageType::Back) {
        // 主页按钮和返回按钮 - 设置为正方形
        setFixedSize(110, 110);
    } else {
        // 其他按钮 - 保持原来的长方形尺寸
        setFixedSize(250, 110);
    }
    
    // 设置字体大小
    QFont font = this->font();
    font.setPointSize(30);  // 将字体大小从12增大到30 (12 * 2.5 = 30)
    setFont(font);
    
    // 设置属性
    setProperty("pageType", static_cast<int>(m_pageType));
    setProperty("active", false);
    
    // 设置样式
    setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(30, 30, 30, 150);"  // 未选中状态为深色半透明
        "  border: none;"
        "  border-radius: 12px;"
        "  color: #FFFFFF;"  // 改为白色，更适合透明背景
        "  padding: 12px 20px;"
        "  text-align: center;"
        "  font-size: 40px;"  // 将字体大小从16px增大到40px (16 * 2.5 = 40)
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(80, 80, 80, 180);"  // 半透明悬停效果
        "}"
        "QPushButton[active=\"true\"] {"
        "  background-color: rgba(100, 100, 100, 220);"  // 半透明激活效果
        "  color: #FFFFFF;"
        "  font-weight: bold;"  // 激活状态加粗
        "}"
    );
    
    LOG_DEBUG(QString("导航按钮初始化完成: %1").arg(text().isEmpty() ? 
              (m_pageType == PageType::Back ? "返回" : "主页") : text()));
} 