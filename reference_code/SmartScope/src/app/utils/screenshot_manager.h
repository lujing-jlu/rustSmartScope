#ifndef SCREENSHOT_MANAGER_H
#define SCREENSHOT_MANAGER_H

#include <QObject>
#include <QWidget>
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QWindow>

namespace SmartScope {
namespace App {
namespace Utils {

class ScreenshotManager : public QObject {
    Q_OBJECT

public:
    explicit ScreenshotManager(QObject *parent = nullptr);
    ~ScreenshotManager();

    // 截取指定窗口的截图
    bool captureWindow(QWidget *window);
    
    // 截取整个屏幕的截图
    bool captureFullScreen();
    
    // 截取当前窗口所在的屏幕 
    bool captureCurrentScreen(QWidget *window);
    
    // 获取最后保存的截图路径
    QString getLastScreenshotPath() const { return m_lastScreenshotPath; }

private:
    QString m_lastScreenshotPath;
    QString generateScreenshotPath() const;
    bool saveScreenshot(const QImage &image, const QString &path);
};

} // namespace Utils
} // namespace App
} // namespace SmartScope

#endif // SCREENSHOT_MANAGER_H 