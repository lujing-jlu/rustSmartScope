#include "stereo_depth/improved_depth_calibration.h"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <cmath>
#include <random>
#include <glog/logging.h>

namespace stereo_depth {

ImprovedDepthCalibration::ImprovedDepthCalibration() {
    // 设置更严格的校准参数
    options_.ransac_threshold = 5.0f;        // 降低到5毫米
    options_.min_samples = 500;              // 降低样本要求
    options_.min_inliers_ratio = 30;         // 提高内点比例到30%
    options_.max_iterations = 100;           // 增加迭代次数
    options_.enable_outlier_detection = true;
    options_.outlier_threshold = 2.0f;       // 2倍标准差
    options_.enable_depth_range_filtering = true;
    options_.min_depth_mm = 50.0f;           // 最小深度50mm
    options_.max_depth_mm = 5000.0f;         // 最大深度5000mm
}

DepthCalibrationResult ImprovedDepthCalibration::calibrateDepth(
    const cv::Mat& mono_depth,
    const cv::Mat& stereo_depth_mm,
    const cv::Mat& disparity,
    const cv::Mat& valid_mask,
    int left_bound_x) {
    
    DepthCalibrationResult result;
    
    if (mono_depth.empty() || stereo_depth_mm.empty() || disparity.empty()) {
        LOG(ERROR) << "输入深度图为空";
        return result;
    }
    
    // 1. 收集有效点对
    std::vector<CalibrationPoint> validPoints = collectValidPoints(
        mono_depth, stereo_depth_mm, disparity, valid_mask, left_bound_x);
    
    if (validPoints.size() < options_.min_samples) {
        LOG(WARNING) << "有效点数量不足: " << validPoints.size() << " < " << options_.min_samples;
        return result;
    }
    
    // 2. 深度范围过滤
    if (options_.enable_depth_range_filtering) {
        validPoints = filterByDepthRange(validPoints);
        LOG(INFO) << "深度范围过滤后剩余点数: " << validPoints.size();
    }
    
    // 3. 异常值检测
    if (options_.enable_outlier_detection) {
        validPoints = detectAndRemoveOutliers(validPoints);
        LOG(INFO) << "异常值检测后剩余点数: " << validPoints.size();
    }
    
    // 4. 分层校准
    std::vector<DepthCalibrationResult> layerResults = performLayeredCalibration(validPoints);
    
    // 5. 选择最佳校准结果
    result = selectBestCalibration(layerResults);
    
    // 6. 验证校准质量
    validateCalibrationQuality(result, validPoints);
    
    return result;
}

std::vector<CalibrationPoint>
ImprovedDepthCalibration::collectValidPoints(
    const cv::Mat& mono_depth,
    const cv::Mat& stereo_depth_mm,
    const cv::Mat& disparity,
    const cv::Mat& valid_mask,
    int left_bound_x) {
    
    std::vector<CalibrationPoint> points;
    points.reserve(mono_depth.rows * mono_depth.cols / 4);
    
    for (int y = 0; y < mono_depth.rows; ++y) {
        const float* mono_ptr = mono_depth.ptr<float>(y);
        const float* stereo_ptr = stereo_depth_mm.ptr<float>(y);
        const float* disp_ptr = disparity.ptr<float>(y);
        const uchar* mask_ptr = valid_mask.empty() ? nullptr : valid_mask.ptr<uchar>(y);
        
        for (int x = std::max(0, left_bound_x); x < mono_depth.cols; ++x) {
            if (mask_ptr && mask_ptr[x] == 0) continue;
            
            float mono_val = mono_ptr[x];
            float stereo_val = stereo_ptr[x];
            float disp_val = disp_ptr[x];
            
            // 基本有效性检查
            if (!std::isfinite(mono_val) || !std::isfinite(stereo_val) || !std::isfinite(disp_val)) {
                continue;
            }
            
            if (mono_val <= 0.0f || stereo_val <= 0.0f || disp_val <= 0.0f) {
                continue;
            }
            
            // 计算置信度权重
            float confidence = calculatePointConfidence(mono_val, stereo_val, disp_val, x, y);
            
            if (confidence > 0.1f) { // 最低置信度阈值
                points.emplace_back(mono_val, stereo_val, confidence, x, y);
            }
        }
    }
    
    return points;
}

float ImprovedDepthCalibration::calculatePointConfidence(
    float mono_depth, float stereo_depth, float disparity, int x, int y) {
    
    // 1. 视差质量权重
    float disp_weight = std::min(disparity / 50.0f, 1.0f);
    
    // 2. 深度一致性权重
    float depth_ratio = std::min(mono_depth / stereo_depth, stereo_depth / mono_depth);
    float depth_weight = depth_ratio * depth_ratio; // 平方以增强差异
    
    // 3. 位置权重（中心区域权重更高）
    int center_x = 640; // 假设图像宽度1280
    int center_y = 360; // 假设图像高度720
    float dist_to_center = std::sqrt((x - center_x) * (x - center_x) + (y - center_y) * (y - center_y));
    float max_dist = std::sqrt(center_x * center_x + center_y * center_y);
    float position_weight = 1.0f - (dist_to_center / max_dist) * 0.3f; // 边缘区域权重降低30%
    
    return disp_weight * depth_weight * position_weight;
}

std::vector<CalibrationPoint>
ImprovedDepthCalibration::filterByDepthRange(const std::vector<CalibrationPoint>& points) {
    
    std::vector<CalibrationPoint> filtered;
    filtered.reserve(points.size());
    
    for (const auto& point : points) {
        if (point.mono_depth >= options_.min_depth_mm && 
            point.mono_depth <= options_.max_depth_mm &&
            point.stereo_depth >= options_.min_depth_mm && 
            point.stereo_depth <= options_.max_depth_mm) {
            filtered.push_back(point);
        }
    }
    
    return filtered;
}

std::vector<CalibrationPoint>
ImprovedDepthCalibration::detectAndRemoveOutliers(const std::vector<CalibrationPoint>& points) {
    
    if (points.size() < 10) return points;
    
    // 计算深度比值的统计信息
    std::vector<float> ratios;
    ratios.reserve(points.size());
    
    for (const auto& point : points) {
        float ratio = point.stereo_depth / point.mono_depth;
        ratios.push_back(ratio);
    }
    
    // 计算均值和标准差
    float sum = 0.0f;
    for (float ratio : ratios) {
        sum += ratio;
    }
    float mean = sum / ratios.size();
    
    float sum_sq_diff = 0.0f;
    for (float ratio : ratios) {
        float diff = ratio - mean;
        sum_sq_diff += diff * diff;
    }
    float std_dev = std::sqrt(sum_sq_diff / ratios.size());
    
    // 过滤异常值
    std::vector<CalibrationPoint> filtered;
    filtered.reserve(points.size());
    
    float threshold = options_.outlier_threshold * std_dev;
    
    for (size_t i = 0; i < points.size(); ++i) {
        float ratio = ratios[i];
        if (std::abs(ratio - mean) <= threshold) {
            filtered.push_back(points[i]);
        }
    }
    
    LOG(INFO) << "异常值检测: 原始点数=" << points.size()
              << ", 过滤后点数=" << filtered.size()
              << ", 均值=" << mean
              << ", 标准差=" << std_dev;
    
    return filtered;
}

std::vector<DepthCalibrationResult>
ImprovedDepthCalibration::performLayeredCalibration(const std::vector<CalibrationPoint>& points) {
    
    std::vector<DepthCalibrationResult> results;
    
    // 先做连通域分割，再在每个连通域内做深度层分层
    std::vector<std::vector<CalibrationPoint>> ccRegions = segmentByConnectivity(points, 1, 50.0f);
    if (ccRegions.empty()) ccRegions.push_back(points);
    
    std::vector<std::vector<CalibrationPoint>> layers;
    layers.reserve(ccRegions.size() * 4);
    for (const auto& region : ccRegions) {
        if (region.size() < 30) continue; // 小域跳过
        auto regionLayers = createDepthLayers(region);
        for (auto& L : regionLayers) {
            if (L.size() >= 30) layers.push_back(std::move(L));
        }
    }
    if (layers.empty()) layers = createDepthLayers(points);
    
    for (size_t i = 0; i < layers.size(); ++i) {
        if (layers[i].size() < 50) continue; // 每层至少需要50个点
        
        DepthCalibrationResult layerResult = calibrateLayer(layers[i], i);
        if (layerResult.success) {
            results.push_back(layerResult);
        }
    }
    
    return results;
}
std::vector<std::vector<CalibrationPoint>>
ImprovedDepthCalibration::segmentByConnectivity(
    const std::vector<CalibrationPoint>& points,
    int maxNeighborDistancePixels,
    float maxDepthDiffMm) {
    // 将点映射到栅格，使用并查集/DFS聚类
    // 简化实现：基于阈值的DBSCAN近邻聚类（像素曼哈顿距离<=1且深度差<=阈值）
    const int N = (int)points.size();
    std::vector<int> visited(N, 0);
    std::vector<std::vector<CalibrationPoint>> regions;
    regions.reserve(64);

    // 为快速邻域搜索，建立哈希：key=(x,y)
    struct Key { int x; int y; };
    struct KeyHash { size_t operator()(const Key& k) const { return ((size_t)k.x<<32) ^ (unsigned)k.y; } };
    struct KeyEq { bool operator()(const Key& a, const Key& b) const { return a.x==b.x && a.y==b.y; } };
    std::unordered_map<Key, std::vector<int>, KeyHash, KeyEq> grid;
    grid.reserve((size_t)N*2);
    for (int i=0;i<N;++i) {
        grid[{points[i].x, points[i].y}].push_back(i);
    }

    auto neighbors = [&](int idx, std::vector<int>& out){
        out.clear();
        const int cx = points[idx].x;
        const int cy = points[idx].y;
        const float cz = points[idx].stereo_depth; // 使用双目深度的连通准则
        for (int dy=-maxNeighborDistancePixels; dy<=maxNeighborDistancePixels; ++dy) {
            for (int dx=-maxNeighborDistancePixels; dx<=maxNeighborDistancePixels; ++dx) {
                auto it = grid.find({cx+dx, cy+dy});
                if (it==grid.end()) continue;
                for (int j : it->second) {
                    if (j==idx) continue;
                    if (std::abs(points[j].stereo_depth - cz) <= maxDepthDiffMm) {
                        out.push_back(j);
                    }
                }
            }
        }
    };

    std::vector<int> stack;
    std::vector<int> nb; nb.reserve(16);
    for (int i=0;i<N;++i) if (!visited[i]) {
        visited[i]=1; stack.clear(); stack.push_back(i);
        std::vector<CalibrationPoint> region; region.reserve(256);
        region.push_back(points[i]);
        while (!stack.empty()) {
            int u = stack.back(); stack.pop_back();
            neighbors(u, nb);
            for (int v : nb) if (!visited[v]) {
                visited[v]=1; stack.push_back(v); region.push_back(points[v]);
            }
        }
        regions.push_back(std::move(region));
    }
    return regions;
}

std::vector<std::vector<CalibrationPoint>>
ImprovedDepthCalibration::createDepthLayers(const std::vector<CalibrationPoint>& points) {
    
    // 计算深度范围
    float min_depth = std::numeric_limits<float>::max();
    float max_depth = std::numeric_limits<float>::min();
    
    for (const auto& point : points) {
        min_depth = std::min(min_depth, point.mono_depth);
        max_depth = std::max(max_depth, point.mono_depth);
    }
    
    // 创建5个深度层
    const int num_layers = 5;
    float layer_size = (max_depth - min_depth) / num_layers;
    
    std::vector<std::vector<CalibrationPoint>> layers(num_layers);
    
    for (const auto& point : points) {
        int layer = std::min(static_cast<int>((point.mono_depth - min_depth) / layer_size), num_layers - 1);
        layers[layer].push_back(point);
    }
    
    return layers;
}

DepthCalibrationResult
ImprovedDepthCalibration::calibrateLayer(const std::vector<CalibrationPoint>& points, int layer_index) {
    
    DepthCalibrationResult result;
    result.layer_index = layer_index;
    
    if (points.size() < 10) {
        return result;
    }
    
    // 使用加权RANSAC进行校准
    double best_scale = 1.0, best_bias = 0.0;
    int best_inliers = 0;
    int min_inliers = std::max(10, static_cast<int>(points.size() * 0.3)); // 30%内点要求
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, points.size() - 1);
    
