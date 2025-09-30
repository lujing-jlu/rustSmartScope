#include "inference/inference_service.hpp"
#include "infrastructure/logging/logger.h"
#include <cmath>
#include <QCoreApplication>

namespace SmartScope {

InferenceService& InferenceService::instance() {
    static InferenceService instance;
    return instance;
}

InferenceService::InferenceService()
    : m_running(false)
    , m_initialized(false)
    , m_currentSessionId(QDateTime::currentMSecsSinceEpoch()) // 使用当前时间戳初始化会话ID
{
    // 将对象移动到工作线程
    this->moveToThread(&m_workerThread);
    
    // 连接信号
    connect(this, &InferenceService::newRequestAvailable,
            this, &InferenceService::processRequest,
            Qt::QueuedConnection);
            
    // 启动工作线程
    m_workerThread.start();
}

InferenceService::~InferenceService() {
    stop();
}

bool InferenceService::initialize(const QString& model_path) {
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        logInfo("推理服务已经初始化");
        return true;
    }
    
    try {
        // 在启动阶段完成综合处理器与统一引擎初始化
        QString camera_param_dir = QCoreApplication::applicationDirPath() + "/camera_parameters";  // 相机参数目录
        stereo_depth::ComprehensiveDepthOptions options;
        options.min_disparity = 0;
        options.num_disparities = 16 * 8;
        options.block_size = 5;
        // 更严格的SGBM过滤：
        options.uniqueness_ratio = 15;   // 默认10 -> 15，提高匹配唯一性要求
        options.disp12_max_diff   = 1;   // 保持严格的左右一致性阈值
        options.speckle_window    = 150; // 去除斑点噪声的窗口更大
        options.speckle_range     = 32;  // 保持较严格范围
        options.prefilter_cap     = 63;  // 预滤波上限
        logInfo("严格SGBM参数: uniqueness=15, disp12_max_diff=1, speckle_window=150, speckle_range=32");
 
        options.min_samples = 1000;
        options.ransac_max_iterations = 50;
        options.ransac_threshold = 30.0f;
        options.min_inliers_ratio = 10;
        
        m_comprehensiveProcessor = std::make_unique<stereo_depth::ComprehensiveDepthProcessor>(
            camera_param_dir.toStdString(), model_path.toStdString(), options);
        m_engine = std::make_unique<StereoDepthEngine>(m_comprehensiveProcessor.get());
        try {
            cv::Mat Q_from_proc = m_comprehensiveProcessor->getQMatrix();
            if (!Q_from_proc.empty()) {
                m_engine->injectQ(Q_from_proc);
                logInfo("StereoDepthEngine: Q 已注入");
            }
        } catch (...) {
            logWarning("StereoDepthEngine: 注入Q失败，保持处理器默认Q");
        }
        
        m_initialized = true;
        m_running = true;
        logInfo("推理服务初始化成功（启动阶段已完成模型与处理器加载）");
        return true;
    } catch (const std::exception& e) {
        logError(QString("推理服务初始化失败: %1").arg(e.what()));
        return false;
    }
}

void InferenceService::setPerformanceMode(StereoDepthInference::PerformanceMode mode) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        logError("推理服务未初始化，无法设置性能模式");
        return;
    }
    
    try {
        m_inference->setPerformanceMode(mode);
        QString mode_str;
        switch (mode) {
            case StereoDepthInference::HIGH_QUALITY: mode_str = "高质量"; break;
            case StereoDepthInference::BALANCED: mode_str = "平衡"; break;
            case StereoDepthInference::FAST: mode_str = "快速"; break;
            case StereoDepthInference::ULTRA_FAST: mode_str = "极速"; break;
            default: mode_str = "未知";
        }
        logInfo(QString("性能模式设置为: %1").arg(mode_str));
    } catch (const std::exception& e) {
        logError(QString("设置性能模式失败: %1").arg(e.what()));
    }
}

StereoDepthInference::PerformanceMode InferenceService::getPerformanceMode() const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        logError("推理服务未初始化，无法获取性能模式");
        return StereoDepthInference::BALANCED; // 默认返回平衡模式
    }
    
    return m_inference->getPerformanceMode();
}

