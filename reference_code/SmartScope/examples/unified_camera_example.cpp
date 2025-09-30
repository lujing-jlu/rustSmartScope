/**
 * @file unified_camera_example.cpp
 * @brief 统一相机管理器使用示例
 */

#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <opencv2/opencv.hpp>

#include "core/camera/unified_camera_manager.h"
#include "infrastructure/config/config_manager.h"

using namespace SmartScope::Core::CameraUtils;
using namespace SmartScope::Infrastructure;

class CameraExample : public QObject
{
    Q_OBJECT

public:
    CameraExample(QObject* parent = nullptr)
        : QObject(parent)
        , m_manager(UnifiedCameraManager::instance())
        , m_frameCount(0)
        , m_syncFrameCount(0)
    {
        // 连接关键信号
        connectSignals();

        // 初始化
        QTimer::singleShot(100, this, &CameraExample::initialize);
    }

private slots:
    void initialize()
    {
        qDebug() << "初始化统一相机管理器示例";

        // 初始化配置管理器
        if (!ConfigManager::instance().initialize()) {
            qDebug() << "配置管理器初始化失败";
            QCoreApplication::quit();
            return;
        }

        // 初始化统一相机管理器
        if (!m_manager.initialize()) {
            qDebug() << "统一相机管理器初始化失败";
            QCoreApplication::quit();
            return;
        }

        qDebug() << "初始化成功，开始监控相机状态...";
    }

    void onOperationModeChanged(CameraOperationMode newMode, CameraOperationMode oldMode)
    {
        Q_UNUSED(oldMode)

        switch (newMode) {
            case CameraOperationMode::NO_CAMERA:
                handleNoCameraMode();
                break;

            case CameraOperationMode::SINGLE_CAMERA:
                handleSingleCameraMode();
                break;

            case CameraOperationMode::DUAL_CAMERA:
                handleDualCameraMode();
                break;
        }
    }

    void handleNoCameraMode()
    {
        qDebug() << "进入无相机模式 - 请连接相机设备";
        // 在实际应用中，这里可以：
        // 1. 显示"请连接相机"提示界面
        // 2. 禁用所有相机相关功能
        // 3. 显示设备连接指导
    }

    void handleSingleCameraMode()
    {
        qDebug() << "进入单相机模式 - 基础功能可用";
        // 在实际应用中，这里可以：
        // 1. 启用基础2D功能
        // 2. 禁用双目3D功能
        // 3. 显示单相机模式提示
        // 4. 调整UI布局为单相机显示
    }

    void handleDualCameraMode()
    {
        qDebug() << "进入双相机模式 - 全功能可用";
        // 在实际应用中，这里可以：
        // 1. 启用所有2D和3D功能
        // 2. 显示双相机画面
        // 3. 启用深度测量功能
        // 4. 调整UI布局为双相机显示
    }

    void onNewFrame(const QString& deviceId, const cv::Mat& frame, int64_t timestamp)
    {
        Q_UNUSED(timestamp)

        m_frameCount++;

        // 每100帧处理一次，避免过于频繁
        if (m_frameCount % 100 == 0) {
            qDebug() << "处理单相机帧:" << deviceId
                    << "分辨率:" << frame.cols << "x" << frame.rows
                    << "总帧数:" << m_frameCount;

            // 在实际应用中，这里可以：
            // 1. 显示图像到UI
            // 2. 进行图像处理
            // 3. 执行2D检测算法
            // 4. 更新图像统计信息

            // 示例：简单的图像处理
            processFrameBasic(frame, deviceId);
        }
    }

    void onNewSyncFrames(const cv::Mat& leftFrame, const cv::Mat& rightFrame, int64_t timestamp)
    {
        Q_UNUSED(timestamp)

        m_syncFrameCount++;

        // 每100帧处理一次
        if (m_syncFrameCount % 100 == 0) {
            qDebug() << "处理双相机同步帧:"
                    << "左:" << leftFrame.cols << "x" << leftFrame.rows
                    << "右:" << rightFrame.cols << "x" << rightFrame.rows
                    << "同步帧数:" << m_syncFrameCount;

            // 在实际应用中，这里可以：
            // 1. 显示左右图像到UI
            // 2. 计算视差图
            // 3. 进行3D重建
            // 4. 执行双目测量
            // 5. 运行立体视觉算法

            // 示例：简单的双目处理
            processStereoFrames(leftFrame, rightFrame);
        }
    }

    void onLeftCameraConnected(const QString& deviceId, const QString& deviceName)
    {
        qDebug() << "左相机已连接:" << deviceName << "(" << deviceId << ")";
        // 可以在这里初始化左相机特定的处理逻辑
    }

    void onRightCameraConnected(const QString& deviceId, const QString& deviceName)
    {
        qDebug() << "右相机已连接:" << deviceName << "(" << deviceId << ")";
        // 可以在这里初始化右相机特定的处理逻辑
    }

private:
    void connectSignals()
    {
        // 连接核心信号
        connect(&m_manager, &UnifiedCameraManager::operationModeChanged,
                this, &CameraExample::onOperationModeChanged);

        connect(&m_manager, &UnifiedCameraManager::newFrame,
                this, &CameraExample::onNewFrame);

        connect(&m_manager, &UnifiedCameraManager::newSyncFrames,
                this, &CameraExample::onNewSyncFrames);

        connect(&m_manager, &UnifiedCameraManager::leftCameraConnected,
                this, &CameraExample::onLeftCameraConnected);

        connect(&m_manager, &UnifiedCameraManager::rightCameraConnected,
                this, &CameraExample::onRightCameraConnected);
    }

    void processFrameBasic(const cv::Mat& frame, const QString& deviceId)
    {
        if (frame.empty()) {
            return;
        }

        // 示例：简单的图像分析
        cv::Scalar meanColor = cv::mean(frame);
        qDebug() << "相机" << deviceId << "平均亮度:" << meanColor[0];

        // 在实际应用中，这里可以添加：
        // 1. 图像增强
        // 2. 特征检测
        // 3. 对象识别
        // 4. 质量评估
        // 等单目视觉处理逻辑
    }

    void processStereoFrames(const cv::Mat& leftFrame, const cv::Mat& rightFrame)
    {
        if (leftFrame.empty() || rightFrame.empty()) {
            return;
        }

        // 示例：简单的双目分析
        cv::Scalar leftMean = cv::mean(leftFrame);
        cv::Scalar rightMean = cv::mean(rightFrame);

        qDebug() << "双目帧分析 - 左相机亮度:" << leftMean[0]
                << "右相机亮度:" << rightMean[0];

        // 在实际应用中，这里可以添加：
        // 1. 图像矫正
        // 2. 立体匹配
        // 3. 视差计算
        // 4. 深度估算
        // 5. 3D重建
        // 6. 双目测量
        // 等立体视觉处理逻辑
    }

private:
    UnifiedCameraManager& m_manager;
    int m_frameCount;
    int m_syncFrameCount;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "统一相机管理器使用示例";
    qDebug() << "这个示例展示了如何使用统一相机管理器";
    qDebug() << "支持无相机/单相机/双相机三种模式的自动切换";

    CameraExample example;

    // 5分钟后自动退出
    QTimer::singleShot(300000, &app, &QCoreApplication::quit);

    return app.exec();
}

#include "unified_camera_example.moc"