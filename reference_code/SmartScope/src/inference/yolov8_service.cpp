#include "inference/yolov8_service.hpp"
#include "app/yolov8/yolov8_detector.h"
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <chrono>
#include <QDebug>
#include <algorithm>

namespace SmartScope {

//=============================================
// DetectionTask 实现
//=============================================

DetectionTask::DetectionTask(const YOLOv8Request& request, YOLOv8Detector* detector, YOLOv8Service* service)
    : m_request(request)
    , m_detector(detector)
    , m_service(service)
{
    setAutoDelete(true); // 任务完成后自动删除
}

void DetectionTask::run()
{
    YOLOv8Result result;
    result.image = m_request.image.clone();
    result.session_id = m_request.session_id;
    result.request_id = m_request.request_id;
    result.success = false;
    
    try {
        // 记录开始时间
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 检查图像是否有效
        if (m_request.image.empty()) {
            result.error_message = "输入图像为空";
            emit taskCompleted(result);
            m_service->releaseDetector(m_detector);
            return;
        }
        
        // 检查检测器是否有效
        if (!m_detector) {
            result.error_message = "检测器实例为空";
            emit taskCompleted(result);
            return;
        }
        
        // 输出图像信息以便调试
        printf("[DetectionTask] 处理图像: 请求ID=%lld, 尺寸=%dx%d, 通道=%d\n",
               m_request.request_id, m_request.image.cols, m_request.image.rows, m_request.image.channels());
        
        // 执行检测
        std::vector<YOLOv8Detection> yolo_detections = m_detector->detect(m_request.image, m_request.confidence_threshold);
        
        // 记录结束时间并计算检测耗时
        auto detect_end = std::chrono::high_resolution_clock::now();
        auto detect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(detect_end - start_time).count();
        
        printf("[DetectionTask] 检测完成，请求ID=%lld, 耗时=%ld ms，检测到=%zu个目标\n", 
               m_request.request_id, detect_duration, yolo_detections.size());
        
        // 将YOLOv8Detection转换为YOLOv8Result::Detection
        for (const auto& det : yolo_detections) {
            YOLOv8Result::Detection detection;
            detection.class_name = det.class_name;
            detection.confidence = det.confidence;
            detection.box = det.box;
            result.detections.push_back(detection);
        }
        
        // 创建结果图像
        result.result_image = m_request.image.clone();
        
        // 在图像上绘制检测结果
        m_detector->drawDetections(result.result_image, yolo_detections);
        
        // 如果指定了保存路径，保存结果图像
        if (!m_request.save_path.isEmpty()) {
            QDir saveDir = QFileInfo(m_request.save_path).dir();
            if (!saveDir.exists()) {
                saveDir.mkpath(".");
            }
            
            try {
                cv::imwrite(m_request.save_path.toStdString(), result.result_image);
            } catch (...) {
                // 忽略保存图像时的异常，不影响结果返回
            }
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.error_message = QString("检测异常: %1").arg(e.what());
        printf("[DetectionTask] 检测异常: %s\n", e.what());
    } catch (...) {
        result.error_message = "检测未知异常";
        printf("[DetectionTask] 检测未知异常\n");
    }
    
    // 释放检测器
    m_service->releaseDetector(m_detector);
    
    // 发射任务完成信号
    emit taskCompleted(result);
}

//=============================================
// ResultOrderManager 实现
//=============================================

ResultOrderManager::ResultOrderManager(QObject* parent)
    : QObject(parent)
    , m_nextExpectedId(1)
{
    // 创建定期检查定时器
    m_checkTimer = new QTimer(this);
    m_checkTimer->setInterval(10); // 每10ms检查一次
    connect(m_checkTimer, &QTimer::timeout, this, &ResultOrderManager::checkPendingResults);
    m_checkTimer->start();
}

void ResultOrderManager::addResult(const YOLOv8Result& result)
{
    QMutexLocker locker(&m_mutex);
    
    // 添加结果到待排序映射
    m_pendingResults[result.request_id] = result;
    
    printf("[ResultOrderManager] 添加结果: 请求ID=%lld, 期望ID=%lld, 待处理数量=%zu\n",
           result.request_id, m_nextExpectedId, m_pendingResults.size());
}

void ResultOrderManager::setExpectedOrder(qint64 requestId)
{
    QMutexLocker locker(&m_mutex);
    m_nextExpectedId = requestId;
    printf("[ResultOrderManager] 设置期望顺序: 从请求ID=%lld开始\n", requestId);
}

void ResultOrderManager::checkPendingResults()
{
    QMutexLocker locker(&m_mutex);
    
    // 检查是否有按顺序的结果可以输出
    while (m_pendingResults.find(m_nextExpectedId) != m_pendingResults.end()) {
        YOLOv8Result result = m_pendingResults[m_nextExpectedId];
        m_pendingResults.erase(m_nextExpectedId);
        
        printf("[ResultOrderManager] 输出有序结果: 请求ID=%lld, 剩余待处理=%zu\n",
               result.request_id, m_pendingResults.size());
        
        m_nextExpectedId++;
        
        // 解锁后发射信号，避免死锁
        locker.unlock();
        emit orderedResultReady(result);
        locker.relock();
    }
}

//=============================================
// YOLOv8Service 实现
//=============================================

// 单例实例
YOLOv8Service& YOLOv8Service::instance() {
    static YOLOv8Service instance;
    return instance;
}

// 构造函数
YOLOv8Service::YOLOv8Service() 
    : m_running(false)
    , m_initialized(false)
    , m_currentSessionId(0)
    , m_currentRequestId(0)
    , m_maxQueueSize(10)
    , m_totalRequests(0)
    , m_completedRequests(0)
{
    // 创建线程池，设置为3个线程（对应3个NPU核心）
    m_threadPool = new QThreadPool(this);
    m_threadPool->setMaxThreadCount(3);
    
    // 创建结果排序管理器
    m_orderManager = std::make_unique<ResultOrderManager>(this);
    
    // 连接排序管理器信号
    connect(m_orderManager.get(), &ResultOrderManager::orderedResultReady,
            this, &YOLOv8Service::handleOrderedResult);
    
    // 创建状态更新定时器
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(1000); // 每秒更新一次状态
    connect(m_statusTimer, &QTimer::timeout, this, &YOLOv8Service::updateThreadPoolStatus);
    
    // 初始化检测器可用状态
    m_detectorAvailable.resize(3, false);
    
    printf("[YOLOv8Service] 服务已创建，线程池最大线程数: %d\n", m_threadPool->maxThreadCount());
}

// 析构函数
YOLOv8Service::~YOLOv8Service() {
    stop();
}

bool YOLOv8Service::initialize(const QString& model_path, const QString& label_path)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        logInfo("YOLOv8Service已经初始化过了");
        return true;
    }
    
