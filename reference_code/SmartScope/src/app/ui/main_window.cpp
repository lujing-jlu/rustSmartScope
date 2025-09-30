#include "app/utils/screenshot_manager.h"
#include "app/utils/keyboard_listener.h"
#include "infrastructure/logging/logger.h"

// 使用日志宏
#define LOG_INFO(message) Logger::instance().info(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARNING(message) Logger::instance().warning(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(message) Logger::instance().error(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_DEBUG(message) Logger::instance().debug(message, __FILE__, __LINE__, __FUNCTION__)

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_screenshotManager(new ScreenshotManager(this))
{
    // ... existing code ...
    
    // 添加截图按钮到工具栏
    QAction *screenshotAction = new QAction(QIcon(":/icons/screenshot.png"), tr("截图"), this);
    screenshotAction->setStatusTip(tr("截取当前窗口画面"));
    connect(screenshotAction, &QAction::triggered, this, &MainWindow::onScreenshot);
    ui->mainToolBar->addAction(screenshotAction);
    
    // 设置窗口属性
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_KeyCompression, false);  // 禁用键盘压缩，确保捕获所有键盘事件
    
    // 安装键盘监听器
    KeyboardListener::instance().installEventFilter(this);
    
    LOG_INFO("主窗口已创建，设置了以下属性:");
    LOG_INFO(QString("- 焦点策略: %1").arg(focusPolicy()));
    LOG_INFO(QString("- 窗口标志: 0x%1").arg(windowFlags(), 0, 16));
    LOG_INFO(QString("- 键盘追踪: %1").arg(hasMouseTracking() ? "已启用" : "未启用"));
    
    // ... existing code ...
}

void MainWindow::onScreenshot()
{
    if (m_screenshotManager->captureWindow(this)) {
        QString path = m_screenshotManager->getLastScreenshotPath();
        QMessageBox::information(this, tr("截图成功"), 
            tr("截图已保存到: %1").arg(path));
    } else {
        QMessageBox::warning(this, tr("截图失败"), 
            tr("截图保存失败，请检查存储空间和权限"));
    }
}

// 重写键盘事件处理函数，增加调试输出
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    LOG_INFO(QString("MainWindow接收到按键事件: 键值=%1, 文本='%2'").arg(event->key()).arg(event->text()));
    QMainWindow::keyPressEvent(event);
}

// 重写焦点事件处理函数，增加调试输出
void MainWindow::focusInEvent(QFocusEvent *event)
{
    LOG_INFO(QString("MainWindow获得焦点，原因=%1").arg(event->reason()));
    QMainWindow::focusInEvent(event);
}

void MainWindow::focusOutEvent(QFocusEvent *event)
{
    LOG_INFO(QString("MainWindow失去焦点，原因=%1").arg(event->reason()));
    QMainWindow::focusOutEvent(event);
} 