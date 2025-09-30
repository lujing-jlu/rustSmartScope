#ifndef INFERENCE_SERVICE_HPP
#define INFERENCE_SERVICE_HPP

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <opencv2/opencv.hpp>
#include "inference/stereo_depth_inference.hpp"
#include "stereo_depth/comprehensive_depth_processor.hpp"
#include "inference/stereo_depth_engine.hpp"

namespace SmartScope {

// 前置声明：统一引擎（集中初始化相机标定/Q/综合处理器）
class StereoDepthEngine;

struct InferenceRequest {
    cv::Mat left_image;
    cv::Mat right_image;
    QString save_path;
    bool generate_pointcloud;
    float baseline;
    float focal_length;
    int original_width = 0;   // 原始图像宽度
    int original_height = 0;  // 原始图像高度
    qint64 session_id = 0;    // 会话ID，用于标识请求属于哪个会话
    // 进入3D测量时的中心裁剪：将图1/图2裁剪为高:宽=4:3，用于后续 mono 与显示
    bool apply_43_crop = false;   // 是否启用4:3裁剪流程
    cv::Rect crop_roi;            // UI侧计算的中心裁剪ROI（相对于原始校正后图像）
    
    // 点云后处理参数
    bool apply_filter = false;          // 是否应用点云过滤
    bool apply_optimize = false;        // 是否应用点云优化
    // PointCloudFilterParams filter_params; // 点云过滤参数 - 暂时注释
    double optimize_threshold = 0.02;   // 平面拟合距离阈值
    bool project_to_plane = false;      // 是否将点投影到平面上
};

struct InferenceResult {
    cv::Mat depth_map;                    // 双目深度图
    cv::Mat mono_depth_raw;               // 单目原始深度图
    cv::Mat mono_depth_calibrated;        // 校准后的单目深度图
    cv::Mat disparity_map;                // 视差图
    cv::Mat confidence_map;               // 置信度图
    QString save_path;
    bool success;
    QString error_message;
    int original_width = 0;   // 原始图像宽度
    int original_height = 0;  // 原始图像高度
    qint64 session_id = 0;    // 会话ID，用于标识结果属于哪个会话
    
    // 点云后处理结果
    QString pointcloud_path;          // 原始点云路径
    QString filtered_pointcloud_path; // 过滤后点云路径
    QString optimized_pointcloud_path; // 优化后点云路径
    bool filter_success = false;      // 过滤是否成功
    bool optimize_success = false;    // 优化是否成功
    
    // 深度校准结果
    double calibration_scale = 1.0;
    double calibration_bias = 0.0;
    bool calibration_success = false;
};

class InferenceService : public QObject {
    Q_OBJECT

public:
    static InferenceService& instance();
    
    // 初始化服务
    bool initialize(const QString& model_path);
    
    // 设置和获取性能模式
    void setPerformanceMode(StereoDepthInference::PerformanceMode mode);
    StereoDepthInference::PerformanceMode getPerformanceMode() const;
    
    // 提交推理请求
    void submitRequest(const InferenceRequest& request);
    
    // 取消当前正在进行的推理任务
    void cancelCurrentTask();
    
    // 完全重置服务状态，清空所有请求和结果
    void resetService();
    
    // 获取当前会话ID
    qint64 getCurrentSessionId() const;
    
    // 重置会话ID，返回新的会话ID
    qint64 resetSessionId();
    
    // 停止服务
    void stop();
    
    // 安全关闭服务并释放资源
    void shutdown();
    
    // 获取服务状态
    bool isRunning() const;
    bool isInitialized() const;
    
    // 获取推理实例指针，仅供同步预处理使用
    StereoDepthInference* getInferencePtr() const;
    
    // 获取综合深度处理器指针
    stereo_depth::ComprehensiveDepthProcessor* getComprehensiveProcessor() const;
    // 新增：获取统一引擎（后续逐步切换到从引擎获取资源）
    StereoDepthEngine* getStereoDepthEngine() const { return m_engine.get(); }
    
    // 设置深度模式
    enum class DepthMode {
        STEREO_ONLY,      // 仅双目深度
        MONO_CALIBRATED   // 校准后的单目深度
    };
    void setDepthMode(DepthMode mode);
    DepthMode getDepthMode() const;

signals:
    // 推理完成信号
    void inferenceCompleted(const InferenceResult& result);
    
    // 内部信号
    void newRequestAvailable();

private slots:
    void processRequest();

private:
    InferenceService();
    ~InferenceService();
    InferenceService(const InferenceService&) = delete;
    InferenceService& operator=(const InferenceService&) = delete;

    // 推理线程函数
    void inferenceThread();
    
    // 成员变量
    QThread m_workerThread;
    mutable QRecursiveMutex m_mutex;
    QWaitCondition m_condition;
    QQueue<InferenceRequest> m_requestQueue;
    std::unique_ptr<StereoDepthInference> m_inference;
    std::unique_ptr<stereo_depth::ComprehensiveDepthProcessor> m_comprehensiveProcessor;
    // 新增：统一引擎实例
    std::unique_ptr<StereoDepthEngine> m_engine;
    bool m_running;
    bool m_initialized;
    qint64 m_currentSessionId;  // 当前会话ID
    DepthMode m_depthMode = DepthMode::STEREO_ONLY;  // 深度模式
    
    // 日志函数
    void logInfo(const QString& message) const;
    void logError(const QString& message) const;
    void logWarning(const QString& message) const;
};

} // namespace SmartScope

#endif // INFERENCE_SERVICE_HPP 