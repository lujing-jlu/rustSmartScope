#include "app/ui/report_page.h"
#include <QDebug>
#include <QLabel>

ReportPage::ReportPage(QWidget *parent)
    : BasePage("报告", parent)
{
    initContent();
}

ReportPage::~ReportPage()
{
}

void ReportPage::initContent()
{
    // 设置内容区域的布局参数，顶部预留状态栏空间
    m_contentLayout->setContentsMargins(40, 120, 40, 40);  // 顶部预留状态栏空间

    // 添加报告提示信息
    QLabel *reportLabel = new QLabel("报告页面正在开发中...", m_contentWidget);
    reportLabel->setAlignment(Qt::AlignCenter);
    QFont reportFont = reportLabel->font();
    reportFont.setPointSize(40);
    reportLabel->setFont(reportFont);
    m_contentLayout->addWidget(reportLabel);

    qDebug() << "报告页面内容初始化完成";
}