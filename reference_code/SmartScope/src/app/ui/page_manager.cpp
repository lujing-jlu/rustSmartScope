#include "app/ui/page_manager.h"
#include "app/ui/home_page.h"
#include "app/ui/preview_page.h"
#include "app/ui/preview_selection_page.h"
#include "app/ui/screenshot_preview_page.h"
#include "app/ui/video_preview_page.h"
#include "app/ui/report_page.h"
#include "app/ui/annotation_page.h"
#include "app/ui/measurement_page.h"
#include "app/ui/debug_page.h"
#include "app/ui/settings_page.h"
#include "app/ui/toast_notification.h"
#include "infrastructure/logging/logger.h"
#include <QTimer>
#include <QEventLoop>
#include <QMessageBox>
#include <QThread>
#include <QElapsedTimer>
#include "infrastructure/config/config_manager.h"

PageManager::PageManager(QWidget *parent)
    : QStackedWidget(parent),
      m_currentPageType(PageType::Home)
{
    setupPages();
}

PageManager::~PageManager()
{
}

void PageManager::setupPages()
{
    // 创建主页
    HomePage *homePage = new HomePage(this);
    m_pages[PageType::Home] = homePage;
    addWidget(homePage);

    // 创建预览选择页面
    PreviewSelectionPage *previewSelectionPage = new PreviewSelectionPage(this);
    m_pages[PageType::Preview] = previewSelectionPage;
    addWidget(previewSelectionPage);

    // 创建拍照预览页面
    PreviewPage *photoPreviewPage = new PreviewPage(this);
    m_pages[PageType::PhotoPreview] = photoPreviewPage;
    addWidget(photoPreviewPage);

    // 创建截屏预览页面
    ScreenshotPreviewPage *screenshotPreviewPage = new ScreenshotPreviewPage(this);
    m_pages[PageType::ScreenshotPreview] = screenshotPreviewPage;
    addWidget(screenshotPreviewPage);

    // 创建视频预览页面
    VideoPreviewPage *videoPreviewPage = new VideoPreviewPage(this);
    m_pages[PageType::VideoPreview] = videoPreviewPage;
    addWidget(videoPreviewPage);

    // 连接主页的工作路径变更信号到预览页面
    connect(homePage, &HomePage::currentWorkPathChanged, photoPreviewPage, &PreviewPage::setCurrentWorkPath);
    connect(homePage, &HomePage::currentWorkPathChanged, screenshotPreviewPage, &ScreenshotPreviewPage::setCurrentWorkPath);
    connect(homePage, &HomePage::currentWorkPathChanged, videoPreviewPage, &VideoPreviewPage::setCurrentWorkPath);

    // 连接预览选择页面的信号
    connect(previewSelectionPage, &PreviewSelectionPage::photoPreviewSelected, this, [this]() {
        switchToPage(PageType::PhotoPreview);
    });
    connect(previewSelectionPage, &PreviewSelectionPage::screenshotPreviewSelected, this, [this]() {
        switchToPage(PageType::ScreenshotPreview);
    });
    connect(previewSelectionPage, &PreviewSelectionPage::videoPreviewSelected, this, [this]() {
        switchToPage(PageType::VideoPreview);
    });
    
    // 创建报告页面
    ReportPage *reportPage = new ReportPage(this);
    m_pages[PageType::Report] = reportPage;
    addWidget(reportPage);
    
    // 注释页面已移除
    
    // 创建3D测量页面
    MeasurementPage *measurementPage = new MeasurementPage(this);
    m_pages[PageType::Measurement] = measurementPage;
    addWidget(measurementPage);
    
    // 创建调试页面
    SmartScope::App::Ui::DebugPage *debugPage = new SmartScope::App::Ui::DebugPage(this);
    m_pages[PageType::Debug] = debugPage;
    addWidget(debugPage);
    
    // 创建参数设置页面
    SettingsPage *settingsPage = new SettingsPage(this);
    m_pages[PageType::Settings] = settingsPage;
    addWidget(settingsPage);
    // 监听调试模式开关，控制调试入口
    connect(settingsPage, &SettingsPage::debugModeSettingChanged, this, [this](bool enabled){
        LOG_INFO(QString("调试模式切换: %1").arg(enabled ? "启用" : "禁用"));
        SmartScope::Infrastructure::ConfigManager::instance().setValue("ui/debug_mode", enabled);
        SmartScope::Infrastructure::ConfigManager::instance().saveConfig();
        // 同步到测量页调试按钮
        MeasurementPage *mp = getMeasurementPage();
        if (mp) mp->setDebugModeFromSettings(enabled);
    });
    // 初始化时按配置设置调试按钮可见性
    {
        bool dbg = SmartScope::Infrastructure::ConfigManager::instance().getValue("ui/debug_mode", false).toBool();
        MeasurementPage *mp = getMeasurementPage();
        if (mp) mp->setDebugModeFromSettings(dbg);
    }
    
    // 设置默认页面
    setCurrentWidget(homePage);
    m_currentPageType = PageType::Home;
    
    LOG_INFO("页面管理器初始化完成");
}

