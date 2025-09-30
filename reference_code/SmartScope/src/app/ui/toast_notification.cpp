#include "app/ui/toast_notification.h"
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QStyle>
#include <QStyleOption>

ToastNotification::ToastNotification(QWidget *parent)
    : QWidget(parent),
      m_messageLabel(nullptr),
      m_timer(nullptr),
      m_opacity(0.0f),
      m_animation(nullptr)
{
    // 设置窗口属性
    setWindowFlags(Qt::FramelessWindowHint | Qt::ToolTip);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    
    // 初始化UI
    initUI();
    
    // 初始化定时器
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        // 开始淡出动画
        m_animation->setStartValue(1.0f);
        m_animation->setEndValue(0.0f);
        m_animation->start();
    });
    
    // 初始化动画
    m_animation = new QPropertyAnimation(this, "opacity", this);
    m_animation->setDuration(300);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
        if (m_opacity == 0.0f) {
            hide();
            deleteLater();
        }
    });
}

ToastNotification::~ToastNotification()
{
}

void ToastNotification::initUI()
{
    // 创建消息标签
    m_messageLabel = new QLabel(this);
    m_messageLabel->setAlignment(Qt::AlignCenter);
    m_messageLabel->setStyleSheet(
        "QLabel {"
        "  color: white;"
        "  font-size: 24px;"
        "  font-weight: bold;"
        "  font-family: 'WenQuanYi Zen Hei';"
        "  padding: 20px 30px;"
        "}"
    );
    
    // 设置布局
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_messageLabel);
    
    // 设置初始大小
    resize(400, 80);
}

void ToastNotification::showMessage(const QString &message, int duration, Position position, Type type)
{
    // 设置消息内容
    m_messageLabel->setText(message);
    
    // 应用样式
    applyStyle(type);
    
    // 调整大小
    adjustSize();
    
    // 计算位置
    calculatePosition(position);
    
    // 设置动画
    m_animation->setStartValue(0.0f);
    m_animation->setEndValue(1.0f);
    m_animation->start();
    
    // 显示窗口
    show();
    raise();
    
    // 启动定时器
    m_timer->start(duration);
}

void ToastNotification::setOpacity(float opacity)
{
    m_opacity = opacity;
    update();
}

float ToastNotification::opacity() const
{
    return m_opacity;
}

void ToastNotification::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 设置透明度
    painter.setOpacity(m_opacity);
    
    // 绘制圆角矩形背景
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

void ToastNotification::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void ToastNotification::calculatePosition(Position position)
{
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    
    int x = 0, y = 0;
    
    switch (position) {
        case TopCenter:
            x = (screenGeometry.width() - width()) / 2;
            y = screenGeometry.height() * 0.1;
            break;
        case TopRight:
            x = screenGeometry.width() - width() - 20;
            y = 20;
            break;
        case TopLeft:
            x = 20;
            y = 20;
            break;
        case BottomCenter:
            x = (screenGeometry.width() - width()) / 2;
            y = screenGeometry.height() - height() - screenGeometry.height() * 0.1;
            break;
        case BottomRight:
            x = screenGeometry.width() - width() - 20;
            y = screenGeometry.height() - height() - 20;
            break;
        case BottomLeft:
            x = 20;
            y = screenGeometry.height() - height() - 20;
            break;
        case Center:
            x = (screenGeometry.width() - width()) / 2;
            y = (screenGeometry.height() - height()) / 2;
            break;
    }
    
    move(x, y);
}

void ToastNotification::applyStyle(Type type)
{
    QString backgroundColor;
    QString borderColor;
    
    switch (type) {
        case Info:
            backgroundColor = "rgba(23, 162, 184, 220)";
            borderColor = "#138496";
            break;
        case Success:
            backgroundColor = "rgba(40, 167, 69, 220)";
            borderColor = "#1e7e34";
            break;
        case Warning:
            backgroundColor = "rgba(255, 193, 7, 220)";
            borderColor = "#d39e00";
            break;
        case Error:
            backgroundColor = "rgba(220, 53, 69, 220)";
            borderColor = "#bd2130";
            break;
    }
    
    setStyleSheet(QString(
        "ToastNotification {"
        "  background-color: %1;"
        "  border: 2px solid %2;"
        "  border-radius: 15px;"
        "}"
    ).arg(backgroundColor, borderColor));
}

// 全局函数，便于直接调用显示toast
void showToast(QWidget *parent, const QString &message, int duration, 
               ToastNotification::Position position, ToastNotification::Type type)
{
    ToastNotification *toast = new ToastNotification(parent);
    toast->showMessage(message, duration, position, type);
} 