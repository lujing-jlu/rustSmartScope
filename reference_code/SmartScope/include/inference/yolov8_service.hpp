#ifndef YOLOV8_SERVICE_HPP
#define YOLOV8_SERVICE_HPP

#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <QThread>
#include <QSemaphore>
#include <QAtomicInteger>
#include <QThreadPool>
#include <QRunnable>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <map>

// 前向声明
class YOLOv8Detector;

namespace SmartScope {

// YOLOv8检测结果结构体
struct YOLOv8Result {
    struct Detection {
        std::string class_name;    // 目标类别名称
        float confidence;          // 置信度
        cv::Rect box;              // 边界框
    };
    
    cv::Mat image;                 // 原始图像
    cv::Mat result_image;          // 带检测结果的图像
    std::vector<Detection> detections; // 检测结果列表
    bool success;                  // 是否成功
    QString error_message;         // 错误信息
    qint64 session_id;             // 会话ID
    qint64 request_id;             // 请求ID（用于排序）
};

// YOLOv8检测请求结构体
struct YOLOv8Request {
    cv::Mat image;                 // 待检测图像
    QString save_path;             // 结果保存路径
    qint64 session_id;             // 会话ID
    qint64 request_id;             // 请求ID（用于排序）
    float confidence_threshold;    // 置信度阈值，默认0.25
    float nms_threshold;           // NMS阈值，默认0.45
};

// 前向声明
class YOLOv8Service;

// 检测任务类（用于线程池）
class DetectionTask : public QObject, public QRunnable {
    Q_OBJECT
public:
    DetectionTask(const YOLOv8Request& request, YOLOv8Detector* detector, YOLOv8Service* service);
    void run() override;

signals:
    void taskCompleted(const YOLOv8Result& result);

private:
    YOLOv8Request m_request;
    YOLOv8Detector* m_detector;
    YOLOv8Service* m_service;
};

// 结果排序管理器
class ResultOrderManager : public QObject {
    Q_OBJECT
public:
    ResultOrderManager(QObject* parent = nullptr);
    void addResult(const YOLOv8Result& result);
    void setExpectedOrder(qint64 requestId);

signals:
    void orderedResultReady(const YOLOv8Result& result);

private slots:
    void checkPendingResults();

private:
    std::map<qint64, YOLOv8Result> m_pendingResults;  // 待排序的结果
    qint64 m_nextExpectedId;                          // 下一个期望的请求ID
    QMutex m_mutex;                                   // 结果映射互斥锁
    QTimer* m_checkTimer;                             // 定期检查定时器
};

// YOLOv8服务类
class YOLOv8Service : public QObject {
    Q_OBJECT

public:
    // 获取单例实例
    static YOLOv8Service& instance();
    
    // 析构函数
    ~YOLOv8Service();
    
    // 初始化服务（会创建3个检测器实例）
    bool initialize(const QString& model_path, const QString& label_path);
    
    // 提交检测请求
    void submitRequest(const YOLOv8Request& request);
    
    // 取消当前任务
    void cancelCurrentTask();
    
    // 重置服务
    void resetService();
    
    // 获取当前会话ID
    qint64 getCurrentSessionId() const;
    
    // 重置会话ID
    qint64 resetSessionId();
    
    // 获取下一个请求ID
    qint64 getNextRequestId();
    
    // 停止服务
    void stop();
    
    // 获取服务状态
    bool isRunning() const;
    bool isInitialized() const;
    
    // 直接执行检测（同步调用）
    YOLOv8Result detect(const cv::Mat& image, float confidence_threshold = 0.5f, float nms_threshold = 0.45f);
    
    // 在图像上绘制检测结果
    void drawDetections(cv::Mat& image, const std::vector<YOLOv8Result::Detection>& detections);
    
    // 获取可用的检测器实例（线程安全）
    YOLOv8Detector* getAvailableDetector();
    
    // 释放检测器实例
    void releaseDetector(YOLOv8Detector* detector);
    
    // 获取队列中的请求数量
    int pendingRequestCount() const;
    
    // 设置最大队列长度
    void setMaxQueueSize(int size);
    
    // 获取线程池状态
    int activeThreadCount() const;
    int maxThreadCount() const;

signals:
    // 检测完成信号（保证顺序）
    void detectionCompleted(const YOLOv8Result& result);
    
    // 队列状态变化信号
    void queueSizeChanged(int size);
    
    // 线程池状态变化信号
    void threadPoolStatusChanged(int activeThreads, int maxThreads);

private slots:
    // 处理任务完成
    void handleTaskCompleted(const YOLOv8Result& result);
    
    // 处理排序后的结果
    void handleOrderedResult(const YOLOv8Result& result);
    
    // 更新线程池状态
    void updateThreadPoolStatus();

private:
    // 私有构造函数（单例模式）
    YOLOv8Service();
    
    // 禁止复制和赋值
    YOLOv8Service(const YOLOv8Service&) = delete;
    YOLOv8Service& operator=(const YOLOv8Service&) = delete;
    
    // 日志函数
    void logInfo(const QString& message) const;
    void logError(const QString& message) const;
    
    // 成员变量
    mutable QMutex m_mutex;                                     // 主互斥锁
    std::vector<std::unique_ptr<YOLOv8Detector>> m_detectors;   // 三个检测器实例
    std::vector<bool> m_detectorAvailable;                      // 检测器可用状态
    QMutex m_detectorMutex;                                     // 检测器分配互斥锁
    QWaitCondition m_detectorCondition;                         // 检测器等待条件
    
    QThreadPool* m_threadPool;                                  // 线程池
    std::unique_ptr<ResultOrderManager> m_orderManager;         // 结果排序管理器
    
    QAtomicInteger<bool> m_running;                             // 服务是否运行
    QAtomicInteger<bool> m_initialized;                         // 服务是否初始化
    QAtomicInteger<qint64> m_currentSessionId;                  // 当前会话ID
    QAtomicInteger<qint64> m_currentRequestId;                  // 当前请求ID
    
    int m_maxQueueSize;                                         // 最大队列长度
    QTimer* m_statusTimer;                                      // 状态更新定时器
    
    // 性能统计
    QAtomicInteger<int> m_totalRequests;                        // 总请求数
    QAtomicInteger<int> m_completedRequests;                    // 完成请求数
};

} // namespace SmartScope

// 声明元类型，使其可以在Qt信号槽系统中使用
Q_DECLARE_METATYPE(SmartScope::YOLOv8Result)
Q_DECLARE_METATYPE(SmartScope::YOLOv8Result::Detection)

#endif // YOLOV8_SERVICE_HPP