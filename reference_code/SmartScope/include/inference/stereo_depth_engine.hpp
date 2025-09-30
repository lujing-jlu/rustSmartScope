#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include <string>

#include "stereo_depth/comprehensive_depth_processor.hpp"

namespace SmartScope {

// 统一深度引擎：集中初始化并管理综合深度处理器与Q矩阵
class StereoDepthEngine {
public:
    // 使用已有处理器构造（由调用方管理处理器生命周期）
    explicit StereoDepthEngine(stereo_depth::ComprehensiveDepthProcessor* proc)
        : processor_(proc) {}

    // 禁用拷贝，允许移动
    StereoDepthEngine(const StereoDepthEngine&) = delete;
    StereoDepthEngine& operator=(const StereoDepthEngine&) = delete;

    // Q 注入
    void injectQ(const cv::Mat& Q) {
        if (processor_ && !Q.empty()) {
            processor_->setQMatrix(Q);
            cachedQ_ = Q.clone();
        }
    }

    // 获取处理器
    stereo_depth::ComprehensiveDepthProcessor* getProcessor() const { return processor_; }

    // 获取已缓存的Q（若未注入则从处理器获取一份）
    cv::Mat getQ() const {
        if (!cachedQ_.empty()) return cachedQ_.clone();
        if (processor_) return processor_->getQMatrix();
        return cv::Mat();
    }

    // 预热（可选）：给定一对校正后的图像触发一次轻量路径，初始化内部状态
    void warmup(const cv::Mat& left_rectified, const cv::Mat& right_rectified) {
        if (!processor_) return;
        if (left_rectified.empty() || right_rectified.empty()) return;
        // 仅计算视差以初始化SGBM和内部缓存
        (void)processor_->computeDisparityOnly(left_rectified, right_rectified);
    }

private:
    stereo_depth::ComprehensiveDepthProcessor* processor_ = nullptr; // 非拥有指针
    mutable cv::Mat cachedQ_;
};

} // namespace SmartScope 