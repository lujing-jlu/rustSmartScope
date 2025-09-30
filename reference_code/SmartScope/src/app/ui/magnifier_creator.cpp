#include "app/ui/magnifier_internal.h"

MagnifierCreator::MagnifierCreator()
{
}

MagnifierCreator::~MagnifierCreator()
{
}

bool MagnifierCreator::create(QWidget* contentWidget, QLabel* leftImageLabel, 
                              float leftAreaRatio, QWidget** outContainer, 
                              QLabel** outLabel, QSize magnifierSize)
{
    // 确保内容区域已初始化
    if (!contentWidget || !leftImageLabel) {
        qDebug() << "内容区域或图像标签未初始化，无法创建放大镜";
        return false;
    }
    
    // 获取内容区域的宽度
    int contentWidth = contentWidget->width();
    
    // 计算分界线位置
    int borderX = static_cast<int>(contentWidth * leftAreaRatio);
    
    // 放大镜位置设在分界线上
    int magnifierX = borderX;
    int magnifierY = contentWidget->height() / 2; // 垂直居中
    
    // 创建一个透明悬浮窗口作为放大镜容器
    QWidget* magnifierContainer = new QWidget(nullptr);
    magnifierContainer->setObjectName("magnifierContainer");
    magnifierContainer->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
    magnifierContainer->setAttribute(Qt::WA_TranslucentBackground);
    magnifierContainer->setAttribute(Qt::WA_ShowWithoutActivating);
    magnifierContainer->setAttribute(Qt::WA_NoSystemBackground);
    magnifierContainer->setAutoFillBackground(false);
    
    // 设置放大镜大小
    int magnifierSizeVal = magnifierSize.width();
    magnifierContainer->setFixedSize(magnifierSizeVal, magnifierSizeVal);
    
    // 创建放大镜标签
    QLabel* magnifierLabel = new QLabel(magnifierContainer);
    magnifierLabel->setObjectName("magnifierLabel");
    magnifierLabel->setFixedSize(magnifierSizeVal, magnifierSizeVal);
    magnifierLabel->setAlignment(Qt::AlignCenter);
    magnifierLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    
    // 居中放置放大镜标签
    magnifierLabel->move(0, 0);
    
    // 计算放大镜在屏幕上的位置（将内容区域坐标转换为全局坐标）
    QPoint globalPos = contentWidget->mapToGlobal(QPoint(magnifierX, magnifierY));
    // 修改：左端对齐，仅垂直方向居中调整
    globalPos -= QPoint(0, magnifierSizeVal/2); // 只调整垂直方向，使放大镜左端对齐分界线
    
    // 显示放大镜
    magnifierContainer->setGeometry(globalPos.x(), globalPos.y(), magnifierSizeVal, magnifierSizeVal);
    magnifierContainer->show();
    magnifierContainer->raise();
    
    // 输出结果
    *outContainer = magnifierContainer;
    *outLabel = magnifierLabel;
    
    // qDebug() << QString("放大镜创建成功，位置：(%1, %2)，大小：%3x%4")
    //        .arg(globalPos.x()).arg(globalPos.y())
    //        .arg(magnifierSizeVal).arg(magnifierSizeVal);
    
    return true;
}

void MagnifierCreator::destroy(QWidget* magnifierContainer)
{
    if (magnifierContainer) {
        // qDebug() << "销毁放大镜资源";
        magnifierContainer->hide();
        magnifierContainer->setParent(nullptr);  // 断开父子关系
        magnifierContainer->deleteLater();
    }
} 