void PageManager::switchToPage(PageType pageType)
{
    if (m_pages.contains(pageType)) {
        QWidget *page = m_pages[pageType];

        // 如果当前页面是预览相关页面，确保关闭任何打开的图片预览对话框
        if (m_currentPageType == PageType::Preview ||
            m_currentPageType == PageType::PhotoPreview ||
            m_currentPageType == PageType::ScreenshotPreview) {
            // 使用ImagePreviewDialog的静态方法关闭对话框
            ImagePreviewDialog::closeIfOpen();
            ScreenshotImagePreviewDialog::closeIfOpen();
        }

        // 如果切换到拍照预览页面，传递当前工作路径
        if (pageType == PageType::PhotoPreview) {
            // 获取主页
            HomePage *homePage = qobject_cast<HomePage*>(m_pages[PageType::Home]);
            if (homePage) {
                // 获取拍照预览页面
                PreviewPage *photoPreviewPage = qobject_cast<PreviewPage*>(page);
                if (photoPreviewPage) {
                    // 设置当前工作路径
                    photoPreviewPage->setCurrentWorkPath(homePage->getCurrentWorkPath());
                    LOG_INFO(QString("设置拍照预览页面工作路径: %1").arg(homePage->getCurrentWorkPath()));
                }
            }

            // 立即切换到拍照预览页面
            setCurrentWidget(page);
            m_currentPageType = pageType;
            LOG_INFO("立即切换到拍照预览页面");
            emit pageChanged(pageType);
            return;
        }
        // 如果切换到截屏预览页面，传递当前工作路径
        else if (pageType == PageType::ScreenshotPreview) {
            // 获取主页
            HomePage *homePage = qobject_cast<HomePage*>(m_pages[PageType::Home]);
            if (homePage) {
                // 获取截屏预览页面
                ScreenshotPreviewPage *screenshotPreviewPage = qobject_cast<ScreenshotPreviewPage*>(page);
                if (screenshotPreviewPage) {
                    // 设置当前工作路径
                    screenshotPreviewPage->setCurrentWorkPath(homePage->getCurrentWorkPath());
                    LOG_INFO(QString("设置截屏预览页面工作路径: %1").arg(homePage->getCurrentWorkPath()));
                }
            }

            // 立即切换到截屏预览页面
            setCurrentWidget(page);
            m_currentPageType = pageType;
            LOG_INFO("立即切换到截屏预览页面");
            emit pageChanged(pageType);
            return;
        }
        else if (pageType == PageType::VideoPreview) {
            // 获取主页
            HomePage *homePage = qobject_cast<HomePage*>(m_pages[PageType::Home]);
            if (homePage) {
                VideoPreviewPage *videoPreviewPage = qobject_cast<VideoPreviewPage*>(page);
                if (videoPreviewPage) {
                    QString root = SmartScope::Infrastructure::ConfigManager::instance().getValue("app/root_directory", QDir::homePath() + "/data").toString();
                    videoPreviewPage->setCurrentWorkPath(root);
                    LOG_INFO(QString("设置视频预览页面根路径: %1").arg(root));
                }
            }

            setCurrentWidget(page);
            m_currentPageType = pageType;
            LOG_INFO("立即切换到视频预览页面");
            emit pageChanged(pageType);
            return;
        }
        // 如果切换到3D测量页面
        else if (pageType == PageType::Measurement) {
            LOG_INFO("请求切换到3D测量页面 - 立即切换，异步准备图像");

            // 立即切换到测量页面
            setCurrentWidget(page);
            m_currentPageType = pageType;
            LOG_INFO("立即切换到3D测量页面");
            emit pageChanged(pageType);

            // 异步准备图像，避免阻塞页面切换
            QTimer::singleShot(0, this, [this]() {
                bool imagesReady = captureCameraImagesForMeasurement();
                if (!imagesReady) {
                    LOG_WARNING("异步图像准备失败，但页面已切换");
                    // 可以在这里显示一个提示，告知用户图像准备失败
                    // 但不影响页面切换的响应性
                }
            });

            return; // 处理完毕，直接返回
        }

        // 其他页面直接切换（若切换到调试页需校验开关）
        if (pageType == PageType::Debug) {
            bool dbg = SmartScope::Infrastructure::ConfigManager::instance().getValue("ui/debug_mode", false).toBool();
            if (!dbg) {
                showToast(this, "请在设置页启用调试模式后再进入调试界面", 2000);
                LOG_WARNING("调试模式未启用，阻止进入调试页面");
                return;
            }
            // 设置测量页保留标志，以便返回后继续显示
            if (auto mp = getMeasurementPage()) {
                mp->setPreserveOnHide(true);
                mp->setSkipClearOnNextShow(true);
            }
        }
        setCurrentWidget(page);
        m_currentPageType = pageType;

        // 记录当前页面名称
        QString pageName;
        switch (pageType) {
            case PageType::Home:
                pageName = "主页";
                break;
            case PageType::Preview:
                pageName = "预览";
                break;
            case PageType::Report:
                pageName = "报告";
                break;
            case PageType::Settings:
                pageName = "参数设置";
                break;
            // 注释页面已移除
            // Measurement case is handled above
            // case PageType::Measurement:
            //     pageName = "3D测量";
            //     break;
            default:
                pageName = "未知";
                break;
        }

        LOG_INFO(QString("切换到页面: %1").arg(pageName));

        // 发送页面变化信号
        emit pageChanged(pageType);
    } else {
        LOG_WARNING(QString("尝试切换到不存在的页面类型: %1").arg(static_cast<int>(pageType)));
    }
}

