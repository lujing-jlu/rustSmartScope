#include "stereo_depth/enhanced_postprocessing.h"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <cmath>

namespace stereo_depth {

EnhancedPostProcessor::EnhancedPostProcessor() {
    // 初始化默认参数
    options_.enable_disparity_refinement = true;
    options_.enable_depth_validation = true;
    options_.enable_confidence_based_filtering = true;
    options_.enable_edge_preserving_smoothing = true;
    
    // 视差优化参数
    options_.disparity_consistency_threshold = 2.0f;
    options_.disparity_gradient_threshold = 5.0f;
    options_.disparity_median_kernel = 5;
    
    // 深度验证参数
    options_.min_depth_mm = 10.0f;
    options_.max_depth_mm = 10000.0f;
    options_.depth_consistency_radius = 3;
    options_.depth_consistency_threshold = 50.0f;
    
    // 置信度滤波参数
    options_.confidence_threshold = 0.3f;
    options_.gradient_weight_scale = 100.0f;
    options_.disparity_weight_scale = 50.0f;
    
    // 边缘保持平滑参数
    options_.edge_preserving_sigma_s = 50.0f;
    options_.edge_preserving_sigma_r = 0.1f;
}

cv::Mat EnhancedPostProcessor::processDisparity(const cv::Mat& disparity, 
                                               const cv::Mat& left_gray, 
                                               const cv::Mat& right_gray) {
    if (disparity.empty()) return cv::Mat();
    
    cv::Mat result = disparity.clone();
    
    if (options_.enable_disparity_refinement) {
        result = refineDisparity(result, left_gray, right_gray);
    }
    
    return result;
}

cv::Mat EnhancedPostProcessor::processDepth(const cv::Mat& depth_mm, 
                                           const cv::Mat& disparity,
                                           const cv::Mat& left_gray) {
    if (depth_mm.empty()) return cv::Mat();
    
    cv::Mat result = depth_mm.clone();
    
    if (options_.enable_depth_validation) {
        result = validateDepth(result, disparity);
    }
    
    if (options_.enable_confidence_based_filtering) {
        result = confidenceBasedFiltering(result, disparity, left_gray);
    }
    
    if (options_.enable_edge_preserving_smoothing) {
        result = edgePreservingSmoothing(result, left_gray);
    }
    
    return result;
}

cv::Mat EnhancedPostProcessor::refineDisparity(const cv::Mat& disparity, 
                                              const cv::Mat& left_gray, 
                                              const cv::Mat& right_gray) {
    cv::Mat refined = disparity.clone();
    
    // 1. 左右一致性检查
    cv::Mat left_disp, right_disp;
    cv::Mat left_gray_8u, right_gray_8u;
    
    if (left_gray.type() != CV_8U) {
        left_gray.convertTo(left_gray_8u, CV_8U);
    } else {
        left_gray_8u = left_gray;
    }
    
    if (right_gray.type() != CV_8U) {
        right_gray.convertTo(right_gray_8u, CV_8U);
    } else {
        right_gray_8u = right_gray;
    }
    
    // 计算右到左的视差
    cv::Ptr<cv::StereoSGBM> sgbm_right = cv::StereoSGBM::create(0, 128, 5);
    sgbm_right->setP1(8 * 5 * 5);
    sgbm_right->setP2(32 * 5 * 5);
    sgbm_right->setMode(cv::StereoSGBM::MODE_SGBM_3WAY);
    
    cv::Mat disp16S_right;
    sgbm_right->compute(right_gray_8u, left_gray_8u, disp16S_right);
    cv::Mat disp32F_right;
    disp16S_right.convertTo(disp32F_right, CV_32F, 1.0 / 16.0);
    
    // 左右一致性检查
    cv::Mat consistency_mask = cv::Mat::ones(disparity.size(), CV_8U);
    for (int y = 0; y < disparity.rows; ++y) {
        const float* dL = disparity.ptr<float>(y);
        const float* dR = disp32F_right.ptr<float>(y);
        uchar* mask = consistency_mask.ptr<uchar>(y);
        
        for (int x = 0; x < disparity.cols; ++x) {
            if (dL[x] > 0 && dR[x] > 0) {
                float diff = std::abs(dL[x] - dR[x]);
                if (diff > options_.disparity_consistency_threshold) {
                    mask[x] = 0;
                }
            }
        }
    }
    
    // 2. 梯度一致性检查
    cv::Mat grad_x, grad_y;
    cv::Sobel(left_gray_8u, grad_x, CV_32F, 1, 0);
    cv::Sobel(left_gray_8u, grad_y, CV_32F, 0, 1);
    cv::Mat gradient_magnitude;
    cv::magnitude(grad_x, grad_y, gradient_magnitude);
    
    cv::Mat grad_disp_x, grad_disp_y;
    cv::Sobel(disparity, grad_disp_x, CV_32F, 1, 0);
    cv::Sobel(disparity, grad_disp_y, CV_32F, 0, 1);
    cv::Mat disp_gradient_magnitude;
    cv::magnitude(grad_disp_x, grad_disp_y, disp_gradient_magnitude);
    
    // 梯度一致性掩码
    cv::Mat gradient_mask = cv::Mat::ones(disparity.size(), CV_8U);
    for (int y = 0; y < disparity.rows; ++y) {
        const float* grad = gradient_magnitude.ptr<float>(y);
        const float* disp_grad = disp_gradient_magnitude.ptr<float>(y);
        uchar* mask = gradient_mask.ptr<uchar>(y);
        
        for (int x = 0; x < disparity.cols; ++x) {
            if (grad[x] > options_.disparity_gradient_threshold && 
                disp_grad[x] > options_.disparity_gradient_threshold) {
                // 高梯度区域，检查梯度方向一致性
                float grad_angle = std::atan2(grad_y.at<float>(y, x), grad_x.at<float>(y, x));
                float disp_grad_angle = std::atan2(grad_disp_y.at<float>(y, x), grad_disp_x.at<float>(y, x));
                float angle_diff = std::abs(grad_angle - disp_grad_angle);
                if (angle_diff > M_PI / 4) { // 45度阈值
                    mask[x] = 0;
                }
            }
        }
    }
    
    // 3. 应用掩码
    cv::Mat final_mask = consistency_mask & gradient_mask;
    refined.setTo(0, ~final_mask);
    
    // 4. 中值滤波去噪
    if (options_.disparity_median_kernel > 1) {
        cv::Mat median_filtered;
        cv::medianBlur(refined, median_filtered, options_.disparity_median_kernel);
        
        // 只更新有效区域
        cv::Mat valid_mask = refined > 0;
        median_filtered.copyTo(refined, valid_mask);
    }
    
    return refined;
}

cv::Mat EnhancedPostProcessor::validateDepth(const cv::Mat& depth_mm, 
                                            const cv::Mat& disparity) {
    cv::Mat validated = depth_mm.clone();
    
    // 1. 基本范围检查
    cv::Mat range_mask = (depth_mm >= options_.min_depth_mm) & 
                        (depth_mm <= options_.max_depth_mm);
    
    // 2. 深度一致性检查
    cv::Mat consistency_mask = cv::Mat::ones(depth_mm.size(), CV_8U);
    int radius = options_.depth_consistency_radius;
    float threshold = options_.depth_consistency_threshold;
    
    for (int y = radius; y < depth_mm.rows - radius; ++y) {
        const float* depth_row = depth_mm.ptr<float>(y);
        uchar* mask_row = consistency_mask.ptr<uchar>(y);
        
        for (int x = radius; x < depth_mm.cols - radius; ++x) {
            if (depth_row[x] <= 0) continue;
            
            // 检查邻域深度一致性
            float center_depth = depth_row[x];
            int valid_neighbors = 0;
            int consistent_neighbors = 0;
            
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    
                    float neighbor_depth = depth_mm.at<float>(y + dy, x + dx);
                    if (neighbor_depth > 0) {
                        valid_neighbors++;
                        if (std::abs(neighbor_depth - center_depth) < threshold) {
                            consistent_neighbors++;
                        }
                    }
                }
            }
            
            // 如果一致性邻居比例过低，标记为无效
            if (valid_neighbors > 0) {
                float consistency_ratio = (float)consistent_neighbors / valid_neighbors;
                if (consistency_ratio < 0.3f) { // 30%阈值
                    mask_row[x] = 0;
                }
            }
        }
    }
    
    // 3. 应用所有掩码
    cv::Mat final_mask = range_mask & consistency_mask;
    validated.setTo(0, ~final_mask);
    
    return validated;
}