void InferenceService::submitRequest(const InferenceRequest& request) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        logError("推理服务未初始化");
        InferenceResult result;
        result.success = false;
        result.error_message = "推理服务未初始化";
        result.session_id = m_currentSessionId; // 添加当前会话ID
        emit inferenceCompleted(result);
        return;
    }
    
    // 创建请求副本并设置会话ID
    InferenceRequest requestWithSessionId = request;
    requestWithSessionId.session_id = m_currentSessionId;
    
    // 添加请求到队列
    m_requestQueue.enqueue(requestWithSessionId);
    
    logInfo(QString("推理请求已加入队列，会话ID: %1").arg(m_currentSessionId));
    
    // 通知处理线程
    emit newRequestAvailable();
}

void InferenceService::cancelCurrentTask() {
    QMutexLocker locker(&m_mutex);
    
    // 清空请求队列
    if (!m_requestQueue.isEmpty()) {
        logInfo("取消当前推理任务，清空请求队列");
        m_requestQueue.clear();
    }
    
    // 构造一个取消的结果通知
    InferenceResult result;
    result.success = false;
    result.error_message = "推理任务被用户取消";
    result.session_id = m_currentSessionId; // 添加当前会话ID
    
    // 发送取消通知
    locker.unlock(); // 解锁以避免死锁
    emit inferenceCompleted(result);
    
    logInfo("推理任务已取消");
}

