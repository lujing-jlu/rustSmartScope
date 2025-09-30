#ifndef BASE_PAGE_H
#define BASE_PAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

/**
 * @brief 基础页面组件
 * 
 * 所有页面的基类，提供统一的布局和样式
 */
class BasePage : public QWidget
{
    Q_OBJECT

public:
    // 状态栏高度常量，用于避免与状态栏重叠
    static constexpr int STATUS_BAR_HEIGHT = 80;
    /**
     * @brief 构造函数
     * @param title 页面标题
     * @param parent 父组件
     */
    explicit BasePage(const QString &title, QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    virtual ~BasePage();
    
    /**
     * @brief 设置页面标题
     * @param title 页面标题
     */
    void setPageTitle(const QString &title);
    
    /**
     * @brief 获取内容区域组件
     * @return 内容区域组件
     */
    QWidget* getContentWidget() const;
    
    /**
     * @brief 获取内容区域布局
     * @return 内容区域布局
     */
    QVBoxLayout* getContentLayout() const;

protected:
    /**
     * @brief 初始化UI
     */
    virtual void initUI();
    
    /**
     * @brief 初始化内容区域
     * 子类应该重写此方法来添加自己的内容
     */
    virtual void initContent() = 0;
    
    QVBoxLayout *m_mainLayout;    ///< 主布局
    QLabel *m_titleLabel;         ///< 标题标签
    QWidget *m_contentWidget;     ///< 内容区域组件
    QVBoxLayout *m_contentLayout; ///< 内容区域布局
};

#endif // BASE_PAGE_H 