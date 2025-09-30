/**
 * @file integration_example.cpp
 * @brief 相机校正管理器集成示例
 * 
 * 展示如何在现有的SmartScope代码中集成CameraCorrectionManager
 */

#include "core/camera/camera_correction_manager.h"
#include "core/camera/camera_correction_factory.h"
#include "app/ui/home_page.h"
#include "infrastructure/logging/logger.h"
#include <QApplication>
#include <QDebug>

using namespace SmartScope::Core;
using namespace SmartScope::Infrastructure;

/**
 * @brief 集成示例类
 * 
 * 展示如何在HomePage中集成相机校正管理器
 */
class IntegratedHomePage : public SmartScope::HomePage
{
    Q_OBJECT

public:
    IntegratedHomePage(QWidget *parent = nullptr) 
        : SmartScope::HomePage(parent)
        , m_correctionManager(nullptr)
    {
        initializeCorrectionManager();
    }

    ~IntegratedHomePage() override = default;

protected:
    /**
     * @brief 重写图像更新方法，集成校正功能
     */
    void updateCameraFrames() override
    {
        // 获取原始相机图像
        cv::Mat leftFrame, rightFrame;
        if (!getCurrentFrames(leftFrame, rightFrame)) {
            return;
        }

        // 使用校正管理器处理图像
        if (m_correctionManager && m_correctionManager->isInitialized()) {
            CorrectionResult result = m_correctionManager->correctImages(
                leftFrame, rightFrame, 
                CorrectionType::DISTORTION | CorrectionType::STEREO_RECTIFICATION
            );

            if (result.success) {
                // 使用校正后的图像
                leftFrame = result.correctedLeftImage;
                rightFrame = result.correctedRightImage;
                
                LOG_DEBUG(QString("Images corrected successfully in %1ms")
                    .arg(result.processingTimeMs));
            } else {
                LOG_WARNING(QString("Image correction failed: %1").arg(result.errorMessage));
            }
        }

        // 调用父类的更新方法
        SmartScope::HomePage::updateCameraFrames();
    }

    /**
     * @brief 重写截图方法，保存校正后的图像
     */
    void captureAndSaveImages() override
    {
        cv::Mat leftFrame, rightFrame;
        if (!getCurrentFrames(leftFrame, rightFrame)) {
            return;
        }

        // 应用校正
        if (m_correctionManager && m_correctionManager->isInitialized()) {
            CorrectionResult result = m_correctionManager->correctImages(
                leftFrame, rightFrame, CorrectionType::ALL
            );

            if (result.success) {
                leftFrame = result.correctedLeftImage;
                rightFrame = result.correctedRightImage;
            }
        }

        // 保存图像
        saveCorrectedImages(leftFrame, rightFrame);
    }

    /**
     * @brief 设置图像变换参数
     */
    void setImageTransformParams(
        int rotationDegrees = 0,
        bool flipHorizontal = false,
        bool flipVertical = false,
        bool invertColors = false,
        float zoomScale = 1.0f)
    {
        if (m_correctionManager) {
            m_correctionManager->setImageTransformParams(
                rotationDegrees, flipHorizontal, flipVertical, invertColors, zoomScale
            );
            LOG_INFO(QString("Image transform params updated: rotation=%1°, flipH=%2, flipV=%3, invert=%4, zoom=%5")
                .arg(rotationDegrees)
                .arg(flipHorizontal)
                .arg(flipVertical)
                .arg(invertColors)
                .arg(zoomScale));
        }
    }

    /**
     * @brief 重置图像变换
     */
    void resetImageTransforms()
    {
        if (m_correctionManager) {
            m_correctionManager->resetImageTransforms();
            LOG_INFO("Image transforms reset to default");
        }
    }