void InferenceService::processRequest() {
    QMutexLocker locker(&m_mutex);

    // 启动阶段已初始化处理器与引擎；此处仅检查
    if (!m_comprehensiveProcessor) {
        logError("综合处理器未初始化（应在启动阶段完成）");
        return;
    }
    
    while (!m_requestQueue.isEmpty() && m_running) {
        // 获取请求
        InferenceRequest request = m_requestQueue.dequeue();
        locker.unlock();
        
        InferenceResult result;
        result.save_path = request.save_path;
        // 传递原始图像尺寸
        result.original_width = request.original_width;
        result.original_height = request.original_height;
        // 传递会话ID
        result.session_id = request.session_id;
        
        try {
            // 记录开始时间
            auto total_start_time = std::chrono::high_resolution_clock::now();
            
            // 直接使用原始图像进行推理（stereo），若启用4:3裁剪，则仅对 mono 输入与输出进行裁剪
            cv::Mat left_for_stereo = request.left_image;
            cv::Mat right_for_stereo = request.right_image;
            cv::Mat left_for_mono = request.left_image;
            bool use_crop = request.apply_43_crop && request.crop_roi.width > 0 && request.crop_roi.height > 0;
            if (use_crop) {
                cv::Rect roi = request.crop_roi & cv::Rect(0, 0, request.left_image.cols, request.left_image.rows);
                if (roi.width > 0 && roi.height > 0) {
                    left_for_mono = request.left_image(roi).clone();
                }
            }
            
            // 根据深度模式选择推理方式
            logInfo(QString("当前深度模式: %1").arg(static_cast<int>(m_depthMode)));
            logInfo(QString("综合深度处理器状态: %1").arg(m_comprehensiveProcessor ? "可用" : "不可用"));
            
            // 始终运行综合深度处理器，生成完整的中间结果（单目原始/校准、双目、置信度等）
            try {
                cv::Mat Q = m_comprehensiveProcessor ? m_comprehensiveProcessor->getQMatrix() : cv::Mat();

                // 并行计算：
                // 任务A：视差 -> 双目深度 -> 过滤
                // 任务B：单目深度
                cv::Mat disparity, stereo_depth, stereo_depth_filtered, mono_depth;

                auto taskStereo = [&]() {
                    try {
                        cv::Mat d = m_comprehensiveProcessor->computeDisparityOnly(left_for_stereo, right_for_stereo);
                        cv::Mat sd = m_comprehensiveProcessor->depthFromDisparity(d, Q);
                        cv::Mat vm;
                        if (!d.empty() && !sd.empty()) {
                            vm = (d > 0) & (sd > 0) & (sd < 1e7);
                        }
                        cv::Mat sdf = sd.empty() ? cv::Mat() : m_comprehensiveProcessor->filterDepth(sd, vm);
                        disparity = d; stereo_depth = sd; stereo_depth_filtered = sdf;
                    } catch (const cv::Exception& e) {
                        logError(QString("Stereo 线程异常: %1").arg(e.what()));
                        disparity.release(); stereo_depth.release(); stereo_depth_filtered.release();
                    } catch (const std::exception& e) {
                        logError(QString("Stereo 线程异常(std): %1").arg(e.what()));
                        disparity.release(); stereo_depth.release(); stereo_depth_filtered.release();
                    } catch (...) {
                        logError("Stereo 线程未知异常");
                        disparity.release(); stereo_depth.release(); stereo_depth_filtered.release();
                    }
                };
                auto taskMono = [&]() {
                    try {
                        mono_depth = m_comprehensiveProcessor->computeMonoDepthOnly(left_for_mono);
                    } catch (const cv::Exception& e) {
                        logError(QString("Mono 线程异常: %1").arg(e.what()));
                        mono_depth.release();
                    } catch (const std::exception& e) {
                        logError(QString("Mono 线程异常(std): %1").arg(e.what()));
                        mono_depth.release();
                    } catch (...) {
                        logError("Mono 线程未知异常");
                        mono_depth.release();
                    }
                };

                // 简易并行：使用两个std::thread，避免引入额外Qt依赖
                std::thread thStereo(taskStereo);
                std::thread thMono(taskMono);
                thStereo.join();
                thMono.join();

                // 若启用4:3裁剪，则将 stereo 系列输出按同一ROI做中心裁剪，以便后续显示/融合一致
                if (use_crop) {
                    cv::Rect roi = request.crop_roi & cv::Rect(0, 0, left_for_stereo.cols, left_for_stereo.rows);
                    auto safeCrop = [&](const cv::Mat& src)->cv::Mat{
                        if (src.empty() || roi.width<=0 || roi.height<=0) return src;
                        if (src.cols!=left_for_stereo.cols || src.rows!=left_for_stereo.rows) return src;
                        return src(roi).clone();
                    };
                    disparity = safeCrop(disparity);
                    stereo_depth = safeCrop(stereo_depth);
                    stereo_depth_filtered = safeCrop(stereo_depth_filtered);
                }

                // 调试：基线/焦距与视差统计
                try {
                    if (!disparity.empty()) {
                        cv::Mat QQ = m_comprehensiveProcessor->getQMatrix();
                        double fx_q = 0.0, invB = 0.0, B_mm = 0.0;
                        if (!QQ.empty()) {
                            if (QQ.rows == 4 && QQ.cols == 4) {
                                fx_q = QQ.at<double>(2, 3);
                                invB = QQ.at<double>(3, 2);
                            } else if (QQ.rows == 3 && QQ.cols == 4) {
                                fx_q = QQ.at<double>(2, 3);
                            }
                            if (invB != 0.0) B_mm = 1.0 / invB;
                        }
                        // 计算视差中位数
                        cv::Mat disp32f;
                        disparity.convertTo(disp32f, CV_32F);
                        cv::Mat validDispMask = disp32f > 0;
                        std::vector<float> dv;
                        if (cv::countNonZero(validDispMask) > 0) {
                            dv.reserve(static_cast<size_t>(disp32f.total()));
                            for (int y = 0; y < disp32f.rows; ++y) {
                                const float* dp = disp32f.ptr<float>(y);
                                const uchar* mp = validDispMask.ptr<uchar>(y);
                                for (int x = 0; x < disp32f.cols; ++x) {
                                    if (mp[x]) dv.push_back(dp[x]);
                                }
                            }
                        }
                        double d_med = 0.0; double z_med_est = 0.0;
                        if (!dv.empty()) {
                            size_t mid = dv.size() / 2;
                            std::nth_element(dv.begin(), dv.begin() + mid, dv.end());
                            d_med = dv[mid];
                            if (fx_q > 0.0 && B_mm > 0.0 && d_med > 1e-6) {
                                z_med_est = fx_q * B_mm / d_med;
                            }
                        }
                        logInfo(QString("Q[2,3]=fx=%1, Q[3,2]=1/B=%2, B(mm)=%3, disparity_median=%4, Z_median_est(mm)=%5")
                                .arg(fx_q).arg(invB).arg(B_mm).arg(d_med).arg(z_med_est));
                    }
                } catch (...) {}

                // 计算左边界（与综合方法一致）
                int leftBoundX = 0;
                try {
                    if (m_comprehensiveProcessor) {
                        auto rois = m_comprehensiveProcessor->getROI();
                        leftBoundX = std::max(rois.first.x, rois.second.x);
                    }
                } catch (...) { /* 忽略 */ }
                logInfo(QString("calibration leftBoundX = %1").arg(leftBoundX));

                cv::Mat mono_calibrated;
                cv::Mat validMask;
                if (!disparity.empty() && !stereo_depth_filtered.empty()) {
                    validMask = (disparity > 0) & (stereo_depth_filtered > 0) & (stereo_depth_filtered < 1e7);
                }
                
                // 使用增强的分层校准方法
                        stereo_depth::DepthCalibrationResult calib = m_comprehensiveProcessor->calibrateDepthPlanarLayered(
            mono_depth, stereo_depth_filtered, disparity, validMask, leftBoundX);
                
                // 应用校准结果到单目深度
                if (calib.success) {
                    mono_depth.convertTo(mono_calibrated, CV_32F);
                    mono_calibrated = mono_calibrated * (float)calib.scale_factor + (float)calib.bias;
                } else {
                    mono_calibrated = mono_depth.clone();
                }

                // 置信度与融合
                cv::Mat confidence = m_comprehensiveProcessor->buildConfidenceMap(disparity, stereo_depth_filtered);
                cv::Mat fused_depth = m_comprehensiveProcessor->fuseDepthMaps(
                    stereo_depth_filtered,
                    mono_calibrated.empty() ? mono_depth : mono_calibrated,
                    confidence);

                // 鲁棒裁剪：将异常深度置零（保持毫米单位）
                auto robustClip = [](const cv::Mat& depth_mm, double expectedMaxMm = 100.0) {
                    if (depth_mm.empty()) return depth_mm;
                    cv::Mat valid = (depth_mm > 0) & (depth_mm < 1e7);
                    if (cv::countNonZero(valid) == 0) return depth_mm;
                    std::vector<float> zv; zv.reserve(static_cast<size_t>(depth_mm.total()));
                    for (int y = 0; y < depth_mm.rows; ++y) {
                        const float* dp = depth_mm.ptr<float>(y);
                        const uchar* mp = valid.ptr<uchar>(y);
                        for (int x = 0; x < depth_mm.cols; ++x) if (mp[x]) zv.push_back(dp[x]);
                    }
                    if (zv.empty()) return depth_mm;
                    std::sort(zv.begin(), zv.end());
                    auto idx = [&](double q){ return zv[std::min(std::max<size_t>(0, (size_t)(q*(zv.size()-1))), zv.size()-1)]; };
                    double zLow = idx(0.01); // 1%
                    double zHigh = idx(0.99); // 99%
                    double zUpper = std::min(zHigh, expectedMaxMm);
                    cv::Mat clipped = depth_mm.clone();
                    // 置零异常值
                    cv::Mat lowMask = clipped < zLow;
                    cv::Mat highMask = clipped > zUpper;
                    clipped.setTo(0, lowMask | highMask);
                    return clipped;
                };
                // 关闭鲁棒裁剪：直接使用滤波后的深度图（保留毫米单位）
                cv::Mat stereo_depth_clean = stereo_depth_filtered;
                cv::Mat mono_calibrated_clean = mono_calibrated;
                cv::Mat fused_depth_clean = fused_depth;
                        
                // 记录清理前后范围
                auto logRange = [&](const char* name, const cv::Mat& m){ if (!m.empty()){ double mn=0,mx=0; cv::minMaxLoc(m, &mn, &mx, 0, 0, m>0); logInfo(QString("%1范围(>0): [%2, %3] mm").arg(name).arg(mn).arg(mx)); }};
                logRange("stereo_depth_clean", stereo_depth_clean);
                if (!mono_calibrated_clean.empty()) logRange("mono_calibrated_clean", mono_calibrated_clean);
                if (!fused_depth_clean.empty()) logRange("fused_depth_clean", fused_depth_clean);

                // 关闭“局部拟合”策略：不再使用单目作为参考对双目进行分块拟合

                // 填充所有调试/显示所需的数据（若启用3:4裁剪，mono已为裁剪输入；stereo系亦已裁剪）
                result.mono_depth_raw = mono_depth;
                result.mono_depth_calibrated = mono_calibrated_clean; // 清理后的“第四张”
                result.disparity_map = disparity;
                result.confidence_map = confidence;
                result.calibration_scale = calib.scale_factor;
                result.calibration_bias = calib.bias;
                result.calibration_success = calib.success;
                        
                // 根据深度模式选择最终用于显示/点云的深度图（仅双目/单目）
                if (m_depthMode == DepthMode::STEREO_ONLY) {
                    result.depth_map = stereo_depth_clean;
                } else if (m_depthMode == DepthMode::MONO_CALIBRATED) {
                    result.depth_map = !mono_calibrated_clean.empty() ? mono_calibrated_clean : mono_depth;
                } else {
                    result.depth_map = stereo_depth_clean;
                }

                // 诊断：打印深度图最小/最大值
                if (!result.depth_map.empty()) {
                    double dmin = 0, dmax = 0;
                    cv::minMaxLoc(result.depth_map, &dmin, &dmax);
                    logInfo(QString("depth_map范围: [%1, %2] mm").arg(dmin).arg(dmax));
                        }
                        
                logInfo(QString("模块化综合处理成功 - 校准成功: %1, 缩放: %2, 偏移: %3")
                        .arg(calib.success ? "是" : "否")
                        .arg(calib.scale_factor)
                        .arg(calib.bias));
                } catch (const std::exception& e) {
                logError(QString("模块化综合处理异常: %1，不再回退到旧管线").arg(e.what()));
                result.success = false;
                result.error_message = QString("综合处理异常: %1").arg(e.what());
                emit inferenceCompleted(result);
                continue;
            }
            
            // 记录推理结束时间
            auto inference_end_time = std::chrono::high_resolution_clock::now();
            auto inference_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                inference_end_time - total_start_time).count();
            
            // 保存结果
            if (!request.save_path.isEmpty()) {
                auto save_start_time = std::chrono::high_resolution_clock::now();
                
                m_inference->saveDisparity(result.depth_map, request.save_path.toStdString());
                
                // 如果需要生成点云
                if (request.generate_pointcloud) {
                    // 生成原始点云
                    QString pointcloud_path = request.save_path;
                    pointcloud_path.replace(".png", ".ply");
                    result.pointcloud_path = pointcloud_path;
                    
                    m_inference->savePointCloud(result.depth_map, request.left_image,
                                             pointcloud_path.toStdString(),
                                             request.baseline,
                                             request.focal_length);
                    
                    // 应用点云过滤（如果启用）- 暂时禁用
                    if (request.apply_filter) {
                        logInfo("点云过滤功能暂时禁用");
                            result.filter_success = false;
                    }
                    // 如果没有启用过滤但需要优化，直接在原始点云上应用优化 - 暂时禁用
                    else if (request.apply_optimize) {
                        logInfo("点云优化功能暂时禁用");
                            result.optimize_success = false;
                    }
                }
                
                auto save_end_time = std::chrono::high_resolution_clock::now();
                auto save_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    save_end_time - save_start_time).count();
                
                logInfo(QString("保存结果耗时: %1 ms").arg(save_duration));
            }
            
            // 记录总结束时间
            auto total_end_time = std::chrono::high_resolution_clock::now();
            auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                total_end_time - total_start_time).count();
            
            result.success = true;
            
            // 输出详细的时间信息
            logInfo(QString("推理完成 - 纯推理耗时: %1 ms, 总耗时: %2 ms, 图像大小: %3x%4")
                   .arg(inference_duration)
                   .arg(total_duration)
                   .arg(request.left_image.cols)
                   .arg(request.left_image.rows));
            
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = QString("推理失败: %1").arg(e.what());
            logError(result.error_message);
        }
        
        // 发送结果
        logInfo(QString("发送推理结果 - 深度图是否为空: %1, 尺寸: %2x%3")
                .arg(result.depth_map.empty() ? "是" : "否")
                .arg(result.depth_map.empty() ? 0 : result.depth_map.cols)
                .arg(result.depth_map.empty() ? 0 : result.depth_map.rows));
        logInfo("开始发射inferenceCompleted信号...");
        emit inferenceCompleted(result);
        logInfo("inferenceCompleted信号已发射完成");
        
        locker.relock();
    }
}

