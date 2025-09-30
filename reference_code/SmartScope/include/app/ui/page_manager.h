#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include <QObject>
#include <QStackedWidget>
#include <QMap>
#include <opencv2/opencv.hpp>
#include "app/ui/debug_page.h"

// 前向声明
class HomePage;
class MeasurementPage;
class SettingsPage;

/**
 * 页面类型枚举
 */
enum class PageType {
    Home,           // 主页
    Preview,        // 预览选择
    PhotoPreview,   // 拍照预览
    ScreenshotPreview, // 截屏预览
    VideoPreview,   // 视频预览
    Report,         // 报告
    Annotation,     // 注释
    Measurement,    // 3D测量
    Debug,          // 调试页面
    Settings,       // 参数设置
    Exit,           // 退出程序
    Back            // 返回上一页
};

/**
 * @brief 页面管理器类
 * 
 * 管理应用程序中的不同页面，处理页面切换
 */
class PageManager : public QStackedWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit PageManager(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~PageManager();
    
    /**
     * @brief 切换到指定页面
     * @param pageType 页面类型
     */
    void switchToPage(PageType pageType);
    
    /**
     * @brief 获取当前页面类型
     * @return 当前页面类型
     */
    PageType getCurrentPageType() const;
    
    /**
     * @brief 捕获左右相机图像并传递给3D测量页面
     * @return 是否成功
     */
    bool captureCameraImagesForMeasurement();
    
    /**
     * @brief 获取主页实例
     * @return 主页实例指针
     */
    HomePage* getHomePage() const;
    
    /**
     * @brief 获取3D测量页面实例
     * @return 3D测量页面实例指针
     */
    MeasurementPage* getMeasurementPage() const;
    
    /**
     * @brief 获取调试页面实例
     * @return 调试页面实例指针
     */
    SmartScope::App::Ui::DebugPage* getDebugPage() const;
    
    /**
     * @brief 获取参数设置页面实例
     * @return 参数设置页面实例指针
     */
    SettingsPage* getSettingsPage() const;

signals:
    /**
     * @brief 页面变化信号
     * @param pageType 新的页面类型
     */
    void pageChanged(PageType pageType);

private:
    /**
     * @brief 设置页面
     */
    void setupPages();
    
    QMap<PageType, QWidget*> m_pages;  ///< 页面映射
    PageType m_currentPageType;        ///< 当前页面类型
};

#endif // PAGE_MANAGER_H 