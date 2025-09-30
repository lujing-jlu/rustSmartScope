#ifndef NAVIGATION_BUTTON_H
#define NAVIGATION_BUTTON_H

#include <QPushButton>
#include <QIcon>
#include <QString>
#include "app/ui/page_manager.h"

/**
 * @brief 导航按钮组件
 * 
 * 用于导航栏的按钮组件，支持图标、文本和页面类型
 */
class NavigationButton : public QPushButton
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param iconPath 图标路径
     * @param text 按钮文本
     * @param pageType 对应的页面类型
     * @param parent 父组件
     */
    explicit NavigationButton(const QString &iconPath, const QString &text, PageType pageType, QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~NavigationButton();
    
    /**
     * @brief 获取按钮对应的页面类型
     * @return 页面类型
     */
    PageType getPageType() const;
    
    /**
     * @brief 设置按钮是否激活
     * @param active 是否激活
     */
    void setActive(bool active);

private:
    /**
     * @brief 初始化按钮样式和属性
     */
    void initialize();
    
    PageType m_pageType;  ///< 对应的页面类型
};

#endif // NAVIGATION_BUTTON_H 