void InferenceService::stop() {
    {
        QMutexLocker locker(&m_mutex);
        if (!m_running) return;
        
        m_running = false;
        m_initialized = false;
        m_requestQueue.clear();
    }
    
    // 停止工作线程
    m_workerThread.quit();
    m_workerThread.wait();
    
    // 清理资源
    m_inference.reset();
    
    logInfo("推理服务已停止");
}

bool InferenceService::isRunning() const {
    QMutexLocker locker(&m_mutex);
    return m_running;
}

bool InferenceService::isInitialized() const {
    QMutexLocker locker(&m_mutex);
    return m_initialized;
}

qint64 InferenceService::getCurrentSessionId() const {
    QMutexLocker locker(&m_mutex);
    return m_currentSessionId;
}

StereoDepthInference* InferenceService::getInferencePtr() const {
    QMutexLocker locker(&m_mutex);
    return m_inference.get();
}

stereo_depth::ComprehensiveDepthProcessor* InferenceService::getComprehensiveProcessor() const {
    QMutexLocker locker(&m_mutex);
    return m_comprehensiveProcessor.get();
}

void InferenceService::setDepthMode(DepthMode mode) {
    QMutexLocker locker(&m_mutex);
    m_depthMode = mode;
    QString mode_str;
    switch (mode) {
        case DepthMode::STEREO_ONLY: mode_str = "仅双目深度"; break;
        case DepthMode::MONO_CALIBRATED: mode_str = "校准后单目深度"; break;
        default: mode_str = "未知";
    }
    logInfo(QString("深度模式设置为: %1").arg(mode_str));
}