    logInfo(QString("开始初始化YOLOv8Service，模型路径: %1，标签路径: %2").arg(model_path).arg(label_path));
    
    try {
        // 检查文件是否存在
        QFile modelFile(model_path);
        QFile labelFile(label_path);
        
        if (!modelFile.exists()) {
            logError(QString("模型文件不存在: %1").arg(model_path));
            return false;
        }
        
        if (!labelFile.exists()) {
            logError(QString("标签文件不存在: %1").arg(label_path));
            return false;
        }
        
        // 创建三个检测器实例
        m_detectors.clear();
        m_detectors.resize(3);
        
        for (int i = 0; i < 3; i++) {
            logInfo(QString("正在初始化检测器 %1/3...").arg(i + 1));
            
            m_detectors[i] = std::make_unique<YOLOv8Detector>();
            
            if (!m_detectors[i]->initialize(model_path.toStdString(), label_path.toStdString())) {
                logError(QString("检测器 %1 初始化失败").arg(i + 1));
                return false;
            }
            
            m_detectorAvailable[i] = true;
            logInfo(QString("检测器 %1 初始化成功").arg(i + 1));
        }
        
        m_initialized = true;
        m_running = true;
        
        // 启动状态定时器
        m_statusTimer->start();
        
        logInfo("YOLOv8Service初始化完成，已创建3个检测器实例");
        return true;
        
    } catch (const std::exception& e) {
        logError(QString("初始化YOLOv8Service时发生异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        logError("初始化YOLOv8Service时发生未知异常");
        return false;
    }
}

void YOLOv8Service::submitRequest(const YOLOv8Request& request)
{
    if (!m_initialized || !m_running) {
        logError("YOLOv8Service未初始化或未运行，无法提交请求");
        return;
    }
    
    // 检查队列是否已满
    if (m_threadPool->activeThreadCount() >= m_maxQueueSize) {
        logInfo("请求队列已满，跳过当前请求");
        return;
    }
    
    // 增加总请求计数
    m_totalRequests.fetchAndAddRelaxed(1);
    
    // 获取可用的检测器
    YOLOv8Detector* detector = getAvailableDetector();
    if (!detector) {
        logError("无可用检测器，请求被拒绝");
        return;
    }
    
    // 创建检测任务
    DetectionTask* task = new DetectionTask(request, detector, this);
    
    // 连接任务完成信号
    connect(task, &DetectionTask::taskCompleted, this, &YOLOv8Service::handleTaskCompleted,
            Qt::QueuedConnection);
    
    // 提交任务到线程池
    m_threadPool->start(task);
    
    printf("[YOLOv8Service] 提交检测任务: 请求ID=%lld, 活跃线程=%d/%d\n",
           request.request_id, m_threadPool->activeThreadCount(), m_threadPool->maxThreadCount());
    
    // 发射队列状态变化信号
    emit queueSizeChanged(m_threadPool->activeThreadCount());
}

YOLOv8Detector* YOLOv8Service::getAvailableDetector()
{
    QMutexLocker locker(&m_detectorMutex);
    
    // 查找可用的检测器
    for (int i = 0; i < 3; i++) {
        if (m_detectorAvailable[i] && m_detectors[i]) {
            m_detectorAvailable[i] = false; // 标记为已使用
            printf("[YOLOv8Service] 分配检测器 %d\n", i);
            return m_detectors[i].get();
        }
    }
    
    // 如果没有可用检测器，等待一小段时间
    if (!m_detectorCondition.wait(&m_detectorMutex, 100)) {
        printf("[YOLOv8Service] 等待检测器超时\n");
        return nullptr;
    }
    
    // 重新尝试获取检测器
    for (int i = 0; i < 3; i++) {
        if (m_detectorAvailable[i] && m_detectors[i]) {
            m_detectorAvailable[i] = false;
            printf("[YOLOv8Service] 等待后分配检测器 %d\n", i);
            return m_detectors[i].get();
        }
    }
    
    return nullptr;
}

void YOLOv8Service::releaseDetector(YOLOv8Detector* detector)
{
    QMutexLocker locker(&m_detectorMutex);
    
    // 查找并释放检测器
    for (int i = 0; i < 3; i++) {
        if (m_detectors[i].get() == detector) {
            m_detectorAvailable[i] = true;
            printf("[YOLOv8Service] 释放检测器 %d\n", i);
            m_detectorCondition.wakeOne(); // 唤醒等待的线程
            break;
        }
    }
}

void YOLOv8Service::handleTaskCompleted(const YOLOv8Result& result)
{
    // 增加完成请求计数
    m_completedRequests.fetchAndAddRelaxed(1);
    
    printf("[YOLOv8Service] 任务完成: 请求ID=%lld, 成功=%s, 检测数=%zu\n",
           result.request_id, result.success ? "是" : "否", result.detections.size());
    
    // 将结果添加到排序管理器
    m_orderManager->addResult(result);
    
    // 发射队列状态变化信号
    emit queueSizeChanged(m_threadPool->activeThreadCount());
}

void YOLOv8Service::handleOrderedResult(const YOLOv8Result& result)
{
    // 发射有序的检测完成信号
    emit detectionCompleted(result);
}

void YOLOv8Service::updateThreadPoolStatus()
{
    int activeThreads = m_threadPool->activeThreadCount();
    int maxThreads = m_threadPool->maxThreadCount();
    
    // 发射线程池状态信号
    emit threadPoolStatusChanged(activeThreads, maxThreads);
    
    // // 每10秒输出一次性能统计
    // static int statusCounter = 0;
    // if (++statusCounter % 10 == 0) {
    //     int totalReq = m_totalRequests.load();
    //     int completedReq = m_completedRequests.load();
    //     printf("[YOLOv8Service] 性能统计: 总请求=%d, 已完成=%d, 活跃线程=%d/%d\n",
    //            totalReq, completedReq, activeThreads, maxThreads);
    // }
}

qint64 YOLOv8Service::resetSessionId()
{
    qint64 newSessionId = QDateTime::currentMSecsSinceEpoch();
    m_currentSessionId.store(newSessionId);
    
    // 重置请求ID计数器
    m_currentRequestId.store(0);
    
    // 设置排序管理器的期望顺序
    m_orderManager->setExpectedOrder(1);
    
    printf("[YOLOv8Service] 重置会话ID: %lld\n", newSessionId);
    return newSessionId;
}

qint64 YOLOv8Service::getCurrentSessionId() const
{
    return m_currentSessionId.load();
}

qint64 YOLOv8Service::getNextRequestId()
{
    return m_currentRequestId.fetchAndAddRelaxed(1) + 1;
}

void YOLOv8Service::stop()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_running) {
        return;
    }
    
