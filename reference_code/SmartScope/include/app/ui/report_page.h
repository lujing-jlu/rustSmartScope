#ifndef REPORT_PAGE_H
#define REPORT_PAGE_H

#include "app/ui/base_page.h"

/**
 * @brief 报告页面组件
 */
class ReportPage : public BasePage
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit ReportPage(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~ReportPage();

protected:
    /**
     * @brief 初始化内容区域
     */
    void initContent() override;
};

#endif // REPORT_PAGE_H 