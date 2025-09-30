#include "app/ui/annotation_page.h"
#include <QDebug>
#include <QLabel>

AnnotationPage::AnnotationPage(QWidget *parent)
    : BasePage("注释", parent)
{
    initContent();
}

AnnotationPage::~AnnotationPage()
{
}

void AnnotationPage::initContent()
{
    // 设置内容区域的布局参数，顶部预留状态栏空间
    m_contentLayout->setContentsMargins(40, 120, 40, 40);  // 顶部预留状态栏空间

    // 添加注释提示信息
    QLabel *annotationLabel = new QLabel("注释页面正在开发中...", m_contentWidget);
    annotationLabel->setAlignment(Qt::AlignCenter);
    QFont annotationFont = annotationLabel->font();
    annotationFont.setPointSize(40);  // 将字体大小从16点放大到40点（放大2.5倍）
    annotationLabel->setFont(annotationFont);
    m_contentLayout->addWidget(annotationLabel);

    qDebug() << "注释页面内容初始化完成";
}