    for (int iter = 0; iter < options_.max_iterations; ++iter) {
        // 随机选择两个点
        int idx1 = dis(gen);
        int idx2 = dis(gen);
        while (idx2 == idx1) idx2 = dis(gen);
        
        const auto& p1 = points[idx1];
        const auto& p2 = points[idx2];
        
        // 计算线性参数
        if (std::abs(p2.mono_depth - p1.mono_depth) < 1e-6f) continue;
        
        double scale = (p2.stereo_depth - p1.stereo_depth) / (p2.mono_depth - p1.mono_depth);
        double bias = p1.stereo_depth - scale * p1.mono_depth;
        
        if (!std::isfinite(scale) || !std::isfinite(bias)) continue;
        
        // 计算内点数量
        int inliers = 0;
        for (const auto& point : points) {
            double predicted = scale * point.mono_depth + bias;
            double error = std::abs(point.stereo_depth - predicted);
            if (error < options_.ransac_threshold) {
                inliers++;
            }
        }
        
        if (inliers > best_inliers && inliers >= min_inliers) {
            best_inliers = inliers;
            best_scale = scale;
            best_bias = bias;
        }
    }
    
    if (best_inliers < min_inliers) {
        return result;
    }
    
    // 使用内点进行加权最小二乘优化
    std::vector<CalibrationPoint> inliers;
    inliers.reserve(points.size());
    