    logInfo("停止YOLOv8Service...");
    
    m_running = false;
    
    // 停止状态定时器
    if (m_statusTimer) {
        m_statusTimer->stop();
    }
    
    // 等待所有任务完成
    if (m_threadPool) {
        m_threadPool->waitForDone(5000); // 最多等待5秒
    }
    
    // 清理检测器
    m_detectors.clear();
    m_detectorAvailable.clear();
    
    m_initialized = false;
    
    logInfo("YOLOv8Service已停止");
}

bool YOLOv8Service::isRunning() const
{
    return m_running.load();
}

bool YOLOv8Service::isInitialized() const
{
    return m_initialized.load();
}

int YOLOv8Service::activeThreadCount() const
{
    return m_threadPool ? m_threadPool->activeThreadCount() : 0;
}

int YOLOv8Service::maxThreadCount() const
{
    return m_threadPool ? m_threadPool->maxThreadCount() : 0;
}

int YOLOv8Service::pendingRequestCount() const
{
    return activeThreadCount();
}

void YOLOv8Service::setMaxQueueSize(int size)
{
    QMutexLocker locker(&m_mutex);
    m_maxQueueSize = size;
}

void YOLOv8Service::cancelCurrentTask()
{
    // 清空线程池中的待处理任务
    if (m_threadPool) {
        m_threadPool->clear();
    }
}

