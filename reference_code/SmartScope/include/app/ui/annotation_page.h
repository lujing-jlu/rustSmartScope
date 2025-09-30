#ifndef ANNOTATION_PAGE_H
#define ANNOTATION_PAGE_H

#include "app/ui/base_page.h"

/**
 * @brief 注释页面组件
 */
class AnnotationPage : public BasePage
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit AnnotationPage(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~AnnotationPage();

protected:
    /**
     * @brief 初始化内容区域
     */
    void initContent() override;
};

#endif // ANNOTATION_PAGE_H 