    for (const auto& point : points) {
        double predicted = best_scale * point.mono_depth + best_bias;
        double error = std::abs(point.stereo_depth - predicted);
        if (error < options_.ransac_threshold) {
            inliers.push_back(point);
        }
    }
    
    // 加权最小二乘拟合
    double scale_weighted, bias_weighted;
    if (weightedLeastSquares(inliers, scale_weighted, bias_weighted)) {
        result.scale_factor = scale_weighted;
        result.bias = bias_weighted;
    } else {
        result.scale_factor = best_scale;
        result.bias = best_bias;
    }
    
    result.success = true;
    result.total_points = points.size();
    result.inlier_points = inliers.size();
    
    // 计算RMS误差
    double sum_squared_error = 0.0;
    for (const auto& point : inliers) {
        double predicted = result.scale_factor * point.mono_depth + result.bias;
        double error = point.stereo_depth - predicted;
        sum_squared_error += error * error;
    }
    result.rms_error = std::sqrt(sum_squared_error / inliers.size());
    
    return result;
}

bool ImprovedDepthCalibration::weightedLeastSquares(
    const std::vector<CalibrationPoint>& points,
    double& scale, double& bias) {
    
    if (points.size() < 2) return false;
    
    double sum_w = 0.0, sum_wx = 0.0, sum_wy = 0.0, sum_wxx = 0.0, sum_wxy = 0.0;
    
    for (const auto& point : points) {
        double w = point.confidence;
        double x = point.mono_depth;
        double y = point.stereo_depth;
        
        sum_w += w;
        sum_wx += w * x;
        sum_wy += w * y;
        sum_wxx += w * x * x;
        sum_wxy += w * x * y;
    }
    
    // 加入先验正则，使解更贴合双目: (scale-1)^2 与 (bias-0)^2
    const double ls = options_.lambda_scale_to_one;
    const double lb = options_.lambda_bias_to_zero;
    
    // 正则后法方程：
    // [[sum_wxx + ls, sum_wx     ], [sum_wx, sum_w + lb]] * [scale, bias]^T = [sum_wxy + ls*1, sum_wy]^T
    double A11 = sum_wxx + ls;
    double A12 = sum_wx;
    double A22 = sum_w + lb;
    double B1  = sum_wxy + ls * 1.0; // 期望scale≈1
    double B2  = sum_wy;             // 期望bias≈0
    
    double det = A11 * A22 - A12 * A12;
    if (std::abs(det) < 1e-8) return false;
    
    scale = ( B1 * A22 - A12 * B2) / det;
    bias  = (-B1 * A12 + A11 * B2) / det;
    
    return std::isfinite(scale) && std::isfinite(bias);
}

