#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFile>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QFocusEvent>
#include <QResizeEvent>
#include <QEvent>
#include "app/ui/toolbar.h"
#include "statusbar.h"
#include "app/ui/screen_recorder_overlay.h"

class PageManager;
class NavigationBar;

/**
 * @brief 主窗口类
 * 
 * 应用程序的主窗口，包含页面管理器、导航栏和工具栏
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit MainWindow(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~MainWindow();
    
    /**
     * @brief 获取工具栏
     * @return 工具栏指针
     */
    SmartScope::ToolBar* getToolBar() const;

protected:
    /**
     * @brief 按键事件处理
     * @param event 按键事件
     */
    void keyPressEvent(QKeyEvent *event) override;
    
    /**
     * @brief 获得焦点事件处理
     * @param event 焦点事件
     */
    void focusInEvent(QFocusEvent *event) override;
    
    /**
     * @brief 失去焦点事件处理
     * @param event 焦点事件
     */
    void focusOutEvent(QFocusEvent *event) override;
    
    /**
     * @brief 窗口关闭事件处理
     * @param event 关闭事件
     */
    void closeEvent(QCloseEvent *event) override;
    
    /**
     * @brief 事件过滤器
     * @param watched 被监视的对象
     * @param event 事件
     * @return 是否处理了事件
     */
    bool eventFilter(QObject *watched, QEvent *event) override;
    
    /**
     * @brief 调整大小事件
     * @param event 事件
     */
    void resizeEvent(QResizeEvent *event) override;

private:
    /**
     * @brief 设置UI
     */
    void setupUi();
    
    /**
     * @brief 加载样式表
     */
    void loadStyleSheet();
    
    /**
     * @brief 更新导航栏位置
     */
    void updateNavigationBarPosition();
    
    /**
     * @brief 更新状态栏位置
     */
    void updateStatusBarPosition();
    
    void updateRecorderPosition();

    /**
     * @brief 创建默认的工作目录结构（root/defaultSubDir、Screenshots、Recordings）
     */
    void createDefaultDirectories();
    
    PageManager *m_pageManager;       ///< 页面管理器
    NavigationBar *m_navigationBar;   ///< 导航栏
    SmartScope::ToolBar *m_toolBar;   ///< 工具栏
    SmartScope::StatusBar *m_statusBar; ///< 状态栏
    ScreenRecorderOverlay* m_screenRecorder = nullptr; ///< 录屏悬浮控件
};

#endif // MAINWINDOW_H 