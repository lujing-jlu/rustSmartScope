#pragma once

#include <opencv2/opencv.hpp>
#include "stereo_depth/comprehensive_depth_processor.hpp"
#include <vector>
#include <limits>

namespace stereo_depth {

/**
 * @brief 改进的深度校准选项
 */
struct ImprovedCalibrationOptions {
    // RANSAC参数
    float ransac_threshold = 5.0f;        // 5毫米误差阈值
    int min_samples = 500;                // 最小样本数
    int min_inliers_ratio = 30;           // 最小内点比例（百分比）
    int max_iterations = 100;             // 最大迭代次数
    
    // 异常值检测
    bool enable_outlier_detection = true;
    float outlier_threshold = 2.0f;       // 2倍标准差阈值
    
    // 深度范围过滤
    bool enable_depth_range_filtering = true;
    float min_depth_mm = 50.0f;           // 最小深度
    float max_depth_mm = 5000.0f;         // 最大深度
    
    // 分层校准
    bool enable_layered_calibration = true;
    int num_depth_layers = 5;             // 深度层数

    // 先验正则（向双目结果贴合）
    double lambda_scale_to_one = 1e-3;    // 正则: (scale-1)^2 权重
    double lambda_bias_to_zero = 1e-3;    // 正则: (bias-0)^2 权重
};

/**
 * @brief 校准点结构
 */
struct CalibrationPoint {
    float mono_depth;      // 单目深度值
    float stereo_depth;    // 双目深度值
    float confidence;      // 置信度权重
    int x, y;             // 像素坐标
    
    CalibrationPoint(float mono, float stereo, float conf, int px, int py)
        : mono_depth(mono), stereo_depth(stereo), confidence(conf), x(px), y(py) {}
};

/**
 * @brief 改进的深度校准器
 * 
 * 提供更精确的深度校准算法，包括：
 * 1. 更严格的RANSAC参数
 * 2. 异常值检测和过滤
 * 3. 深度范围过滤
 * 4. 分层校准
 * 5. 加权最小二乘优化
 */
class ImprovedDepthCalibration {
public:
    ImprovedDepthCalibration();
    ~ImprovedDepthCalibration() = default;
    
    /**
     * @brief 执行深度校准
     * @param mono_depth 单目深度图
     * @param stereo_depth_mm 双目深度图（毫米）
     * @param disparity 视差图
     * @param valid_mask 有效像素掩码
     * @param left_bound_x 左边界X坐标
     * @return 校准结果
     */
    DepthCalibrationResult calibrateDepth(
        const cv::Mat& mono_depth,
        const cv::Mat& stereo_depth_mm,
        const cv::Mat& disparity,
        const cv::Mat& valid_mask,
        int left_bound_x);
    
    /**
     * @brief 设置校准选项
     * @param opts 校准选项
     */
    void setOptions(const ImprovedCalibrationOptions& opts);
    
    /**
     * @brief 获取校准选项
     * @return 当前校准选项
     */
    ImprovedCalibrationOptions getOptions() const;

private:
    /**
     * @brief 收集有效校准点
     */
    std::vector<CalibrationPoint> collectValidPoints(
        const cv::Mat& mono_depth,
        const cv::Mat& stereo_depth_mm,
        const cv::Mat& disparity,
        const cv::Mat& valid_mask,
        int left_bound_x);
    
    /**
     * @brief 计算点的置信度权重
     */
    float calculatePointConfidence(
        float mono_depth, float stereo_depth, float disparity, int x, int y);
    
    /**
     * @brief 按深度范围过滤点
     */
    std::vector<CalibrationPoint> filterByDepthRange(
        const std::vector<CalibrationPoint>& points);
    
    /**
     * @brief 检测并移除异常值
     */
    std::vector<CalibrationPoint> detectAndRemoveOutliers(
        const std::vector<CalibrationPoint>& points);
    
    /**
     * @brief 执行分层校准
     */
    std::vector<DepthCalibrationResult> performLayeredCalibration(
        const std::vector<CalibrationPoint>& points);
    
    /**
     * @brief 创建深度层
     */
    std::vector<std::vector<CalibrationPoint>> createDepthLayers(
        const std::vector<CalibrationPoint>& points);

    /**
     * @brief 基于像素连通域分割有效点（按8邻域且深度差小于阈值）
     */
    std::vector<std::vector<CalibrationPoint>> segmentByConnectivity(
        const std::vector<CalibrationPoint>& points,
        int maxNeighborDistancePixels = 1,
        float maxDepthDiffMm = 50.0f);
    
    /**
     * @brief 校准单个深度层
     */
    DepthCalibrationResult calibrateLayer(
        const std::vector<CalibrationPoint>& points, int layer_index);
    
    /**
     * @brief 选择最佳校准结果
     */
    DepthCalibrationResult selectBestCalibration(
        const std::vector<DepthCalibrationResult>& results);
    
    /**
     * @brief 验证校准质量
     */
    void validateCalibrationQuality(
        DepthCalibrationResult& result, 
        const std::vector<CalibrationPoint>& points);
    
    /**
     * @brief 加权最小二乘拟合
     */
    bool weightedLeastSquares(
        const std::vector<CalibrationPoint>& points,
        double& scale, double& bias);

private:
    ImprovedCalibrationOptions options_;
};

} // namespace stereo_depth