DepthCalibrationResult
ImprovedDepthCalibration::selectBestCalibration(const std::vector<DepthCalibrationResult>& results) {
    
    if (results.empty()) {
        return DepthCalibrationResult();
    }
    
    // 选择RMS误差最小的结果
    auto best_result = std::min_element(results.begin(), results.end(),
        [](const DepthCalibrationResult& a, const DepthCalibrationResult& b) {
            return a.rms_error < b.rms_error;
        });
    
    return *best_result;
}

void ImprovedDepthCalibration::validateCalibrationQuality(
    DepthCalibrationResult& result, const std::vector<CalibrationPoint>& points) {
    
    if (!result.success) return;
    
    // 检查校准参数的合理性
    if (result.scale_factor < 0.5 || result.scale_factor > 2.0) {
        LOG(WARNING) << "校准比例因子异常: " << result.scale_factor;
        result.success = false;
        return;
    }
    
    if (std::abs(result.bias) > 1000.0) { // 偏置过大
        LOG(WARNING) << "校准偏置异常: " << result.bias;
        result.success = false;
        return;
    }
    
    // 检查RMS误差
    if (result.rms_error > 20.0) { // RMS误差过大
        LOG(WARNING) << "校准RMS误差过大: " << result.rms_error;
        result.success = false;
        return;
    }
    
    LOG(INFO) << "深度校准成功: 比例因子=" << result.scale_factor
              << ", 偏置=" << result.bias
              << ", RMS误差=" << result.rms_error
              << ", 内点数=" << result.inlier_points << "/" << result.total_points;
}

void ImprovedDepthCalibration::setOptions(const ImprovedCalibrationOptions& opts) {
    options_ = opts;
}

ImprovedCalibrationOptions ImprovedDepthCalibration::getOptions() const {
    return options_;
}

} // namespace stereo_depth
