#pragma once

#include <opencv2/opencv.hpp>

namespace stereo_depth {

/**
 * @brief 增强后处理选项
 */
struct EnhancedPostProcessingOptions {
    // 视差优化
    bool enable_disparity_refinement = true;
    float disparity_consistency_threshold = 2.0f;
    float disparity_gradient_threshold = 5.0f;
    int disparity_median_kernel = 5;
    
    // 深度验证
    bool enable_depth_validation = true;
    float min_depth_mm = 10.0f;
    float max_depth_mm = 10000.0f;
    int depth_consistency_radius = 3;
    float depth_consistency_threshold = 50.0f;
    
    // 置信度滤波
    bool enable_confidence_based_filtering = true;
    float confidence_threshold = 0.3f;
    float gradient_weight_scale = 100.0f;
    float disparity_weight_scale = 50.0f;
    
    // 边缘保持平滑
    bool enable_edge_preserving_smoothing = true;
    float edge_preserving_sigma_s = 50.0f;
    float edge_preserving_sigma_r = 0.1f;
};

/**
 * @brief 增强后处理器
 * 
 * 提供智能的后处理算法，包括：
 * 1. 视差优化：左右一致性检查、梯度一致性检查
 * 2. 深度验证：范围检查、邻域一致性检查
 * 3. 置信度滤波：基于多因素的置信度计算
 * 4. 边缘保持平滑：保持深度边缘的平滑算法
 */
class EnhancedPostProcessor {
public:
    EnhancedPostProcessor();
    ~EnhancedPostProcessor() = default;
    
    /**
     * @brief 处理视差图
     * @param disparity 输入视差图
     * @param left_gray 左图灰度图
     * @param right_gray 右图灰度图
     * @return 处理后的视差图
     */
    cv::Mat processDisparity(const cv::Mat& disparity, 
                            const cv::Mat& left_gray, 
                            const cv::Mat& right_gray);
    
    /**
     * @brief 处理深度图
     * @param depth_mm 输入深度图（毫米）
     * @param disparity 视差图
     * @param left_gray 左图灰度图
     * @return 处理后的深度图
     */
    cv::Mat processDepth(const cv::Mat& depth_mm, 
                        const cv::Mat& disparity,
                        const cv::Mat& left_gray);
    
    /**
     * @brief 设置处理选项
     * @param opts 处理选项
     */
    void setOptions(const EnhancedPostProcessingOptions& opts);
    
    /**
     * @brief 获取处理选项
     * @return 当前处理选项
     */
    EnhancedPostProcessingOptions getOptions() const;

private:
    /**
     * @brief 视差优化
     */
    cv::Mat refineDisparity(const cv::Mat& disparity, 
                           const cv::Mat& left_gray, 
                           const cv::Mat& right_gray);
    
    /**
     * @brief 深度验证
     */
    cv::Mat validateDepth(const cv::Mat& depth_mm, 
                         const cv::Mat& disparity);
    
    /**
     * @brief 置信度滤波
     */
    cv::Mat confidenceBasedFiltering(const cv::Mat& depth_mm, 
                                    const cv::Mat& disparity,
                                    const cv::Mat& left_gray);
    
    /**
     * @brief 边缘保持平滑
     */
    cv::Mat edgePreservingSmoothing(const cv::Mat& depth_mm, 
                                   const cv::Mat& left_gray);
    
    /**
     * @brief 计算置信度图
     */
    cv::Mat calculateConfidence(const cv::Mat& disparity, 
                               const cv::Mat& depth_mm,
                               const cv::Mat& left_gray);

private:
    EnhancedPostProcessingOptions options_;
};

} // namespace stereo_depth