InferenceService::DepthMode InferenceService::getDepthMode() const {
    QMutexLocker locker(&m_mutex);
    return m_depthMode;
}

void InferenceService::logInfo(const QString& message) const {
    LOG_INFO(message);
}

void InferenceService::logError(const QString& message) const {
    LOG_ERROR(message);
}

void InferenceService::logWarning(const QString& message) const {
    LOG_WARNING(message);
}

void InferenceService::resetService() {
    QMutexLocker locker(&m_mutex);
    
    // 清空请求队列
    if (!m_requestQueue.isEmpty()) {
        logInfo("重置推理服务，清空请求队列");
        m_requestQueue.clear();
    }
    
    // 重置会话ID
    qint64 newSessionId = resetSessionId();
    
    // 阻断当前正在处理的任务
    logInfo("推理服务已完全重置");
    
    locker.unlock(); // 解锁以避免死锁
    
    // 发送一个重置通知，确保任何等待结果的地方都能收到通知
    InferenceResult result;
    result.success = false;
    result.error_message = "推理服务已重置";
    result.session_id = newSessionId; // 使用新的会话ID
    emit inferenceCompleted(result);
}

qint64 InferenceService::resetSessionId() {
    QMutexLocker locker(&m_mutex);
    // 使用当前时间戳生成新的会话ID
    m_currentSessionId = QDateTime::currentMSecsSinceEpoch();
    logInfo(QString("重置会话ID: %1").arg(m_currentSessionId));
    return m_currentSessionId;
}

void InferenceService::shutdown() {
    QMutexLocker locker(&m_mutex);
    
    logInfo("正在关闭推理服务...");
    
    // 停止服务
    stop();
    
    // 释放推理引擎资源
    if (m_inference) {
        try {
            m_inference.reset();
            logInfo("推理引擎资源已释放");
        } catch (const std::exception& e) {
            logError(QString("释放推理引擎时发生异常: %1").arg(e.what()));
        }
    }
    
    // 重置状态
    m_initialized = false;
    
    logInfo("推理服务已关闭");
}

} // namespace SmartScope 