    /**
     * @brief 获取校正统计信息
     */
    QString getCorrectionStatistics() const
    {
        if (m_correctionManager) {
            return m_correctionManager->getCorrectionStatistics();
        }
        return "Correction manager not available";
    }

private:
    /**
     * @brief 初始化校正管理器
     */
    void initializeCorrectionManager()
    {
        LOG_INFO("Initializing camera correction manager...");

        // 创建校正配置
        CorrectionConfig config;
        config.cameraParametersPath = "./camera_parameters";
        config.imageSize = cv::Size(1280, 720);
        config.enableDistortionCorrection = true;
        config.enableStereoRectification = true;
        config.enableDepthCalibration = true;
        config.enableImageTransform = true;
        config.useHardwareAcceleration = true;
        config.precomputeMaps = true;

        // 创建校正管理器
        m_correctionManager = CameraCorrectionFactory::createCustomCorrectionManager(config, this);
        
        if (m_correctionManager) {
            // 连接信号
            connect(m_correctionManager.get(), &CameraCorrectionManager::correctionCompleted,
                    this, &IntegratedHomePage::onCorrectionCompleted);
            connect(m_correctionManager.get(), &CameraCorrectionManager::correctionError,
                    this, &IntegratedHomePage::onCorrectionError);
            connect(m_correctionManager.get(), &CameraCorrectionManager::correctionProgress,
                    this, &IntegratedHomePage::onCorrectionProgress);

            LOG_INFO("Camera correction manager initialized successfully");
        } else {
            LOG_ERROR("Failed to initialize camera correction manager");
        }
    }

    /**
     * @brief 获取当前相机帧
     */
    bool getCurrentFrames(cv::Mat& leftFrame, cv::Mat& rightFrame)
    {
        // 这里应该从相机管理器获取当前帧
        // 示例实现
        if (m_leftFrame.empty() || m_rightFrame.empty()) {
            return false;
        }
        
        leftFrame = m_leftFrame.clone();
        rightFrame = m_rightFrame.clone();
        return true;
    }

    /**
     * @brief 保存校正后的图像
     */
    void saveCorrectedImages(const cv::Mat& leftFrame, const cv::Mat& rightFrame)
    {
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString leftPath = QString("captures/left_%1.jpg").arg(timestamp);
        QString rightPath = QString("captures/right_%1.jpg").arg(timestamp);

        cv::imwrite(leftPath.toStdString(), leftFrame);
        cv::imwrite(rightPath.toStdString(), rightFrame);

        LOG_INFO(QString("Corrected images saved: %1, %2").arg(leftPath).arg(rightPath));
    }

private slots:
    void onCorrectionCompleted(const CorrectionResult& result)
    {
        LOG_DEBUG(QString("Correction completed: success=%1, time=%2ms, corrections=%3")
            .arg(result.success)
            .arg(result.processingTimeMs)
            .arg(static_cast<int>(result.appliedCorrections)));
    }

    void onCorrectionError(const QString& errorMessage)
    {
        LOG_ERROR(QString("Correction error: %1").arg(errorMessage));
    }

    void onCorrectionProgress(int progress)
    {
        LOG_DEBUG(QString("Correction progress: %1%").arg(progress));
    }

private:
    std::shared_ptr<CameraCorrectionManager> m_correctionManager;
    cv::Mat m_leftFrame, m_rightFrame;  // 示例：存储当前帧
};

/**
 * @brief 简化的集成示例
 */
class SimpleIntegrationExample : public QObject
{
    Q_OBJECT

public:
    void runExample()
    {
        LOG_INFO("Running simple integration example...");

        // 1. 创建校正管理器
        auto manager = CameraCorrectionFactory::createStandardCorrectionManager(this);
        if (!manager) {
            LOG_ERROR("Failed to create correction manager");
            return;
        }

        // 2. 连接信号
        connect(manager.get(), &CameraCorrectionManager::correctionCompleted,
                this, &SimpleIntegrationExample::onCorrectionCompleted);

        // 3. 模拟相机数据
        cv::Mat leftImage = cv::Mat::zeros(720, 1280, CV_8UC3);
        cv::Mat rightImage = cv::Mat::zeros(720, 1280, CV_8UC3);

        // 4. 执行校正
        CorrectionResult result = manager->correctImages(leftImage, rightImage, CorrectionType::ALL);
        
        if (result.success) {
            LOG_INFO(QString("Correction successful, processing time: %1ms")
                .arg(result.processingTimeMs));
        } else {
            LOG_ERROR(QString("Correction failed: %1").arg(result.errorMessage));
        }

        // 5. 显示统计信息
        LOG_INFO(manager->getCorrectionStatistics());
    }

private slots:
    void onCorrectionCompleted(const CorrectionResult& result)
    {
        LOG_INFO(QString("Signal received: correction completed in %1ms")
            .arg(result.processingTimeMs));
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 初始化日志系统
    Logger::instance().init("", LogLevel::INFO, true, false);
    
    // 运行简单集成示例
    SimpleIntegrationExample example;
    example.runExample();
    
    return 0;
}

#include "integration_example.moc"
