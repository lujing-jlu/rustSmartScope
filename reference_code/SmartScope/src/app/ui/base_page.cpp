#include "app/ui/base_page.h"
#include <QDebug>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>

BasePage::BasePage(const QString &title, QWidget *parent)
    : QWidget(parent),
      m_mainLayout(nullptr),
      m_titleLabel(nullptr),
      m_contentWidget(nullptr),
      m_contentLayout(nullptr)
{
    setPageTitle(title);
    initUI();
    // 不要在构造函数中调用纯虚函数
    // 子类会在自己的构造函数完成后调用initContent()
    
    // 在构造函数中添加
    setAttribute(Qt::WA_AcceptTouchEvents);
    
    // 设置所有输入控件接受触摸事件
    QList<QWidget*> widgets = findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (qobject_cast<QLineEdit*>(widget) || 
            qobject_cast<QTextEdit*>(widget) || 
            qobject_cast<QPlainTextEdit*>(widget)) {
            widget->setAttribute(Qt::WA_AcceptTouchEvents);
            widget->setProperty("QT_IVI_SURFACE_VIRTUAL_KEYBOARD", true);
        }
    }
}

BasePage::~BasePage()
{
}

void BasePage::setPageTitle(const QString &title)
{
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

QWidget* BasePage::getContentWidget() const
{
    return m_contentWidget;
}

QVBoxLayout* BasePage::getContentLayout() const
{
    return m_contentLayout;
}

void BasePage::initUI()
{
    // 创建主布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);  // 去掉外边距
    m_mainLayout->setSpacing(0);  // 减小间距为0，完全消除空隙
    
    // 创建标题 - 设置为0高度，实际上隐藏标题
    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setFixedHeight(0); // 设置高度为0，隐藏标题
    m_titleLabel->setContentsMargins(0, 0, 0, 0);  // 设置标题的内边距为0
    m_mainLayout->addWidget(m_titleLabel);
    
    // 创建内容区域
    m_contentWidget = new QWidget(this);
    m_contentWidget->setObjectName("pageContent");
    m_contentWidget->setStyleSheet("background-color: #1E1E1E; border-radius: 0px;");  // 去掉圆角
    
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);  // 设置内容区域边距为0
    m_contentLayout->setSpacing(0);  // 设置间距为0
    
    // 添加内容区域到主布局，占据所有空间
    m_mainLayout->addWidget(m_contentWidget, 1);
    
    qDebug() << "BasePage UI初始化完成";
} 