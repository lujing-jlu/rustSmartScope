#include "app/ui/toolbar.h"
#include "app/ui/modern_icons.h"
#include "infrastructure/logging/logger.h"
#include <QResizeEvent>
#include <QShowEvent>
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QTimer>

namespace SmartScope {

ToolBar::ToolBar(QWidget *parent)
    : QWidget(parent)
{
    // 设置无边框窗口
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    // 确保工具栏可以接收鼠标事件
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setMouseTracking(true);
    
    // 设置工具栏样式
    setStyleSheet(
        "QWidget {"
        "   background-color: rgba(30, 30, 30, 150);"
        "   border-radius: 15px;"
        "   border: 1px solid rgba(80, 80, 80, 200);"
        "}"
    );
    
    // 底部信息标签
    m_bottomInfoLabel = new QLabel(this);
    m_bottomInfoLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   background-color: transparent;"
        "   font-size: 18px;"
        "}"
    );
    m_bottomInfoLabel->setText("");
    m_bottomInfoLabel->hide();
    
    // 确保工具栏可见
    show();
    raise();
    
    LOG_INFO("工具栏初始化完成");
}

ToolBar::~ToolBar()
{
    // 清理按钮
    for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it) {
        delete it.value();
    }
    m_buttons.clear();
    m_buttonPositions.clear();
}

QPushButton* ToolBar::addButton(const QString& id, const QString& iconPath, const QString& tooltip, int position)
{
    // 检查ID是否已存在
    if (m_buttons.contains(id)) {
        LOG_WARNING(QString("按钮ID已存在: %1").arg(id));
        return m_buttons[id];
    }
    
    // 创建按钮
    QPushButton* button = new QPushButton("", this);
    button->setObjectName(id);
    button->setFixedSize(m_buttonSize, m_buttonSize);
    button->setToolTip(tooltip);
    
    // 设置图标
    if (!iconPath.isEmpty()) {
        QIcon icon;
        // 检查是否使用现代化图标
        if (iconPath.startsWith("modern:")) {
            QString iconType = iconPath.mid(7); // 移除 "modern:" 前缀
            icon = createModernIcon(iconType);
        } else {
            icon = createWhiteIcon(iconPath);
        }
        button->setIcon(icon);

        // 根据图标类型设置不同的大小
        QSize iconSize(70, 70); // 默认大小
        if (iconPath.contains("screenshot")) {
            iconSize = QSize(60, 60); // 截图图标稍小
        } else if (iconPath.contains("AI")) {
            iconSize = QSize(58, 58); // AI图标更小
        } else if (iconPath.contains("brightness")) {
            iconSize = QSize(65, 65); // 亮度图标中等大小
        }
        button->setIconSize(iconSize);
    }
    
    // 设置样式
    button->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(30, 30, 30, 220);"
        "   color: white;"
        "   border-radius: 15px;"
        "   border: 0px solid #444444;"
        "   padding: 0px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(60, 60, 60, 220);"
        "   border: 0px solid #666666;"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(90, 90, 90, 220);"
        "   border: 0px solid #888888;"
        "}"
    );
    
    // 确保按钮可以接收点击事件
    button->setFocusPolicy(Qt::StrongFocus);
    button->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    button->setMouseTracking(true);
    
    // 添加调试信息
    LOG_INFO(QString("创建按钮: %1, 可接收鼠标事件: %2").arg(id).arg(!button->testAttribute(Qt::WA_TransparentForMouseEvents)));
    
    // 添加到映射
    m_buttons[id] = button;
    m_buttonPositions[id] = position;
    
    // 重新排列按钮
    rearrangeButtons();
    
    // 显示按钮
    button->show();
    button->raise();
    
    LOG_INFO(QString("添加按钮: %1, 位置: %2").arg(id).arg(position));
    
    return button;
}

QPushButton* ToolBar::addBottomButton(const QString& id, const QString& iconPath, const QString& tooltip)
{
    if (m_buttons.contains(id)) {
        return m_buttons[id];
    }
    QPushButton* button = new QPushButton("", this);
    button->setObjectName(id);
    button->setFixedSize(m_buttonSize, m_buttonSize);
    button->setToolTip(tooltip);

    // 使用资源图标
    QIcon icon = createWhiteIcon(iconPath);
    button->setIcon(icon);
    button->setIconSize(QSize(70, 70));

    button->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(30, 30, 30, 220);"
        "   color: white;"
        "   border-radius: 15px;"
        "   border: 0px solid #444444;"
        "   padding: 0px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(60, 60, 60, 220);"
        "   border: 0px solid #666666;"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(90, 90, 90, 220);"
        "   border: 0px solid #888888;"
        "}"
    );

    m_buttons[id] = button;
    m_bottomButtonId = id;

    rearrangeButtons();
    button->show();
    button->raise();
    return button;
}

QPushButton* ToolBar::getButton(const QString& id)
{
    if (m_buttons.contains(id)) {
        return m_buttons[id];
    }
    return nullptr;
}

bool ToolBar::removeButton(const QString& id)
{
    if (m_buttons.contains(id)) {
        QPushButton* button = m_buttons[id];
        m_buttons.remove(id);
        m_buttonPositions.remove(id);
        if (m_bottomButtonId == id) m_bottomButtonId.clear();
        delete button;
        
        // 重新排列按钮
        rearrangeButtons();
        
        LOG_INFO(QString("移除按钮: %1").arg(id));
        return true;
    }
    
    LOG_WARNING(QString("尝试移除不存在的按钮: %1").arg(id));
    return false;
}

