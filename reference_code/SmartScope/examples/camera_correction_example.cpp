/**
 * @file camera_correction_example.cpp
 * @brief 相机校正管理器使用示例
 * 
 * 本示例展示了如何使用CameraCorrectionManager进行相机校正
 */

#include "core/camera/camera_correction_manager.h"
#include "core/camera/camera_correction_factory.h"
#include "infrastructure/logging/logger.h"
#include <opencv2/opencv.hpp>
#include <QApplication>
#include <QDebug>
#include <QTimer>

using namespace SmartScope::Core;

class CorrectionExample : public QObject
{
    Q_OBJECT

public:
    CorrectionExample(QObject* parent = nullptr) : QObject(parent) {}

    void runExample()
    {
        LOG_INFO("Starting camera correction example...");

        // 示例1: 使用工厂创建标准校正管理器
        example1_StandardCorrection();

        // 示例2: 使用工厂创建快速校正管理器
        example2_FastCorrection();

        // 示例3: 自定义配置校正管理器
        example3_CustomCorrection();

        // 示例4: 图像变换功能
        example4_ImageTransforms();

        // 示例5: 深度校准功能
        example5_DepthCalibration();

        LOG_INFO("Camera correction example completed");
    }

private:
    void example1_StandardCorrection()
    {
        LOG_INFO("=== Example 1: Standard Correction ===");

        // 创建标准校正管理器
        auto manager = CameraCorrectionFactory::createStandardCorrectionManager(this);
        if (!manager) {
            LOG_ERROR("Failed to create standard correction manager");
            return;
        }

        // 连接信号
        connect(manager.get(), &CameraCorrectionManager::correctionCompleted,
                this, &CorrectionExample::onCorrectionCompleted);
        connect(manager.get(), &CameraCorrectionManager::correctionError,
                this, &CorrectionExample::onCorrectionError);

        // 创建测试图像
        cv::Mat leftImage = cv::Mat::zeros(720, 1280, CV_8UC3);
        cv::Mat rightImage = cv::Mat::zeros(720, 1280, CV_8UC3);
        
        // 添加一些测试内容
        cv::rectangle(leftImage, cv::Point(100, 100), cv::Point(300, 300), cv::Scalar(255, 0, 0), -1);
        cv::rectangle(rightImage, cv::Point(100, 100), cv::Point(300, 300), cv::Scalar(0, 255, 0), -1);

        // 执行完整校正
        CorrectionResult result = manager->correctImages(leftImage, rightImage, CorrectionType::ALL);
        
        if (result.success) {
            LOG_INFO(QString("Standard correction successful, processing time: %1ms")
                .arg(result.processingTimeMs));
            
            // 保存结果图像
            cv::imwrite("corrected_left_standard.jpg", result.correctedLeftImage);
            cv::imwrite("corrected_right_standard.jpg", result.correctedRightImage);
        } else {
            LOG_ERROR(QString("Standard correction failed: %1").arg(result.errorMessage));
        }
    }

    void example2_FastCorrection()
    {
        LOG_INFO("=== Example 2: Fast Correction ===");

        // 创建快速校正管理器
        auto manager = CameraCorrectionFactory::createFastCorrectionManager(this);
        if (!manager) {
            LOG_ERROR("Failed to create fast correction manager");
            return;
        }

        // 创建测试图像
        cv::Mat leftImage = cv::Mat::zeros(720, 1280, CV_8UC3);
        cv::Mat rightImage = cv::Mat::zeros(720, 1280, CV_8UC3);

        // 仅执行畸变校正
        CorrectionResult result = manager->correctImages(
            leftImage, rightImage, 
            CorrectionType::DISTORTION | CorrectionType::STEREO_RECTIFICATION
        );
        
        if (result.success) {
            LOG_INFO(QString("Fast correction successful, processing time: %1ms")
                .arg(result.processingTimeMs));
        } else {
            LOG_ERROR(QString("Fast correction failed: %1").arg(result.errorMessage));
        }
    }