void YOLOv8Service::resetService()
{
    QMutexLocker locker(&m_mutex);
    
    // 取消当前任务
    cancelCurrentTask();
    
    // 重置计数器
    m_totalRequests.store(0);
    m_completedRequests.store(0);
    
    logInfo("YOLOv8Service已重置");
}

// 保留原有的同步检测方法用于兼容性
YOLOv8Result YOLOv8Service::detect(const cv::Mat& image, float confidence_threshold, float nms_threshold)
{
    YOLOv8Result result;
    result.success = false;
    
    if (!m_initialized || m_detectors.empty()) {
        result.error_message = "服务未初始化";
        return result;
    }
    
    // 使用第一个检测器进行同步检测
    YOLOv8Detector* detector = m_detectors[0].get();
    if (!detector) {
        result.error_message = "检测器不可用";
        return result;
    }
    
    try {
        auto yolo_detections = detector->detect(image, confidence_threshold);
        
        for (const auto& det : yolo_detections) {
            YOLOv8Result::Detection detection;
            detection.class_name = det.class_name;
            detection.confidence = det.confidence;
            detection.box = det.box;
            result.detections.push_back(detection);
        }
        
        result.image = image.clone();
        result.result_image = image.clone();
        detector->drawDetections(result.result_image, yolo_detections);
        result.success = true;
        
    } catch (const std::exception& e) {
        result.error_message = QString("检测异常: %1").arg(e.what());
    }
    
    return result;
}

void YOLOv8Service::drawDetections(cv::Mat& image, const std::vector<YOLOv8Result::Detection>& detections)
{
    // 这个方法保持不变，用于兼容性
    if (m_initialized && !m_detectors.empty()) {
        // 转换检测结果格式
        std::vector<YOLOv8Detection> yolo_dets;
        for (const auto& det : detections) {
            YOLOv8Detection yolo_det;
            yolo_det.class_name = det.class_name;
            yolo_det.confidence = det.confidence;
            yolo_det.box = det.box;
            yolo_dets.push_back(yolo_det);
        }
        
        m_detectors[0]->drawDetections(image, yolo_dets);
    }
}

void YOLOv8Service::logInfo(const QString& message) const
{
    qDebug() << "[YOLOv8Service]" << message;
}

void YOLOv8Service::logError(const QString& message) const
{
    qDebug() << "[YOLOv8Service] ERROR:" << message;
}

} // namespace SmartScope 