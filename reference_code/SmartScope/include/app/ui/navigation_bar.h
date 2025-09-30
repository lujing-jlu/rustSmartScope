#ifndef NAVIGATION_BAR_H
#define NAVIGATION_BAR_H

#include <QWidget>
#include <QHBoxLayout>
#include <QMap>
#include <QShowEvent>
#include <QHideEvent>
#include <QResizeEvent>
#include "app/ui/page_manager.h"
#include "app/ui/navigation_button.h"

/**
 * @brief 导航栏组件
 * 
 * 用于应用程序底部的导航栏，包含多个导航按钮
 */
class NavigationBar : public QWidget
{
    Q_OBJECT

signals:
    /**
     * @brief 导航栏大小改变信号
     */
    void sizeChanged();

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit NavigationBar(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~NavigationBar();
    
    /**
     * @brief 设置页面管理器
     * @param pageManager 页面管理器指针
     */
    void setPageManager(PageManager *pageManager);
    
    /**
     * @brief 计算导航栏的推荐宽度
     * @return 推荐宽度
     */
    int calculateOptimalWidth() const;

    /**
     * @brief 根据相机模式更新3D测量按钮可见性
     * @param isSingleCameraMode 是否为单相机模式
     */
    void updateMeasurementButtonVisibility(bool isSingleCameraMode);

protected:
    /**
     * @brief 显示事件处理
     * @param event 显示事件
     */
    void showEvent(QShowEvent *event) override;
    
    /**
     * @brief 隐藏事件处理
     * @param event 隐藏事件
     */
    void hideEvent(QHideEvent *event) override;
    
    /**
     * @brief 大小变化事件处理
     * @param event 大小变化事件
     */
    void resizeEvent(QResizeEvent *event) override;

private slots:
    /**
     * @brief 导航按钮点击事件处理
     */
    void onNavigationButtonClicked();
    
    /**
     * @brief 页面变化事件处理
     * @param pageType 新的页面类型
     */
    void onPageChanged(PageType pageType);

private:
    /**
     * @brief 设置UI
     */
    void setupUi();
    
    /**
     * @brief 创建导航按钮
     * @param iconPath 图标路径
     * @param text 按钮文本
     * @param pageType 页面类型
     * @return 导航按钮指针
     */
    NavigationButton* createNavButton(const QString &iconPath, const QString &text, PageType pageType);
    
    /**
     * @brief 更新按钮状态
     */
    void updateButtonStates();
    
    /**
     * @brief 调整导航栏大小以适应内容
     */
    void adjustSizeToContent();
    
    QHBoxLayout *m_layout;                      ///< 水平布局
    PageManager *m_pageManager;                 ///< 页面管理器
    QMap<PageType, NavigationButton*> m_navButtons;  ///< 导航按钮映射
};

#endif // NAVIGATION_BAR_H 