    void example3_CustomCorrection()
    {
        LOG_INFO("=== Example 3: Custom Correction ===");

        // 创建自定义配置
        CorrectionConfig config;
        config.cameraParametersPath = "./camera_parameters";
        config.imageSize = cv::Size(1280, 720);
        config.enableDistortionCorrection = true;
        config.enableStereoRectification = true;
        config.enableDepthCalibration = false;  // 禁用深度校准
        config.enableImageTransform = true;
        
        // 设置图像变换参数
        config.rotationDegrees = 90;
        config.flipHorizontal = true;
        config.zoomScale = 1.2f;

        // 创建自定义校正管理器
        auto manager = CameraCorrectionFactory::createCustomCorrectionManager(config, this);
        if (!manager) {
            LOG_ERROR("Failed to create custom correction manager");
            return;
        }

        // 创建测试图像
        cv::Mat leftImage = cv::Mat::zeros(720, 1280, CV_8UC3);
        cv::Mat rightImage = cv::Mat::zeros(720, 1280, CV_8UC3);

        // 执行校正
        CorrectionResult result = manager->correctImages(leftImage, rightImage, CorrectionType::ALL);
        
        if (result.success) {
            LOG_INFO(QString("Custom correction successful, processing time: %1ms")
                .arg(result.processingTimeMs));
        } else {
            LOG_ERROR(QString("Custom correction failed: %1").arg(result.errorMessage));
        }
    }

    void example4_ImageTransforms()
    {
        LOG_INFO("=== Example 4: Image Transforms ===");

        auto manager = CameraCorrectionFactory::createStandardCorrectionManager(this);
        if (!manager) {
            LOG_ERROR("Failed to create correction manager for transforms");
            return;
        }

        // 创建测试图像
        cv::Mat testImage = cv::Mat::zeros(480, 640, CV_8UC3);
        cv::rectangle(testImage, cv::Point(100, 100), cv::Point(200, 200), cv::Scalar(255, 255, 255), -1);

        // 测试各种变换
        std::vector<RGATransform> transforms = {
            RGATransform::Rotate90,
            RGATransform::Rotate180,
            RGATransform::Rotate270,
            RGATransform::FlipHorizontal,
            RGATransform::FlipVertical,
            RGATransform::Invert,
            RGATransform::Scale2X,
            RGATransform::ScaleHalf
        };

        for (size_t i = 0; i < transforms.size(); ++i) {
            cv::Mat transformed = manager->applyImageTransform(testImage, transforms[i]);
            if (!transformed.empty()) {
                QString filename = QString("transform_%1.jpg").arg(i);
                cv::imwrite(filename.toStdString(), transformed);
                LOG_INFO(QString("Applied transform %1, saved as %2")
                    .arg(static_cast<int>(transforms[i]))
                    .arg(filename));
            }
        }
    }

    void example5_DepthCalibration()
    {
        LOG_INFO("=== Example 5: Depth Calibration ===");

        auto manager = CameraCorrectionFactory::createFullCorrectionManager(this);
        if (!manager) {
            LOG_ERROR("Failed to create correction manager for depth calibration");
            return;
        }

        // 创建模拟深度数据
        cv::Mat monoDepth = cv::Mat::zeros(480, 640, CV_32F);
        cv::Mat stereoDepth = cv::Mat::zeros(480, 640, CV_32F);
        cv::Mat disparity = cv::Mat::zeros(480, 640, CV_32F);

        // 填充一些测试数据
        for (int y = 0; y < monoDepth.rows; ++y) {
            for (int x = 0; x < monoDepth.cols; ++x) {
                float depth = 1000.0f + (x + y) * 2.0f;  // 模拟深度值
                monoDepth.at<float>(y, x) = depth;
                stereoDepth.at<float>(y, x) = depth * 1.1f + 50.0f;  // 模拟双目深度
                disparity.at<float>(y, x) = 1000.0f / depth;  // 模拟视差
            }
        }

        // 执行深度校准
        auto depthResult = manager->calibrateDepth(monoDepth, stereoDepth, disparity);
        
        if (depthResult.success) {
            LOG_INFO(QString("Depth calibration successful:")
                .append(QString("\n  Scale factor: %1").arg(depthResult.scale_factor))
                .append(QString("\n  Bias: %1").arg(depthResult.bias))
                .append(QString("\n  RMS error: %1").arg(depthResult.rms_error))
                .append(QString("\n  Inlier points: %1/%2")
                    .arg(depthResult.inlier_points)
                    .arg(depthResult.total_points)));
        } else {
            LOG_ERROR("Depth calibration failed");
        }
    }

private slots:
    void onCorrectionCompleted(const CorrectionResult& result)
    {
        LOG_INFO(QString("Correction completed signal received, success: %1, time: %2ms")
            .arg(result.success)
            .arg(result.processingTimeMs));
    }

    void onCorrectionError(const QString& errorMessage)
    {
        LOG_ERROR(QString("Correction error signal received: %1").arg(errorMessage));
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 初始化日志系统
    SmartScope::Infrastructure::Logger::instance().init("", SmartScope::Infrastructure::LogLevel::INFO, true, false);
    
    // 运行示例
    CorrectionExample example;
    example.runExample();
    
    return 0;
}

#include "camera_correction_example.moc"
