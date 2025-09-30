#include "app/ui/measurement_menu.h"
#include "infrastructure/logging/logger.h"
#include <QIcon>
#include <QTimer>

// MeasurementMenuButton 实现
MeasurementMenuButton::MeasurementMenuButton(const QString &iconPath, const QString &text, QWidget *parent)
    : QPushButton(parent)
{
    // 设置图标
    QIcon icon(iconPath);
    setIcon(icon);
    setIconSize(QSize(40, 40));
    
    // 设置文本
    setText(text);
    
    // 初始化按钮
    initialize();
    
    // 确保点击响应正常
    setAutoDefault(false);
    setDefault(false);
    setAutoRepeat(false);
    setCheckable(false);
    setFocusPolicy(Qt::NoFocus); // 避免焦点问题
    
    // 对于撤回按钮，特别确保正常响应点击
    if (text == "撤回") {
        LOG_INFO("创建撤回按钮，确保使用标准点击处理");
        
        // 使用 timer 延迟释放
        connect(this, &QPushButton::pressed, this, [this]() {
            // 强制延迟处理释放信号，避免过快响应导致的问题
            QTimer::singleShot(10, this, [this]() {
                emit clicked();
            });
        });
    }
}

void MeasurementMenuButton::initialize()
{
    // 设置按钮尺寸
    if (text().isEmpty()) {
        // 主页和返回按钮 - 设置为正方形
        setFixedSize(120, 120);  // 修改高度与其他按钮保持一致
        
        // 设置样式
        setStyleSheet(
            "QPushButton {"
            "  background-color: rgba(30, 30, 30, 150);"  // 未选中状态为深色半透明
            "  border: none;"
            "  border-radius: 15px;"  // 设置圆角
            "  color: #FFFFFF;"  // 改为白色，更适合透明背景
            "  padding: 15px;"  // 设置内边距
            "  text-align: center;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(80, 80, 80, 180);"  // 半透明悬停效果
            "}"
            "QPushButton[active=\"true\"] {"
            "  background-color: rgba(100, 100, 100, 220);"  // 半透明激活效果
            "  color: #FFFFFF;"
            "}"
        );
    } else {
        // 其他按钮 - 保持原来的长方形尺寸，但减小高度以适应菜单栏
        setFixedSize(220, 120);  // 减小高度以适应菜单栏
        
        // 设置字体大小
        QFont font = this->font();
        font.setPointSize(30);  // 稍微减小字体大小
        setFont(font);
        
        // 设置样式
        setStyleSheet(
            "QPushButton {"
            "  background-color: rgba(30, 30, 30, 150);"  // 未选中状态为深色半透明
            "  border: none;"
            "  border-radius: 15px;"  // 设置圆角
            "  color: #FFFFFF;"  // 改为白色，更适合透明背景
            "  padding: 10px 15px;"  // 减小内边距
            "  text-align: center;"
            "  font-size: 40px;"  // 调整字体大小
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(80, 80, 80, 180);"  // 半透明悬停效果
            "}"
            "QPushButton[active=\"true\"] {"
            "  background-color: rgba(100, 100, 100, 220);"  // 半透明激活效果
            "  color: #FFFFFF;"
            "}"
        );
    }
    
    // 设置图标大小
    setIconSize(QSize(50, 50));  // 稍微减小图标尺寸
    
    LOG_DEBUG(QString("3D测量菜单按钮初始化完成: %1").arg(text().isEmpty() ? "图标按钮" : text()));
}

// MeasurementMenuBar 实现
MeasurementMenuBar::MeasurementMenuBar(QWidget *parent)
    : QWidget(parent),
      m_layout(nullptr),
      m_backgroundPanel(nullptr),
      m_buttonLayout(nullptr)
{
    // 设置独立组件所需的属性
    setObjectName("measurementMenuBar");
    
    // 确保菜单栏可以接收鼠标事件
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    
    // 设置固定高度和最小宽度
    setFixedHeight(160);  // 增加高度，确保能完全包含按钮
    setMinimumWidth(1700);  // 增加最小宽度以容纳更大的按钮
    
    // 设置透明背景
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    
    // 设置样式
    setStyleSheet(
        "QWidget#measurementMenuBar {"
        "   background-color: rgba(30, 30, 30, 220);"
        "   border-radius: 40px;"
        "   border: 1px solid #444444;"
        "}"
    );
    
    setupUI();
    
    LOG_INFO("3D测量菜单栏构造完成");
}

void MeasurementMenuBar::setupUI()
{
    // 创建水平布局
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(20, 5, 20, 5);  // 减少上下边距，让内部面板更大
    m_layout->setSpacing(15);  // 增加间距
    m_layout->setAlignment(Qt::AlignCenter);
    
    // 创建半透明背景面板
    m_backgroundPanel = new QWidget(this);
    m_backgroundPanel->setObjectName("menuBackgroundPanel");
    m_backgroundPanel->setStyleSheet("background-color: rgba(30, 30, 30, 150); border-radius: 40px; border: 1px solid rgba(80, 80, 80, 200);");
    m_backgroundPanel->setAutoFillBackground(false);
    m_backgroundPanel->setAttribute(Qt::WA_TranslucentBackground, true);
    m_backgroundPanel->setFixedHeight(150);  // 增加高度，确保能完全包含按钮
    m_backgroundPanel->setMinimumWidth(1650);  // 增加最小宽度以适应更大的按钮
    
    // 将背景面板添加到布局中
    m_layout->addWidget(m_backgroundPanel);
    
    // 创建按钮布局，使用网格布局确保按钮在面板内居中显示
    m_buttonLayout = new QHBoxLayout(m_backgroundPanel);
    m_buttonLayout->setContentsMargins(20, 10, 20, 10);  // 调整上下边距
    m_buttonLayout->setSpacing(20);  // 增加按钮间距
    m_buttonLayout->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);  // 确保按钮垂直和水平居中
    
    LOG_INFO("3D测量菜单栏UI设置完成");
}

MeasurementMenuButton* MeasurementMenuBar::addButton(const QString &iconPath, const QString &text)
{
    MeasurementMenuButton *button = new MeasurementMenuButton(iconPath, text, this);
    // 确保按钮在布局中正确显示
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_buttonLayout->addWidget(button);
    return button;
} 