cv::Mat EnhancedPostProcessor::confidenceBasedFiltering(const cv::Mat& depth_mm, 
                                                       const cv::Mat& disparity,
                                                       const cv::Mat& left_gray) {
    cv::Mat filtered = depth_mm.clone();
    
    // 计算置信度图
    cv::Mat confidence = calculateConfidence(disparity, depth_mm, left_gray);
    
    // 基于置信度进行滤波
    cv::Mat low_confidence_mask = confidence < options_.confidence_threshold;
    filtered.setTo(0, low_confidence_mask);
    
    return filtered;
}

cv::Mat EnhancedPostProcessor::edgePreservingSmoothing(const cv::Mat& depth_mm, 
                                                      const cv::Mat& left_gray) {
    cv::Mat smoothed;
    
    // 使用双边滤波进行边缘保持平滑
    cv::Mat depth_8u;
    depth_mm.convertTo(depth_8u, CV_8U, 255.0 / 10000.0); // 归一化到0-255
    
    cv::bilateralFilter(depth_8u, smoothed, -1, 
                       options_.edge_preserving_sigma_s, 
                       options_.edge_preserving_sigma_r);
    
    // 转换回原始范围
    smoothed.convertTo(smoothed, CV_32F, 10000.0 / 255.0);
    
    // 只更新有效区域
    cv::Mat valid_mask = depth_mm > 0;
    smoothed.copyTo(smoothed, valid_mask);
    
    return smoothed;
}