void ToolBar::showButton(const QString& id)
{
    if (m_buttons.contains(id)) {
        m_buttons[id]->show();
        m_buttons[id]->raise();
        LOG_INFO(QString("显示按钮: %1").arg(id));
    }
}

void ToolBar::hideButton(const QString& id)
{
    if (m_buttons.contains(id)) {
        m_buttons[id]->hide();
        LOG_INFO(QString("隐藏按钮: %1").arg(id));
    }
}

void ToolBar::updatePosition()
{
    // 获取主窗口
    QWidget* mainWindow = window();
    if (!mainWindow) {
        return;
    }
    
    // 计算工具栏在主窗口中的位置 - 靠右显示，但留出一些空隙
    int toolbarX = mainWindow->width() - m_buttonSize - 20; // 右侧留出20像素的空隙
    int toolbarY = 100; // 从顶部开始的位置
    
    // 设置工具栏自身的大小和位置
    setGeometry(toolbarX, toolbarY, m_buttonSize, mainWindow->height() - 200);
    
    // 重新排列按钮
    rearrangeButtons();
    
    // 确保工具栏可见
    show();
    raise();
    
    LOG_INFO(QString("更新工具栏位置: (%1, %2)，大小: %3x%4").arg(toolbarX).arg(toolbarY)
             .arg(width()).arg(height()));
}

void ToolBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // 更新按钮位置
    rearrangeButtons();
}

void ToolBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    
    // 更新按钮位置
    QTimer::singleShot(100, this, &ToolBar::updatePosition);
}

QIcon ToolBar::createWhiteIcon(const QString& iconPath)
{
    QPixmap pixmap(iconPath);
    if (pixmap.isNull()) {
        LOG_WARNING(QString("无法加载图标: %1").arg(iconPath));
        return QIcon();
    }

    // 创建白色图标 - 多重方法确保转换成功
    QPixmap whitePixmap(pixmap.size());
    whitePixmap.fill(Qt::transparent);

    QPainter painter(&whitePixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 方法1：标准合成模式
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawPixmap(0, 0, pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(whitePixmap.rect(), QColor(255, 255, 255, 255));

    // 方法2：如果第一种方法效果不好，使用强制白色覆盖
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    painter.fillRect(whitePixmap.rect(), QColor(255, 255, 255, 255));

    painter.end();

    return QIcon(whitePixmap);
}

QIcon ToolBar::createModernIcon(const QString& iconType)
{
    if (iconType == "screenshot") {
        return ModernIcons::createScreenshotIcon(70, Qt::white);
    } else if (iconType == "led_brightness") {
        return ModernIcons::createLedBrightnessIcon(70, Qt::white);
    } else if (iconType == "ai_detection") {
        return ModernIcons::createAiDetectionIcon(70, Qt::white);
    } else if (iconType == "camera_adjust") {
        return ModernIcons::createCameraAdjustIcon(70, Qt::white);
    } else {
        LOG_WARNING(QString("未知的现代化图标类型: %1").arg(iconType));
        return QIcon();
    }
}

void ToolBar::rearrangeButtons()
{
    // 获取主窗口
    QWidget* mainWindow = window();
    if (!mainWindow) {
        return;
    }
    
    // 按位置排序按钮
    QMap<int, QString> positionToId;
    for (auto it = m_buttonPositions.begin(); it != m_buttonPositions.end(); ++it) {
        positionToId[it.value()] = it.key();
    }
    
    // 从顶部开始正常放置
    int topY = 20;
    for (auto it = positionToId.begin(); it != positionToId.end(); ++it) {
        QString id = it.value();
        if (m_buttons.contains(id)) {
            QPushButton* button = m_buttons[id];
            button->move(0, topY + (m_buttonSize + m_buttonSpacing) * it.key());
            button->raise();
            LOG_INFO(QString("按钮 %1 位置: (%2, %3)").arg(id).arg(button->x()).arg(button->y()));
        }
    }

    // 底部固定按钮与文本
    if (!m_bottomButtonId.isEmpty() && m_buttons.contains(m_bottomButtonId)) {
        QPushButton* bottomBtn = m_buttons[m_bottomButtonId];
        int bottomMargin = 20;
        // 按钮固定在底部，不因计时文本而改变位置
        int y = height() - bottomMargin - m_buttonSize;
        bottomBtn->move(0, y);
        bottomBtn->raise();
        if (m_bottomInfoLabel) {
            m_bottomInfoLabel->adjustSize();
            // 计时文本显示在按钮上方
            int labelX = (m_buttonSize - m_bottomInfoLabel->width()) / 2;
            int labelY = y - m_bottomInfoLabel->height() - 6;
            m_bottomInfoLabel->move(labelX, labelY);
            m_bottomInfoLabel->raise();
        }
    }
}

void ToolBar::setBottomInfoVisible(bool visible)
{
    if (m_bottomInfoLabel) {
        if (visible) m_bottomInfoLabel->show();
        else m_bottomInfoLabel->hide();
        rearrangeButtons();
    }
}

void ToolBar::setBottomInfoText(const QString& text)
{
    if (m_bottomInfoLabel) {
        m_bottomInfoLabel->setText(text);
        m_bottomInfoLabel->adjustSize();
        rearrangeButtons();
    }
}

} // namespace SmartScope 