PageType PageManager::getCurrentPageType() const
{
    return m_currentPageType;
}

bool PageManager::captureCameraImagesForMeasurement()
{
    // 获取主页实例
    HomePage *homePage = getHomePage();
    if (!homePage) {
        LOG_WARNING("无法获取主页实例，无法准备测量图像。");
        return false;
    }
    
    // 获取测量页面实例
    MeasurementPage *measurementPage = getMeasurementPage();
    if (!measurementPage) {
        LOG_WARNING("无法获取3D测量页面实例，无法准备测量图像。");
        return false;
    }
    
    // 获取相机管理器
    using namespace SmartScope::Core::CameraUtils;
    MultiCameraManager& cameraManager = MultiCameraManager::instance();
    
    const int maxRetries = 10;        // 最大重试次数
    const int retryDelayMs = 100;     // 重试间隔（毫秒）
    const int timeoutMs = 5000;       // 总超时时间（毫秒）
    LOG_INFO(QString("开始尝试获取双目同步图像 (超时时间: %1 ms, 最大重试次数: %2)...").arg(timeoutMs).arg(maxRetries));
    
    QElapsedTimer timer;
    timer.start();

    int retryCount = 0;
    while (retryCount < maxRetries && timer.elapsed() < timeoutMs) {
        std::map<std::string, cv::Mat> frames;
        std::map<std::string, int64_t> timestamps;
        bool success = false;
        cv::Mat leftImage, rightImage;
        bool leftFound = false;
        bool rightFound = false;

        // 1. 尝试获取同步帧
        success = cameraManager.getSyncFrames(frames, timestamps, 50, SyncMode::LOW_LATENCY);

        if (!success) {
            LOG_DEBUG(QString("第 %1 次获取同步帧失败，%2 ms后重试...").arg(retryCount + 1).arg(retryDelayMs));
            retryCount++;
            if (timer.elapsed() + retryDelayMs >= timeoutMs) {
                LOG_WARNING("剩余时间不足，即将超时");
                break;
            }
            QThread::msleep(retryDelayMs);
            continue;
        }

        // 2. 验证图像是否为空
        if (frames.empty()) {
            LOG_DEBUG(QString("第 %1 次获取同步帧成功，但返回的帧集合为空，%2 ms后重试...").arg(retryCount + 1).arg(retryDelayMs));
            retryCount++;
            if (timer.elapsed() + retryDelayMs >= timeoutMs) {
                LOG_WARNING("剩余时间不足，即将超时");
                break; 
            }
            QThread::msleep(retryDelayMs);
            continue;
        }

        // 3. 获取相机ID
        QString leftCameraId = homePage->getLeftCameraId();
        QString rightCameraId = homePage->getRightCameraId();

        if (leftCameraId.isEmpty() || rightCameraId.isEmpty()) {
            LOG_ERROR(QString("相机ID为空，无法查找图像。请检查相机配置。第 %1 次尝试失败。").arg(retryCount + 1));
            retryCount++;
            if (timer.elapsed() + retryDelayMs >= timeoutMs) {
                LOG_WARNING("剩余时间不足，即将超时");
                break; 
            }
            QThread::msleep(retryDelayMs);
            continue;
        }

        // 4. 查找左右相机图像
        auto leftIt = frames.find(leftCameraId.toStdString());
        if (leftIt != frames.end() && !leftIt->second.empty()) {
            leftImage = leftIt->second.clone();
            leftFound = true;
        }

        auto rightIt = frames.find(rightCameraId.toStdString());
        if (rightIt != frames.end() && !rightIt->second.empty()) {
            rightImage = rightIt->second.clone();
            rightFound = true;
        }

        // 5. 验证是否获取到两个相机的图像
        if (!leftFound || !rightFound) {
            LOG_DEBUG(QString("第 %1 次尝试：未获取到完整的双目图像（左：%2，右：%3），%4 ms后重试...")
                .arg(retryCount + 1)
                .arg(leftFound ? "成功" : "失败")
                .arg(rightFound ? "成功" : "失败")
                .arg(retryDelayMs));
            retryCount++;
            if (timer.elapsed() + retryDelayMs >= timeoutMs) {
                LOG_WARNING("剩余时间不足，即将超时");
                break; 
            }
            QThread::msleep(retryDelayMs);
            continue;
        }

        // 6. 成功获取到双目图像
        LOG_INFO(QString("成功获取双目同步图像（尝试次数：%1，耗时：%2 ms）").arg(retryCount + 1).arg(timer.elapsed()));
        measurementPage->setImagesFromHomePage(leftImage, rightImage);
        return true;
    }

    // 如果所有重试都失败了，记录日志但不显示错误提示
    LOG_WARNING(QString("获取双目同步图像失败（尝试次数：%1，耗时：%2 ms）").arg(retryCount).arg(timer.elapsed()));
    return false;
}

HomePage* PageManager::getHomePage() const
{
    return qobject_cast<HomePage*>(m_pages.value(PageType::Home));
}

MeasurementPage* PageManager::getMeasurementPage() const
{
    return qobject_cast<MeasurementPage*>(m_pages.value(PageType::Measurement));
}

SmartScope::App::Ui::DebugPage* PageManager::getDebugPage() const
{
    return qobject_cast<SmartScope::App::Ui::DebugPage*>(m_pages.value(PageType::Debug));
}

SettingsPage* PageManager::getSettingsPage() const
{
    return qobject_cast<SettingsPage*>(m_pages.value(PageType::Settings));
} 