cv::Mat EnhancedPostProcessor::calculateConfidence(const cv::Mat& disparity, 
                                                  const cv::Mat& depth_mm,
                                                  const cv::Mat& left_gray) {
    cv::Mat confidence = cv::Mat::zeros(disparity.size(), CV_32F);
    
    // 计算梯度
    cv::Mat grad_x, grad_y;
    cv::Sobel(left_gray, grad_x, CV_32F, 1, 0);
    cv::Sobel(left_gray, grad_y, CV_32F, 0, 1);
    cv::Mat gradient_magnitude;
    cv::magnitude(grad_x, grad_y, gradient_magnitude);
    
    for (int y = 0; y < disparity.rows; ++y) {
        const float* disp_ptr = disparity.ptr<float>(y);
        const float* depth_ptr = depth_mm.ptr<float>(y);
        const float* grad_ptr = gradient_magnitude.ptr<float>(y);
        float* conf_ptr = confidence.ptr<float>(y);
        
        for (int x = 0; x < disparity.cols; ++x) {
            if (disp_ptr[x] <= 0 || depth_ptr[x] <= 0) {
                conf_ptr[x] = 0.0f;
                continue;
            }
            
            // 视差质量权重
            float disp_weight = std::min(disp_ptr[x] / options_.disparity_weight_scale, 1.0f);
            
            // 深度范围权重
            float depth_weight = std::exp(-depth_ptr[x] / 1000.0f);
            
            // 梯度一致性权重
            float grad_weight = std::exp(-grad_ptr[x] / options_.gradient_weight_scale);
            
            conf_ptr[x] = disp_weight * depth_weight * grad_weight;
        }
    }
    
    return confidence;
}

void EnhancedPostProcessor::setOptions(const EnhancedPostProcessingOptions& opts) {
    options_ = opts;
}

EnhancedPostProcessingOptions EnhancedPostProcessor::getOptions() const {
    return options_;
}

} // namespace stereo_depth
