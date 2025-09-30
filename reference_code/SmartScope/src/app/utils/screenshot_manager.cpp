#include "screenshot_manager.h"
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QWindow>
#include "infrastructure/config/config_manager.h"

namespace SmartScope {
namespace App {
namespace Utils {

ScreenshotManager::ScreenshotManager(QObject *parent)
    : QObject(parent)
{
}

ScreenshotManager::~ScreenshotManager()
{
}

bool ScreenshotManager::captureWindow(QWidget *window)
{
    if (!window) {
        return false;
    }

    // 获取窗口的截图
    QImage screenshot = window->grab().toImage();
    
    // 生成保存路径
    QString savePath = generateScreenshotPath();
    
    // 保存截图
    if (saveScreenshot(screenshot, savePath)) {
        m_lastScreenshotPath = savePath;
        return true;
    }
    
    return false;
}

bool ScreenshotManager::captureFullScreen()
{
    // 获取主屏幕
    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    if (!primaryScreen) {
        return false;
    }
    
    // 捕获所有可见屏幕的组合图像
    QRect fullScreenRect;
    for (QScreen* screen : QGuiApplication::screens()) {
        fullScreenRect = fullScreenRect.united(screen->geometry());
    }
    
    // 创建全屏截图
    QImage screenshot = primaryScreen->grabWindow(0, 
                                                fullScreenRect.x(), 
                                                fullScreenRect.y(), 
                                                fullScreenRect.width(), 
                                                fullScreenRect.height()).toImage();
    
    // 生成保存路径
    QString savePath = generateScreenshotPath();
    
    // 保存截图
    if (saveScreenshot(screenshot, savePath)) {
        m_lastScreenshotPath = savePath;
        return true;
    }
    
    return false;
}

bool ScreenshotManager::captureCurrentScreen(QWidget *window)
{
    if (!window) {
        return false;
    }
    
    // 获取窗口所在的屏幕
    QScreen *screen = window->screen();
    if (!screen) {
        // 如果无法获取窗口的屏幕，尝试使用主屏幕
        screen = QGuiApplication::primaryScreen();
        if (!screen) {
            return false;
        }
    }
    
    // 获取窗口的原生句柄
    WId windowId = window->window()->winId();
    
    // 截取窗口所在屏幕的完整图像
    QImage screenshot = screen->grabWindow(0).toImage();
    
    // 生成保存路径
    QString savePath = generateScreenshotPath();
    
    // 保存截图
    if (saveScreenshot(screenshot, savePath)) {
        m_lastScreenshotPath = savePath;
        return true;
    }
    
    return false;
}

QString ScreenshotManager::generateScreenshotPath() const
{
    // 统一根目录: ~/data（可通过ConfigManager覆盖）
    QString rootDirectory = SmartScope::Infrastructure::ConfigManager::instance().getValue("app/root_directory", QDir::homePath() + "/data").toString();
    QString defaultSubDir = SmartScope::Infrastructure::ConfigManager::instance().getValue("app/default_subdirectory", "CAM").toString();
    defaultSubDir.remove('"');
    defaultSubDir.remove('\'');

    // 保存目录: 根/子目录/Screenshots
    QString saveDir = rootDirectory + "/Screenshots";

    QDir dir(saveDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    return saveDir + "/screenshot_" + timestamp + ".png";
}

bool ScreenshotManager::saveScreenshot(const QImage &image, const QString &path)
{
    if (image.isNull()) {
        return false;
    }
    
    // 保存图片
    if (image.save(path)) {
        return true;
    }
    
    return false;
}

} // namespace Utils
} // namespace App
} // namespace SmartScope 