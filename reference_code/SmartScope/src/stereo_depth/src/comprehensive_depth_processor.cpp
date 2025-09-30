#include "stereo_depth/comprehensive_depth_processor.hpp"
#include "stereo_depth/improved_depth_calibration.h"
#include <glog/logging.h>
#include "depth_anything_inference.hpp"
#include "stereo_depth/enhanced_postprocessing.h"
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include <iostream>

namespace stereo_depth {

using cv::Mat;
using cv::Size;
using std::string;

// 静态函数：读取3x3矩阵
static bool readMatrix3x3(std::istream& in, Mat& M) {
    double a00, a01, a02, a10, a11, a12, a20, a21, a22;
    if (!(in >> a00 >> a01 >> a02)) return false;
    if (!(in >> a10 >> a11 >> a12)) return false;
    if (!(in >> a20 >> a21 >> a22)) return false;
    M = (cv::Mat_<double>(3, 3) << a00, a01, a02, a10, a11, a12, a20, a21, a22);
    return true;
}

ComprehensiveDepthProcessor::ComprehensiveDepthProcessor(const std::string& camera_param_dir,
                                                       const std::string& mono_model_path,
                                                       const ComprehensiveDepthOptions& options)
    : camera_param_dir_(camera_param_dir), mono_model_path_(mono_model_path), options_(options) {
    // 初始化增强后处理器
    enhanced_postprocessor_ = std::make_unique<EnhancedPostProcessor>();
    initialize();
}

void ComprehensiveDepthProcessor::setQMatrix(const cv::Mat& Q_matrix) {
    if (Q_matrix.empty()) return;
    if (Q_matrix.size() == cv::Size(4,4) || Q_matrix.size() == cv::Size(3,4)) {
        Q_ = Q_matrix.clone();
    }
}

ComprehensiveDepthProcessor::~ComprehensiveDepthProcessor() {
    if (mono_engine_) {
        mono_engine_->StopPipeline();
        mono_engine_->ClosePipeline();
    }
}

bool ComprehensiveDepthProcessor::readIntrinsics(const std::string& path, Mat& K, Mat& D) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "[stereo_depth] readIntrinsics: open failed: " << path << std::endl;
        return false;
    }
    std::cerr << "[stereo_depth] readIntrinsics: open ok: " << path << std::endl;
    
    string tag;
    std::getline(in, tag); // intrinsic:
    std::cerr << "[stereo_depth] readIntrinsics: first line: '" << tag << "'" << std::endl;
    if (!readMatrix3x3(in, K)) {
        std::cerr << "[stereo_depth] readIntrinsics: readMatrix3x3 failed for K from: " << path << std::endl;
        return false;
    }
    std::cerr << "[stereo_depth] readIntrinsics: K= " << K.at<double>(0,0) << "," << K.at<double>(1,1) << std::endl;
    
    std::getline(in, tag); // consume endline
    std::getline(in, tag); // distortion:
    std::cerr << "[stereo_depth] readIntrinsics: tag for distortion: '" << tag << "'" << std::endl;
    
    // 读取可能跨行的畸变参数，兼容逗号与空格分隔，直到至少收集到5个或到达文件末尾
    std::vector<double> coeffs;
    for (int lineCount = 0; lineCount < 8 && in.good(); ++lineCount) {
        std::string line;
        if (!std::getline(in, line)) break;
        // 将非数值字符(除 . - + e E 空格)统一替换为空格，便于解析
        for (char& c : line) {
            if (!( (c >= '0' && c <= '9') || c==' ' || c=='\t' || c=='.' || c=='-' || c=='+' || c=='e' || c=='E')) {
                c = ' ';
            }
        }
        std::istringstream ss(line);
    double t;
    while (ss >> t) coeffs.push_back(t);
        if (coeffs.size() >= 5) break;
    }
    std::cerr << "[stereo_depth] readIntrinsics: distortion coeff count (raw) = " << coeffs.size() << std::endl;

    // 只保留前5个参数，若不足则补零
    std::array<double,5> dvals{0,0,0,0,0};
    for (size_t i = 0; i < std::min(coeffs.size(), size_t(5)); ++i) dvals[i] = coeffs[i];

    D = Mat(1, 5, CV_64F);
    for (int i = 0; i < 5; ++i) D.at<double>(0, i) = dvals[i];

    return true;
}

bool ComprehensiveDepthProcessor::readRotTrans(const std::string& path, Mat& R, Mat& T) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "[stereo_depth] readRotTrans: open failed: " << path << std::endl;
        return false;
    }
    std::cerr << "[stereo_depth] readRotTrans: open ok: " << path << std::endl;
    
    string tag;
    std::getline(in, tag); // R:
    if (!readMatrix3x3(in, R)) {
        std::cerr << "[stereo_depth] readRotTrans: readMatrix3x3 failed for R from: " << path << std::endl;
        return false;
    }
    
    std::getline(in, tag); // T:
    double tx = 0, ty = 0, tz = 0;
    in >> tx >> ty >> tz;
    T = (cv::Mat_<double>(3, 1) << tx, ty, tz);
    return true;
}

void ComprehensiveDepthProcessor::initialize() {
    if (initialized_) return;
    
    std::cerr << "[stereo_depth] initialize(): camera_param_dir_ = " << camera_param_dir_ << std::endl;
    
    // 读取相机参数
    std::string p0 = camera_param_dir_ + "/camera0_intrinsics.dat";
    std::string p1 = camera_param_dir_ + "/camera1_intrinsics.dat";
    std::string pr = camera_param_dir_ + "/camera1_rot_trans.dat";
    std::cerr << "[stereo_depth] initialize(): read " << p0 << std::endl;
    if (!readIntrinsics(p0, K0_, D0_))
        throw std::runtime_error("读取 camera0_intrinsics.dat 失败");
    std::cerr << "[stereo_depth] initialize(): read " << p1 << std::endl;
    if (!readIntrinsics(p1, K1_, D1_))
        throw std::runtime_error("读取 camera1_intrinsics.dat 失败");
    std::cerr << "[stereo_depth] initialize(): read " << pr << std::endl;
    if (!readRotTrans(pr, R_, T_))
        throw std::runtime_error("读取 camera1_rot_trans.dat 失败");
    
    // 初始化单目深度模型
    try {
        std::cerr << "[stereo_depth] initialize(): create mono_engine with model: " << mono_model_path_ << std::endl;
        mono_engine_ = depth_anything::CreateRknnInferCore(mono_model_path_);
        mono_model_ = depth_anything::CreateDepthAnythingModel(mono_engine_, 518, 518);
        mono_engine_->InitPipeline();
        std::cerr << "[stereo_depth] initialize(): mono_engine initialized ok" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "单目模型初始化失败: " << e.what() << std::endl;
        throw;
    }
    
    initialized_ = true;
}

void ComprehensiveDepthProcessor::preprocessImage(const cv::Mat& src, cv::Mat& dst) {
    if (src.empty()) {
        dst.release();
        return;
    }
    
    // 最小化预处理：只进行必要的缩放，移除所有滤波
    if (options_.scale > 0 && std::abs(options_.scale - 1.0) > 1e-6) {
        Size newSize(static_cast<int>(src.cols * options_.scale), 
                    static_cast<int>(src.rows * options_.scale));
        int interp = (options_.scale < 1.0) ? cv::INTER_AREA : cv::INTER_LINEAR;
        cv::resize(src, dst, newSize, 0, 0, interp);
    } else {
        dst = src.clone();
    }
}

float ComprehensiveDepthProcessor::calculateConfidenceWeight(float disparity, float depth, float gradient) const {
    // 视差质量权重
    float dispWeight = std::min(std::max(disparity / options_.disparity_weight_scale, 0.1f), 1.0f);
    
    // 深度范围权重
    float depthWeight = std::exp(-depth / options_.depth_weight_scale);
    
    // 梯度一致性权重
    float gradWeight = std::exp(-gradient / options_.gradient_weight_scale);
    
    return dispWeight * depthWeight * gradWeight;
}

bool ComprehensiveDepthProcessor::ransacLinearFit(const std::vector<std::tuple<float, float, float>>& points,
                                                 double& s_out, double& b_out) {
    if (points.size() < 2) return false;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, points.size() - 1);
    
    double bestS = 1.0, bestB = 0.0;
    int bestInliers = 0;
    int minInliers = std::max(10, (int)(points.size() * options_.min_inliers_ratio / 100));
    
    for (int iter = 0; iter < options_.ransac_max_iterations; ++iter) {
        // 随机选择两个点
        int idx1 = dis(gen);
        int idx2 = dis(gen);
        while (idx2 == idx1) idx2 = dis(gen);
        
        float x1 = std::get<0>(points[idx1]);
        float y1 = std::get<1>(points[idx1]);
        float x2 = std::get<0>(points[idx2]);
        float y2 = std::get<1>(points[idx2]);
        
        // 计算线性参数
        if (std::abs(x2 - x1) < 1e-6f) continue;
        double s = (y2 - y1) / (x2 - x1);
        double b = y1 - s * x1;
        
        if (!std::isfinite(s) || !std::isfinite(b)) continue;
        
        // 计算内点数量
        int inliers = 0;
        for (const auto& point : points) {
            float x = std::get<0>(point);
            float y = std::get<1>(point);
            float predicted = s * x + b;
            float error = std::abs(y - predicted);
            if (error < options_.ransac_threshold) {
                inliers++;
            }
        }
        
        // 更新最佳模型
        if (inliers > bestInliers && inliers >= minInliers) {
            bestInliers = inliers;
            bestS = s;
            bestB = b;
        }
    }
    
    if (bestInliers < minInliers) return false;
    
    s_out = bestS;
    b_out = bestB;
    return true;
}

bool ComprehensiveDepthProcessor::weightedLinearFit(const std::vector<std::tuple<float, float, float>>& points,
                                                   double& s_out, double& b_out) {
    if (points.empty()) return false;
    
    double sumW = 0.0, sumWX = 0.0, sumWY = 0.0, sumWXX = 0.0, sumWXY = 0.0;
    
    for (const auto& point : points) {
        float x = std::get<0>(point);
        float y = std::get<1>(point);
        float w = std::get<2>(point);
        
        sumW += w;
        sumWX += w * x;
        sumWY += w * y;
        sumWXX += w * x * x;
        sumWXY += w * x * y;
    }
    
    if (sumW < 1e-6) return false;
    
    double denom = sumW * sumWXX - sumWX * sumWX;
    if (std::abs(denom) < 1e-8) return false;
    
    double s = (sumW * sumWXY - sumWX * sumWY) / denom;
    double b = (sumWY - s * sumWX) / sumW;
    
    s_out = s;
    b_out = b;
    return std::isfinite(s_out) && std::isfinite(b_out);
}

// 新增：异常值检测函数
cv::Mat ComprehensiveDepthProcessor::detectAnomalies(const cv::Mat& depth, const cv::Mat& disparity, 
                                                    float local_threshold, int window_size) {
    cv::Mat anomalies = cv::Mat::zeros(depth.size(), CV_8U);
    
    if (depth.empty() || disparity.empty()) return anomalies;
    
    // 局部统计异常检测
    cv::Mat local_mean, local_std;
    cv::boxFilter(depth, local_mean, CV_32F, cv::Size(window_size, window_size));
    cv::Mat diff = depth - local_mean;
    cv::Mat diff_squared;
    cv::multiply(diff, diff, diff_squared);
    cv::boxFilter(diff_squared, local_std, CV_32F, cv::Size(window_size, window_size));
    cv::sqrt(local_std, local_std);
    
    // 标记异常点（偏离局部均值过大的点）
    cv::Mat anomaly_mask = cv::abs(diff) > (local_threshold * local_std);
    anomalies.setTo(255, anomaly_mask);
    
    // 空间连续性检查：孤立点更可能是异常
    cv::Mat isolated;
    cv::morphologyEx(anomalies, isolated, cv::MORPH_OPEN, 
                    cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3,3)));
    
    // 结合视差一致性检查
    cv::Mat disparity_anomalies = cv::Mat::zeros(disparity.size(), CV_8U);
    cv::Mat disp_grad_x, disp_grad_y;
    cv::Sobel(disparity, disp_grad_x, CV_32F, 1, 0);
    cv::Sobel(disparity, disp_grad_y, CV_32F, 0, 1);
    cv::Mat disp_gradient;
    cv::magnitude(disp_grad_x, disp_grad_y, disp_gradient);
    
    // 视差梯度异常大的区域
    double max_grad = 0;
    cv::minMaxLoc(disp_gradient, nullptr, &max_grad);
    cv::Mat high_gradient = disp_gradient > (0.3 * max_grad);
    disparity_anomalies.setTo(255, high_gradient);
    
    // 合并两种异常检测结果
    cv::Mat combined_anomalies;
    cv::bitwise_or(isolated, disparity_anomalies, combined_anomalies);
    
    return combined_anomalies;
}

// 新增：孔洞区域检测
cv::Mat ComprehensiveDepthProcessor::detectHoleRegions(const cv::Mat& depth, const cv::Mat& disparity,
                                                      float hole_depth_threshold, int min_hole_size) {
    cv::Mat hole_mask = cv::Mat::zeros(depth.size(), CV_8U);
    
    if (depth.empty() || disparity.empty()) return hole_mask;
    
    // 检测深孔区域（深度值较大的连续区域）
    cv::Mat deep_regions = (depth > hole_depth_threshold) & (disparity > 0);
    
    // 形态学操作找到连续区域
    cv::Mat connected;
    cv::morphologyEx(deep_regions, connected, cv::MORPH_CLOSE, 
                    cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5,5)));
    
    // 连通组件分析，找到足够大的孔洞区域
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(connected, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    for (const auto& contour : contours) {
        if (cv::contourArea(contour) > min_hole_size) {
            cv::fillPoly(hole_mask, std::vector<std::vector<cv::Point>>{contour}, 255);
        }
    }
    
    return hole_mask;
}

// 新增：自适应权重计算
cv::Mat ComprehensiveDepthProcessor::calculateAdaptiveWeights(const cv::Mat& mono_depth,
                                                             const cv::Mat& stereo_depth_mm,
                                                             const cv::Mat& disparity,
                                                             const cv::Mat& anomalies) {
    cv::Mat weights = cv::Mat::ones(stereo_depth_mm.size(), CV_32F);
    
    if (stereo_depth_mm.empty()) return weights;
    
    const int rows = stereo_depth_mm.rows;
    const int cols = stereo_depth_mm.cols;
    
    for (int y = 0; y < rows; ++y) {
        const float* dptr = disparity.ptr<float>(y);
        const float* depth_ptr = stereo_depth_mm.ptr<float>(y);
        const uchar* aptr = anomalies.empty() ? nullptr : anomalies.ptr<uchar>(y);
        float* wptr = weights.ptr<float>(y);
        
        for (int x = 0; x < cols; ++x) {
            float weight = 1.0f;
            
            // 异常值权重降低
            if (aptr && aptr[x] > 0) {
                weight *= 0.1f;
            }
            
            // 视差质量权重
            if (dptr[x] > 0) {
                float disp_quality = std::min(dptr[x] / 50.0f, 1.0f); // 视差越大质量越好
                weight *= disp_quality;
            }
            
            // 深度梯度权重（边缘区域权重降低）
            if (x > 0 && x < cols - 1 && y > 0 && y < rows - 1) {
                float grad_x = std::abs(depth_ptr[x+1] - depth_ptr[x-1]) / 2.0f;
                float grad_y = std::abs(stereo_depth_mm.ptr<float>(y+1)[x] - stereo_depth_mm.ptr<float>(y-1)[x]) / 2.0f;
                float gradient = std::sqrt(grad_x * grad_x + grad_y * grad_y);
                
                // 梯度过大的区域权重降低（可能是噪声）
                if (gradient > 100.0f) {
                    weight *= 0.5f;
                }
            }
            
            wptr[x] = weight;
        }
    }
    
    return weights;
}

// 新增：分层校准
DepthCalibrationResult ComprehensiveDepthProcessor::calibrateDepthLayered(const cv::Mat& mono_depth,
                                                                          const cv::Mat& stereo_depth_mm,
                                                                          const cv::Mat& disparity,
                                                                          const cv::Mat& valid_mask,
                                                                          int left_bound_x) {
    DepthCalibrationResult result;
    
    if (mono_depth.empty() || stereo_depth_mm.empty() || disparity.empty()) {
        return result;
    }
    
    // 检测异常值
    cv::Mat anomalies = detectAnomalies(stereo_depth_mm, disparity, 2.0f, 5);
    
    // 检测孔洞区域
    cv::Mat hole_mask = detectHoleRegions(stereo_depth_mm, disparity, 500.0f, 50);
    
    // 计算自适应权重
    cv::Mat adaptive_weights = calculateAdaptiveWeights(mono_depth, stereo_depth_mm, disparity, anomalies);
    
    // 分层校准（相机工作距离近：焦距<10cm，针对0-120mm加密分层）
    std::vector<DepthCalibrationResult> layer_results;
    std::vector<float> depth_ranges = {
        // 近距高精度层（单位：mm）
        0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120,
        // 过渡层（防止边界外偶发值）
        140, 170, 210, 260, 320, 400, 550, 750, 1000, 1500, 2500, 5000, 10000
    };
    
    for (size_t i = 0; i < depth_ranges.size() - 1; ++i) {
        cv::Mat layer_mask = (stereo_depth_mm >= depth_ranges[i]) & 
                           (stereo_depth_mm < depth_ranges[i + 1]) & 
                           (anomalies == 0) &
                           (valid_mask.empty() ? cv::Mat::ones(stereo_depth_mm.size(), CV_8U) : valid_mask);
        
        int sample_count = cv::countNonZero(layer_mask);
        if (sample_count > 50) { // 降低最小样本数要求
            auto layer_result = calibrateDepthLayer(mono_depth, stereo_depth_mm, 
                                                  disparity, layer_mask, adaptive_weights);
            layer_result.layer_index = i;
            layer_result.depth_range_min = depth_ranges[i];
            layer_result.depth_range_max = depth_ranges[i + 1];
            layer_result.sample_count = sample_count;
            layer_results.push_back(layer_result);
        }
    }
    
    // 孔洞区域特殊校准
    if (cv::countNonZero(hole_mask) > 20) {
        auto hole_result = calibrateHoleRegions(mono_depth, stereo_depth_mm, disparity, hole_mask);
        hole_result.layer_index = -1; // 特殊标记
        layer_results.push_back(hole_result);
    }
    
    // 融合各层结果
    if (layer_results.empty()) {
        // 回退到原始校准
        return calibrateDepth(mono_depth, stereo_depth_mm, disparity, valid_mask, left_bound_x);
    }
    
    return fuseLayerResults(layer_results, stereo_depth_mm);
}

// 新增：单层校准
DepthCalibrationResult ComprehensiveDepthProcessor::calibrateDepthLayer(const cv::Mat& mono_depth,
                                                                        const cv::Mat& stereo_depth_mm,
                                                                        const cv::Mat& disparity,
                                                                        const cv::Mat& layer_mask,
                                                                        const cv::Mat& weights) {
    DepthCalibrationResult result;
    
    if (mono_depth.empty() || stereo_depth_mm.empty() || disparity.empty()) {
        return result;
    }
    
    const int rows = mono_depth.rows;
    const int cols = mono_depth.cols;
    
    // 收集有效点对（使用自适应权重）
    std::vector<std::tuple<float, float, float>> validPoints;
    validPoints.reserve(rows * cols / 8);
    
    for (int y = 0; y < rows; ++y) {
        const float* mptr = mono_depth.ptr<float>(y);
        const float* sptr = stereo_depth_mm.ptr<float>(y);
        const float* dptr = disparity.ptr<float>(y);
        const float* wptr = weights.empty() ? nullptr : weights.ptr<float>(y);
        const uchar* mask_ptr = layer_mask.empty() ? nullptr : layer_mask.ptr<uchar>(y);
        
        for (int x = 0; x < cols; ++x) {
            if (mask_ptr && mask_ptr[x] == 0) continue;
            
            float mv = mptr[x];
            float sv = sptr[x];
            float dv = dptr[x];
            float weight = wptr ? wptr[x] : 1.0f;
            
            if (!std::isfinite(mv) || !std::isfinite(sv) || !std::isfinite(dv)) continue;
            if (mv <= 0.0f || sv <= 0.0f || dv <= 0.0f) continue;
            if (weight < 0.1f) continue; // 权重过低的点跳过
            
            validPoints.emplace_back(mv, sv, weight);
        }
    }
    
    result.total_points = validPoints.size();
    if (validPoints.size() < 20) { // 降低最小样本数
        return result;
    }
    
    // 加权RANSAC拟合
    double s_ransac = 1.0, b_ransac = 0.0;
    bool ransacSuccess = ransacLinearFit(validPoints, s_ransac, b_ransac);
    
    if (!ransacSuccess) {
        // 回退到加权最小二乘
        double s_weighted = 1.0, b_weighted = 0.0;
        bool weightedSuccess = weightedLinearFit(validPoints, s_weighted, b_weighted);
        
        if (weightedSuccess) {
            result.scale_factor = s_weighted;
            result.bias = b_weighted;
            result.success = true;
        }
        return result;
    }
    
    // 使用RANSAC内点进行加权最小二乘
    std::vector<std::tuple<float, float, float>> inliers;
    inliers.reserve(validPoints.size());
    
    for (const auto& point : validPoints) {
        float x = std::get<0>(point);
        float y = std::get<1>(point);
        float w = std::get<2>(point);
        float predicted = s_ransac * x + b_ransac;
        float error = std::abs(y - predicted);
        
        if (error < options_.ransac_threshold) {
            inliers.emplace_back(x, y, w);
        }
    }
    
    result.inlier_points = inliers.size();
    
    if (inliers.size() < 10) { // 降低内点要求
        result.scale_factor = s_ransac;
        result.bias = b_ransac;
        result.success = true;
        return result;
    }
    
    // 加权最小二乘拟合
    double s_weighted = 1.0, b_weighted = 0.0;
    bool weightedSuccess = weightedLinearFit(inliers, s_weighted, b_weighted);
    
    if (weightedSuccess) {
        result.scale_factor = s_weighted;
        result.bias = b_weighted;
    } else {
        result.scale_factor = s_ransac;
        result.bias = b_ransac;
    }
    
    result.success = true;
    
    // 计算RMS误差
    double sumSquaredError = 0.0;
    int validCount = 0;
    for (const auto& point : inliers) {
        float x = std::get<0>(point);
        float y = std::get<1>(point);
        float predicted = result.scale_factor * x + result.bias;
        sumSquaredError += (y - predicted) * (y - predicted);
        validCount++;
    }
    if (validCount > 0) {
        result.rms_error = std::sqrt(sumSquaredError / validCount);
    }
    
    return result;
}

// 新增：孔洞区域校准
DepthCalibrationResult ComprehensiveDepthProcessor::calibrateHoleRegions(const cv::Mat& mono_depth,
                                                                         const cv::Mat& stereo_depth_mm,
                                                                         const cv::Mat& disparity,
                                                                         const cv::Mat& hole_mask) {
    DepthCalibrationResult result;
    
    if (mono_depth.empty() || stereo_depth_mm.empty() || disparity.empty() || hole_mask.empty()) {
        return result;
    }
    
    // 对每个孔洞区域进行局部校准
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(hole_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    std::vector<DepthCalibrationResult> hole_results;
    
    for (const auto& contour : contours) {
        cv::Rect bbox = cv::boundingRect(contour);
        
        // 扩展边界以包含更多上下文
        cv::Rect expanded_bbox = bbox + cv::Size(40, 40);
        expanded_bbox &= cv::Rect(0, 0, stereo_depth_mm.cols, stereo_depth_mm.rows);
        
        cv::Mat local_mono = mono_depth(expanded_bbox);
        cv::Mat local_stereo = stereo_depth_mm(expanded_bbox);
        cv::Mat local_disparity = disparity(expanded_bbox);
        cv::Mat local_mask = cv::Mat::zeros(expanded_bbox.size(), CV_8U);
        
        // 将孔洞轮廓转换到扩展边界坐标系
        std::vector<cv::Point> shifted_contour;
        for (const auto& pt : contour) {
            shifted_contour.push_back(cv::Point(pt.x - expanded_bbox.x, pt.y - expanded_bbox.y));
        }
        cv::fillPoly(local_mask, std::vector<std::vector<cv::Point>>{shifted_contour}, 255);
        
        if (cv::countNonZero(local_mask) > 10) { // 降低孔洞最小样本数
            auto local_result = calibrateDepthLayer(local_mono, local_stereo, 
                                                  local_disparity, local_mask);
            if (local_result.success) {
                hole_results.push_back(local_result);
            }
        }
    }
    
    if (hole_results.empty()) {
        return result;
    }
    
    // 融合孔洞校准结果（使用中位数避免异常值影响）
    std::vector<double> scales, biases;
    for (const auto& hole_result : hole_results) {
        if (hole_result.success) {
            scales.push_back(hole_result.scale_factor);
            biases.push_back(hole_result.bias);
        }
    }
    
    if (scales.empty()) {
        return result;
    }
    
    // 使用中位数作为最终结果
    std::sort(scales.begin(), scales.end());
    std::sort(biases.begin(), biases.end());
    
    result.scale_factor = scales[scales.size() / 2];
    result.bias = biases[biases.size() / 2];
    result.success = true;
    result.total_points = hole_results.size();
    result.inlier_points = hole_results.size();
    
    return result;
}

// 新增：融合分层结果
DepthCalibrationResult ComprehensiveDepthProcessor::fuseLayerResults(const std::vector<DepthCalibrationResult>& layer_results,
                                                                     const cv::Mat& stereo_depth_mm) {
    DepthCalibrationResult result;
    
    if (layer_results.empty()) {
        return result;
    }
    
    // 按深度范围加权融合
    double total_weight = 0.0;
    double weighted_scale = 0.0;
    double weighted_bias = 0.0;
    
    for (const auto& layer_result : layer_results) {
        if (!layer_result.success) continue;
        
        // 计算该层的权重（基于样本数量和质量）
        double weight = layer_result.inlier_points;
        if (layer_result.rms_error > 0) {
            weight /= (1.0 + layer_result.rms_error / 100.0); // RMS误差越小权重越大
        }
        
        // 孔洞区域给予更高权重
        if (layer_result.layer_index == -1) {
            weight *= 2.0;
        }
        
        weighted_scale += layer_result.scale_factor * weight;
        weighted_bias += layer_result.bias * weight;
        total_weight += weight;
    }
    
    if (total_weight > 0) {
        result.scale_factor = weighted_scale / total_weight;
        result.bias = weighted_bias / total_weight;
        result.success = true;
        result.total_points = layer_results.size();
        result.inlier_points = static_cast<int>(total_weight);
    }
    
    return result;
}

DepthCalibrationResult ComprehensiveDepthProcessor::calibrateDepth(const cv::Mat& mono_depth,
                                                                  const cv::Mat& stereo_depth_mm,
                                                                  const cv::Mat& disparity,
                                                                  const cv::Mat& valid_mask,
                                                                  int left_bound_x) {
    DepthCalibrationResult result;
    
    if (mono_depth.empty() || stereo_depth_mm.empty() || disparity.empty()) {
        return result;
    }
    
    const int rows = mono_depth.rows;
    const int cols = mono_depth.cols;
    
    // 计算梯度
    cv::Mat gradX, gradY;
    cv::Sobel(stereo_depth_mm, gradX, CV_32F, 1, 0);
    cv::Sobel(stereo_depth_mm, gradY, CV_32F, 0, 1);
    cv::Mat gradient;
    cv::magnitude(gradX, gradY, gradient);
    
    // 收集有效点
    std::vector<std::tuple<float, float, float>> validPoints;
    validPoints.reserve(rows * cols / 4);
    
    for (int y = 0; y < rows; ++y) {
        const float* xptr = mono_depth.ptr<float>(y);
        const float* yptr = stereo_depth_mm.ptr<float>(y);
        const float* dptr = disparity.ptr<float>(y);
        const float* gptr = gradient.ptr<float>(y);
        const uchar* mptr = valid_mask.empty() ? nullptr : valid_mask.ptr<uchar>(y);
        
        for (int x = std::max(0, left_bound_x); x < cols; ++x) {
            if (mptr && mptr[x] == 0) continue;
            
            float xv = xptr[x];
            float yv = yptr[x];
            float dv = dptr[x];
            float gv = gptr[x];
            
            if (!std::isfinite(xv) || !std::isfinite(yv) || !std::isfinite(dv)) continue;
            if (xv <= 0.0f || yv <= 0.0f || dv <= 0.0f) continue;
            
            float weight = calculateConfidenceWeight(dv, yv, gv);
            validPoints.emplace_back(xv, yv, weight);
        }
    }
    
    result.total_points = validPoints.size();
    if (validPoints.size() < options_.min_samples) {
        return result;
    }
    
    // RANSAC拟合
    double s_ransac = 1.0, b_ransac = 0.0;
    bool ransacSuccess = ransacLinearFit(validPoints, s_ransac, b_ransac);
    
    if (!ransacSuccess) {
        // 回退到简单线性拟合
        double sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0;
        for (const auto& point : validPoints) {
            float xv = std::get<0>(point);
            float yv = std::get<1>(point);
            sumX += xv;
            sumY += yv;
            sumXX += (double)xv * (double)xv;
            sumXY += (double)xv * (double)yv;
        }
        
        double n = static_cast<double>(validPoints.size());
        double denom = n * sumXX - sumX * sumX;
        if (std::abs(denom) < 1e-8) return result;
        
        double s = (n * sumXY - sumX * sumY) / denom;
        double b = (sumY - s * sumX) / n;
        result.scale_factor = s;
        result.bias = b;
        result.success = std::isfinite(s) && std::isfinite(b);
        return result;
    }
    
    // 使用RANSAC内点进行加权最小二乘
    std::vector<std::tuple<float, float, float>> inliers;
    inliers.reserve(validPoints.size());
    
    for (const auto& point : validPoints) {
        float x = std::get<0>(point);
        float y = std::get<1>(point);
        float w = std::get<2>(point);
        float predicted = s_ransac * x + b_ransac;
        float error = std::abs(y - predicted);
        
        if (error < options_.ransac_threshold) {
            inliers.emplace_back(x, y, w);
        }
    }
    
    result.inlier_points = inliers.size();
    
    if (inliers.size() < options_.min_samples / 2) {
        result.scale_factor = s_ransac;
        result.bias = b_ransac;
        result.success = true;
        return result;
    }
    
    // 加权最小二乘拟合
    double s_weighted = 1.0, b_weighted = 0.0;
    bool weightedSuccess = weightedLinearFit(inliers, s_weighted, b_weighted);
    
    if (weightedSuccess) {
        result.scale_factor = s_weighted;
        result.bias = b_weighted;
    } else {
        result.scale_factor = s_ransac;
        result.bias = b_ransac;
    }
    
    result.success = true;
    
    // 计算RMS误差
    double sumSquaredError = 0.0;
    int validCount = 0;
    for (const auto& point : inliers) {
        float x = std::get<0>(point);
        float y = std::get<1>(point);
        float predicted = result.scale_factor * x + result.bias;
        sumSquaredError += (y - predicted) * (y - predicted);
        validCount++;
    }
    if (validCount > 0) {
        result.rms_error = std::sqrt(sumSquaredError / validCount);
    }
    
    return result;
}

ComprehensiveDepthResult ComprehensiveDepthProcessor::processRectifiedImages(const cv::Mat& left_rectified,
                                                                             const cv::Mat& right_rectified) {
    ComprehensiveDepthResult result;
    
    if (left_rectified.empty() || right_rectified.empty()) {
        return result;
    }
    
    // 确保已初始化
    if (!initialized_) {
        initialize();
    }
    
    // 设置图像尺寸
    if (image_size_.empty()) {
        image_size_ = left_rectified.size();
        
        // 立体校正
        cv::stereoRectify(K0_, D0_, K1_, D1_, image_size_, R_, T_, R1_, R2_, P1_, P2_, Q_,
                         cv::CALIB_ZERO_DISPARITY, -1.0, image_size_, &roi1_, &roi2_);
        
        // 初始化SGBM
        sgbm_ = cv::StereoSGBM::create(options_.min_disparity, options_.num_disparities, options_.block_size);
        sgbm_->setP1(8 * options_.block_size * options_.block_size);
        sgbm_->setP2(32 * options_.block_size * options_.block_size);
        sgbm_->setDisp12MaxDiff(options_.disp12_max_diff);
        sgbm_->setUniquenessRatio(options_.uniqueness_ratio);
        sgbm_->setSpeckleWindowSize(options_.speckle_window);
        sgbm_->setSpeckleRange(options_.speckle_range);
        sgbm_->setPreFilterCap(options_.prefilter_cap);
        sgbm_->setMode(cv::StereoSGBM::MODE_SGBM_3WAY);
    }
    
    // 预处理
    cv::Mat procL, procR;
    preprocessImage(left_rectified, procL);
    preprocessImage(right_rectified, procR);
    
    // 计算视差
    cv::Mat grayL, grayR;
    cv::cvtColor(procL, grayL, cv::COLOR_BGR2GRAY);
    cv::cvtColor(procR, grayR, cv::COLOR_BGR2GRAY);
    
    cv::Mat disp16S;
    sgbm_->compute(grayL, grayR, disp16S);
    cv::Mat disp32F;
    disp16S.convertTo(disp32F, CV_32F, 1.0 / 16.0);
    result.disparity = disp32F.clone();
    
    // 计算双目深度
    cv::Mat points3D;
    cv::reprojectImageTo3D(disp32F, points3D, Q_, true, CV_32F);
    std::vector<cv::Mat> xyz;
    cv::split(points3D, xyz);
    cv::Mat depthZ = xyz[2];
    
    // 移除深度后处理，保持原始深度值
    depthZ.convertTo(result.stereo_depth_mm, CV_32F, 1.0); // 转换为毫米
    
    // 单目深度推理
    if (mono_model_) {
        mono_model_->ComputeDepth(procL, result.mono_depth_raw);
    }

    // 新融合策略：MonoSmoothStereo（以双目为基准、单目补洞+低频）
    if (options_.fusion_mode == FusionMode::MonoSmoothStereo && !result.mono_depth_raw.empty()) {
        // 1) 置信度/掩码（使用简单启发式：有效即 conf=1，否则0；可替换为代价/一致性映射）
        cv::Mat stereo_conf = (result.stereo_depth_mm > 0);
        stereo_conf.convertTo(stereo_conf, CV_32F, 1.0/255.0);

        // 2) 单目低频 Lmono
        cv::Mat Lmono;
        cv::bilateralFilter(result.mono_depth_raw, Lmono,
                            options_.fusion_bilateral_d,
                            options_.fusion_bilateral_sigma_r,
                            options_.fusion_bilateral_sigma_s);

        // 3) 双目低频 Zbase 与高频 Hstereo
        cv::Mat Zbase;
        cv::bilateralFilter(result.stereo_depth_mm, Zbase,
                            options_.fusion_bilateral_d,
                            options_.fusion_bilateral_sigma_r,
                            options_.fusion_bilateral_sigma_s);
        cv::Mat Zbase2;
        cv::bilateralFilter(result.stereo_depth_mm, Zbase2,
                            options_.fusion_bilateral_d,
                            options_.fusion_bilateral_sigma_r,
                            options_.fusion_bilateral_sigma_s);
        cv::Mat Hstereo = result.stereo_depth_mm - Zbase2;

        // 4) 置信度映射 α=conf^gamma
        cv::Mat alpha;
        cv::pow(stereo_conf, options_.fusion_confidence_gamma, alpha);
        // 高频仅在 conf>阈值 时注入
        cv::Mat highMask = stereo_conf > options_.fusion_confidence_thresh;

        // 5) 融合 Z = α·Zbase + (1−α)·Lmono + Hstereo·mask
        cv::Mat oneMinusAlpha = 1.0 - alpha;
        cv::Mat fused = alpha.mul(Zbase) + oneMinusAlpha.mul(Lmono);
        Hstereo.copyTo(Hstereo, highMask);
        fused = fused + Hstereo;

        result.final_fused_depth = fused;
        last_final_fused_depth_ = result.final_fused_depth.clone();
    }
    
    // 简化：置信度图按有效性生成（可后续接入代价/一致性）
    result.confidence_map = (result.stereo_depth_mm > 0);
    result.confidence_map.convertTo(result.confidence_map, CV_32F, 1.0/255.0);
    
    result.success = true;
    return result;
}

ComprehensiveDepthResult ComprehensiveDepthProcessor::processAlreadyRectifiedImages(const cv::Mat& left_rectified,
                                                                                    const cv::Mat& right_rectified,
                                                                                    const cv::Mat& Q_matrix) {
    ComprehensiveDepthResult result;
    
    if (left_rectified.empty() || right_rectified.empty()) {
        return result;
    }
    
    // 确保已初始化
    if (!initialized_) {
        initialize();
    }
    
    // 设置图像尺寸（跳过立体校正）
    if (image_size_.empty()) {
        image_size_ = left_rectified.size();
        
        // 使用提供的Q矩阵或计算默认Q矩阵
        if (!Q_matrix.empty()) {
            Q_ = Q_matrix.clone();
        } else {
            // 计算默认的Q矩阵，基于相机参数
            double baseline = cv::norm(T_);
            double focal_length = (K0_.at<double>(0, 0) + K0_.at<double>(1, 1)) / 2.0;
            double cx = K0_.at<double>(0, 2);
            double cy = K0_.at<double>(1, 2);
            
            Q_ = (cv::Mat_<double>(4, 4) << 
                  1.0, 0.0, 0.0, -cx,
                  0.0, 1.0, 0.0, -cy,
                  0.0, 0.0, 0.0, focal_length,
                  0.0, 0.0, 1.0/baseline, 0.0);
        }
        
        // 设置ROI为整个图像
        roi1_ = cv::Rect(0, 0, image_size_.width, image_size_.height);
        roi2_ = cv::Rect(0, 0, image_size_.width, image_size_.height);
        
        // 初始化SGBM
        sgbm_ = cv::StereoSGBM::create(options_.min_disparity, options_.num_disparities, options_.block_size);
        sgbm_->setP1(8 * options_.block_size * options_.block_size);
        sgbm_->setP2(32 * options_.block_size * options_.block_size);
        sgbm_->setDisp12MaxDiff(options_.disp12_max_diff);
        sgbm_->setUniquenessRatio(options_.uniqueness_ratio);
        sgbm_->setSpeckleWindowSize(options_.speckle_window);
        sgbm_->setSpeckleRange(options_.speckle_range);
        sgbm_->setPreFilterCap(options_.prefilter_cap);
        sgbm_->setMode(cv::StereoSGBM::MODE_SGBM_3WAY);
    }
    
    // 预处理
    cv::Mat procL, procR;
    preprocessImage(left_rectified, procL);
    preprocessImage(right_rectified, procR);
    
    // 计算视差
    cv::Mat grayL, grayR;
    cv::cvtColor(procL, grayL, cv::COLOR_BGR2GRAY);
    cv::cvtColor(procR, grayR, cv::COLOR_BGR2GRAY);
    
    cv::Mat disp16S;
    sgbm_->compute(grayL, grayR, disp16S);
    cv::Mat disp32F;
    disp16S.convertTo(disp32F, CV_32F, 1.0 / 16.0);
    result.disparity = disp32F.clone();
    
    // 计算双目深度
    cv::Mat points3D;
    cv::reprojectImageTo3D(disp32F, points3D, Q_, true, CV_32F);
    std::vector<cv::Mat> xyz;
    cv::split(points3D, xyz);
    cv::Mat depthZ = xyz[2];
    
    // 移除深度后处理，保持原始深度值
    depthZ.convertTo(result.stereo_depth_mm, CV_32F, 1.0); // 转换为毫米
    
    // 单目深度推理
    if (mono_model_) {
        mono_model_->ComputeDepth(procL, result.mono_depth_raw);
    }
    
    // 深度校准
    if (!result.mono_depth_raw.empty()) {
        int leftBoundX = std::max(roi1_.x, roi2_.x);
        result.calibration = calibrateDepth(result.mono_depth_raw, result.stereo_depth_mm, 
                                          disp32F, cv::Mat(), leftBoundX);
        
        // 应用校准
        if (result.calibration.success) {
            result.mono_depth_calibrated_mm = cv::Mat::zeros(result.mono_depth_raw.size(), CV_32F);
            result.mono_depth_raw.convertTo(result.mono_depth_calibrated_mm, CV_32F);
            result.mono_depth_calibrated_mm = result.mono_depth_calibrated_mm * 
                                            (float)result.calibration.scale_factor + 
                                            (float)result.calibration.bias;
        }
    }
    
    // 生成置信度图
    result.confidence_map = cv::Mat::zeros(disp32F.size(), CV_32F);
    for (int y = 0; y < disp32F.rows; ++y) {
        const float* dptr = disp32F.ptr<float>(y);
        const float* depthPtr = result.stereo_depth_mm.ptr<float>(y);
        const float* gradPtr = [&]() -> const float* {
            static cv::Mat gradient;
            if (gradient.empty()) {
                cv::Mat gradX, gradY;
                cv::Sobel(result.stereo_depth_mm, gradX, CV_32F, 1, 0);
                cv::Sobel(result.stereo_depth_mm, gradY, CV_32F, 0, 1);
                cv::magnitude(gradX, gradY, gradient);
            }
            return gradient.ptr<float>(y);
        }();
        float* confPtr = result.confidence_map.ptr<float>(y);
        
        for (int x = 0; x < disp32F.cols; ++x) {
            if (true) { // 移除valid检查
                confPtr[x] = calculateConfidenceWeight(dptr[x], depthPtr[x], gradPtr[x]);
            }
        }
    }
    
    result.success = true;
    return result;
}

// 新增：细粒度处理方法实现

ComprehensiveDepthResult ComprehensiveDepthProcessor::processRectifiedImagesFineGrained(
    const cv::Mat& left_rectified, const cv::Mat& right_rectified, 
    const FineGrainedOptions& fine_options) {
    
    ComprehensiveDepthResult result;
    
    if (!initialized_) {
        image_size_ = left_rectified.size();
        initialize();
    }
    
    // 1. 图像预处理
    if (fine_options.save_preprocessed_images) {
        preprocessImage(left_rectified, result.left_preprocessed);
        preprocessImage(right_rectified, result.right_preprocessed);
        last_left_preprocessed_ = result.left_preprocessed.clone();
        last_right_preprocessed_ = result.right_preprocessed.clone();
    } else {
        preprocessImage(left_rectified, result.left_preprocessed);
        preprocessImage(right_rectified, result.right_preprocessed);
    }
    
    // 2. 转换为灰度图
    if (fine_options.save_gray_images) {
        cv::cvtColor(result.left_preprocessed, result.left_gray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(result.right_preprocessed, result.right_gray, cv::COLOR_BGR2GRAY);
        last_left_gray_ = result.left_gray.clone();
        last_right_gray_ = result.right_gray.clone();
    } else {
        cv::cvtColor(result.left_preprocessed, result.left_gray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(result.right_preprocessed, result.right_gray, cv::COLOR_BGR2GRAY);
    }
    
    // 3. 计算视差
    cv::Mat disp16S;
    sgbm_->compute(result.left_gray, result.right_gray, disp16S);
    
    if (fine_options.save_raw_disparity) {
        result.disparity_raw = disp16S.clone();
        last_disparity_raw_ = result.disparity_raw.clone();
    }
    
    cv::Mat disp32F;
    disp16S.convertTo(disp32F, CV_32F, 1.0 / 16.0);
    result.disparity = disp32F.clone();
    last_disparity_ = result.disparity.clone();
    
    // 4. 计算双目深度
    cv::Mat points3D;
    cv::reprojectImageTo3D(disp32F, points3D, Q_, true, CV_32F);
    
    if (fine_options.save_3d_points) {
        result.points_3d = points3D.clone();
        last_points_3d_ = result.points_3d.clone();
    }
    
    std::vector<cv::Mat> xyz;
    cv::split(points3D, xyz);
    cv::Mat depthZ = xyz[2];
    
    // 移除深度后处理，保持原始深度值
    depthZ.convertTo(result.stereo_depth_mm, CV_32F, 1.0); // 转换为毫米
    
    if (fine_options.save_valid_mask) {
        result.valid_mask = cv::Mat();
        last_valid_mask_ = result.valid_mask.clone();
    }
    
    last_stereo_depth_ = result.stereo_depth_mm.clone();
    
    // 5. 单目深度推理
    if (mono_model_) {
        mono_model_->ComputeDepth(result.left_preprocessed, result.mono_depth_raw);
        last_mono_depth_raw_ = result.mono_depth_raw.clone();
    }
    
    // 6. 深度校准
    if (!result.mono_depth_raw.empty()) {
        int leftBoundX = std::max(roi1_.x, roi2_.x);
        // 选择校准方法：检测是否需要非线性校准
        if (options_.enable_nonlinear_calibration) {
            float curvature = detectPlaneCurvature(result.stereo_depth_mm, cv::Mat());
            if (curvature > options_.curvature_detection_threshold) {
                LOG(INFO) << "检测到平面曲率 " << curvature << "，使用非线性校准";
                result.calibration = calibrateDepthNonlinear(result.mono_depth_raw, result.stereo_depth_mm,
                                                           disp32F, cv::Mat(), leftBoundX, options_.nonlinear_type);
            } else {
                LOG(INFO) << "平面曲率较小，使用线性校准";
                result.calibration = calibrateDepth(result.mono_depth_raw, result.stereo_depth_mm,
                                                  disp32F, cv::Mat(), leftBoundX);
            }
        } else {
            result.calibration = calibrateDepth(result.mono_depth_raw, result.stereo_depth_mm,
                                              disp32F, cv::Mat(), leftBoundX);
        }
        
        // 应用校准 - 支持非线性校准
        if (result.calibration.success) {
            result.mono_depth_calibrated_mm = cv::Mat::zeros(result.mono_depth_raw.size(), CV_32F);
            result.mono_depth_raw.convertTo(result.mono_depth_calibrated_mm, CV_32F);

            // 根据校准类型应用不同的校准方法
            if (result.calibration.calibration_type == NonlinearCalibrationType::LINEAR) {
                // 传统线性校准
                result.mono_depth_calibrated_mm = result.mono_depth_calibrated_mm *
                                                (float)result.calibration.scale_factor +
                                                (float)result.calibration.bias;
            } else {
                // 非线性校准
                result.mono_depth_calibrated_mm = applyNonlinearCalibration(result.mono_depth_calibrated_mm,
                                                                           result.calibration);
            }
            last_mono_depth_calibrated_ = result.mono_depth_calibrated_mm.clone();
        }
    }
    
    // 7. 生成置信度图
    result.confidence_map = cv::Mat::zeros(disp32F.size(), CV_32F);
    cv::Mat gradient;
    if (fine_options.save_gradient) {
        cv::Mat gradX, gradY;
        cv::Sobel(result.stereo_depth_mm, gradX, CV_32F, 1, 0);
        cv::Sobel(result.stereo_depth_mm, gradY, CV_32F, 0, 1);
        cv::magnitude(gradX, gradY, gradient);
        result.gradient_magnitude = gradient.clone();
        last_gradient_magnitude_ = result.gradient_magnitude.clone();
    } else {
        cv::Mat gradX, gradY;
        cv::Sobel(result.stereo_depth_mm, gradX, CV_32F, 1, 0);
        cv::Sobel(result.stereo_depth_mm, gradY, CV_32F, 0, 1);
        cv::magnitude(gradX, gradY, gradient);
    }
    
    for (int y = 0; y < disp32F.rows; ++y) {
        const float* dptr = disp32F.ptr<float>(y);
        const float* depthPtr = result.stereo_depth_mm.ptr<float>(y);
        const float* gradPtr = gradient.ptr<float>(y);
        float* confPtr = result.confidence_map.ptr<float>(y);
        
        for (int x = 0; x < disp32F.cols; ++x) {
            if (true) { // 移除valid检查
                confPtr[x] = calculateConfidenceWeight(dptr[x], depthPtr[x], gradPtr[x]);
            }
        }
    }
    
    // 8. 深度融合（可选）
    if (fine_options.enable_depth_fusion && !result.mono_depth_calibrated_mm.empty()) {
        result.final_fused_depth = fuseDepthMaps(result.stereo_depth_mm, 
                                                result.mono_depth_calibrated_mm, 
                                                result.confidence_map);
        if (fine_options.save_final_fused_depth) {
            last_final_fused_depth_ = result.final_fused_depth.clone();
        }
    }
    
    result.success = true;
    return result;
}

cv::Mat ComprehensiveDepthProcessor::computeDisparityOnly(const cv::Mat& left_rectified,
                                                        const cv::Mat& right_rectified) {
    if (!initialized_) {
        image_size_ = left_rectified.size();
        initialize();
    }
    // 确保SGBM已创建
    if (sgbm_.empty()) {
        sgbm_ = cv::StereoSGBM::create(options_.min_disparity, options_.num_disparities, options_.block_size);
        sgbm_->setP1(8 * options_.block_size * options_.block_size);
        sgbm_->setP2(32 * options_.block_size * options_.block_size);
        sgbm_->setDisp12MaxDiff(options_.disp12_max_diff);
        sgbm_->setUniquenessRatio(options_.uniqueness_ratio);
        sgbm_->setSpeckleWindowSize(options_.speckle_window);
        sgbm_->setSpeckleRange(options_.speckle_range);
        sgbm_->setPreFilterCap(options_.prefilter_cap);
        sgbm_->setMode(cv::StereoSGBM::MODE_SGBM_3WAY);
    }
    
    // 预处理
    cv::Mat procL, procR;
    preprocessImage(left_rectified, procL);
    preprocessImage(right_rectified, procR);
    
    // 转换为灰度
    cv::Mat grayL, grayR;
    cv::cvtColor(procL, grayL, cv::COLOR_BGR2GRAY);
    cv::cvtColor(procR, grayR, cv::COLOR_BGR2GRAY);
    
    // 计算视差
    cv::Mat disp16S;
    sgbm_->compute(grayL, grayR, disp16S);
    cv::Mat disp32F;
    disp16S.convertTo(disp32F, CV_32F, 1.0 / 16.0);
 
    // 保存中间结果
    last_disparity_ = disp32F.clone();
 
    // 移除所有视差后处理，保持原始SGBM输出
    
    return disp32F;
}

cv::Mat ComprehensiveDepthProcessor::computeStereoDepthOnly(const cv::Mat& left_rectified,
                                                          const cv::Mat& right_rectified) {
    cv::Mat disparity = computeDisparityOnly(left_rectified, right_rectified);
    
    // 计算深度
    cv::Mat points3D;
    cv::reprojectImageTo3D(disparity, points3D, Q_, true, CV_32F);
    std::vector<cv::Mat> xyz;
    cv::split(points3D, xyz);
    cv::Mat depthZ = xyz[2];
    
    // 移除深度后处理，保持原始深度值
    depthZ.convertTo(depthZ, CV_32F, 1.0); // 转换为毫米
    
    return depthZ;
}

cv::Mat ComprehensiveDepthProcessor::computeMonoDepthOnly(const cv::Mat& left_rectified) {
    if (left_rectified.empty()) {
        return cv::Mat();
    }
    if (!initialized_) {
        image_size_ = left_rectified.size();
        initialize();
    }
    
    if (!mono_model_) {
        return cv::Mat();
    }
    
    // 预处理
    cv::Mat procL;
    preprocessImage(left_rectified, procL);
    
    // 单目深度推理
    cv::Mat mono_depth;
    mono_model_->ComputeDepth(procL, mono_depth);
    
    return mono_depth;
}

cv::Mat ComprehensiveDepthProcessor::depthFromDisparity(const cv::Mat& disparity32F, const cv::Mat& Q_matrix) {
    if (disparity32F.empty()) return cv::Mat();
    cv::Mat Quse = !Q_matrix.empty() ? Q_matrix : Q_;
    if (Quse.empty()) return cv::Mat();
    cv::Mat points3D;
    cv::reprojectImageTo3D(disparity32F, points3D, Quse, true, CV_32F);
    std::vector<cv::Mat> xyz;
    cv::split(points3D, xyz);
    return xyz[2];
}

cv::Mat ComprehensiveDepthProcessor::filterDepth(const cv::Mat& depth_mm, const cv::Mat& valid_mask) {
    // 移除深度滤波，直接返回原始深度
    if (depth_mm.empty()) return cv::Mat();
    return depth_mm.clone();
}

cv::Mat ComprehensiveDepthProcessor::buildConfidenceMap(const cv::Mat& disparity32F, const cv::Mat& stereo_depth_mm) {
    if (disparity32F.empty() || stereo_depth_mm.empty()) return cv::Mat();
    cv::Mat gradX, gradY, gradient;
    cv::Sobel(stereo_depth_mm, gradX, CV_32F, 1, 0);
    cv::Sobel(stereo_depth_mm, gradY, CV_32F, 0, 1);
    cv::magnitude(gradX, gradY, gradient);
    cv::Mat conf = cv::Mat::zeros(disparity32F.size(), CV_32F);
    for (int y = 0; y < disparity32F.rows; ++y) {
        const float* dptr = disparity32F.ptr<float>(y);
        const float* depthPtr = stereo_depth_mm.ptr<float>(y);
        const float* gradPtr = gradient.ptr<float>(y);
        float* cptr = conf.ptr<float>(y);
        for (int x = 0; x < disparity32F.cols; ++x) {
            float w = calculateConfidenceWeight(dptr[x], depthPtr[x], gradPtr[x]);
            cptr[x] = w;
        }
    }
    return conf;
}

DepthCalibrationResult ComprehensiveDepthProcessor::calibrateMonoToStereo(const cv::Mat& mono_depth,
                                                                         const cv::Mat& stereo_depth_mm,
                                                                         const cv::Mat& disparity,
                                                                         const cv::Mat& valid_mask,
                                                                         int left_bound_x,
                                                                         cv::Mat& mono_calibrated_out) {
    DepthCalibrationResult r = calibrateDepth(mono_depth, stereo_depth_mm, disparity, valid_mask, left_bound_x);
    if (!r.success) {
        mono_calibrated_out.release();
        return r;
    }

    // 先应用线性标定作为基线
    cv::Mat linCalibrated;
    mono_depth.convertTo(linCalibrated, CV_32F);
    linCalibrated = linCalibrated * (float)r.scale_factor + (float)r.bias;

    // 基于分位数锚点的全局单调PWL映射，增强动态范围与局部对比
    // 收集有效配对样本 (mono_raw, stereo_mm)
    std::vector<float> monoSamples;
    std::vector<float> stereoSamples;
    monoSamples.reserve((size_t)mono_depth.total() / 4);
    stereoSamples.reserve((size_t)stereo_depth_mm.total() / 4);

    const int rows = mono_depth.rows;
    const int cols = mono_depth.cols;
    for (int y = 0; y < rows; ++y) {
        const float* mptr = mono_depth.ptr<float>(y);
        const float* sptr = stereo_depth_mm.ptr<float>(y);
        const float* dptr = disparity.ptr<float>(y);
        const uchar* vptr = valid_mask.empty() ? nullptr : valid_mask.ptr<uchar>(y);
        for (int x = std::max(0, left_bound_x); x < cols; ++x) {
            if (vptr && vptr[x] == 0) continue;
            float mv = mptr[x];
            float sv = sptr[x];
            float dv = dptr[x];
            if (!std::isfinite(mv) || !std::isfinite(sv) || !std::isfinite(dv)) continue;
            if (mv <= 0.0f || sv <= 0.0f || dv <= 0.0f) continue;
            monoSamples.push_back(mv);
            stereoSamples.push_back(sv);
        }
    }

    // 当有效样本足够时，构建分位数锚点映射
    const int minSamplesForPWL = std::max(1000, options_.min_samples);
    const int numAnchors = 8; // 锚点数（包含两端共9个分位）
    if ((int)monoSamples.size() >= minSamplesForPWL) {
        auto computeQuantiles = [](std::vector<float>& data, int anchors) {
            std::vector<float> qv;
            qv.reserve(anchors + 1);
            std::sort(data.begin(), data.end());
            for (int k = 0; k <= anchors; ++k) {
                double q = (double)k / (double)anchors;
                size_t idx = (size_t)std::min(std::max(0.0, q * (data.size() - 1)), (double)data.size() - 1);
                qv.push_back(data[idx]);
            }
            return qv;
        };

        // 复制样本用于排序
        std::vector<float> monoSorted = monoSamples;
        std::vector<float> stereoSorted = stereoSamples;
        std::vector<float> qm = computeQuantiles(monoSorted, numAnchors);
        std::vector<float> qs = computeQuantiles(stereoSorted, numAnchors);

        // 确保单调性（防止极端值导致锚点非增）
        for (int i = 1; i < (int)qm.size(); ++i) {
            if (qm[i] < qm[i - 1]) qm[i] = qm[i - 1];
            if (qs[i] < qs[i - 1]) qs[i] = qs[i - 1];
        }

        // 构建映射函数：对任意v，在线性插值 qm->qs
        auto mapPWL = [&](float v) -> float {
            if (v <= qm.front()) {
                // 端点外延
                float m0 = qm.front();
                float s0 = qs.front();
                if (m0 > 1e-6f) return s0 * (v / m0);
                return s0;
            }
            if (v >= qm.back()) {
                float m1 = qm[qm.size() - 2];
                float m2 = qm.back();
                float s1 = qs[qs.size() - 2];
                float s2 = qs.back();
                float denom = std::max(1e-6f, m2 - m1);
                float t = (v - m1) / denom;
                return s1 + t * (s2 - s1);
            }
            // 区间查找
            int lo = 0, hi = (int)qm.size() - 1;
            while (hi - lo > 1) {
                int mid = (lo + hi) >> 1;
                if (qm[mid] <= v) lo = mid; else hi = mid;
            }
            float m1 = qm[lo], m2 = qm[lo + 1];
            float s1 = qs[lo], s2 = qs[lo + 1];
            float denom = std::max(1e-6f, m2 - m1);
            float t = (v - m1) / denom;
            return s1 + t * (s2 - s1);
        };

        // 应用PWL映射于单目原始值（不叠加线性s,b，避免双重缩放），获得非线性校准结果
        mono_calibrated_out = cv::Mat::zeros(mono_depth.size(), CV_32F);
        for (int y = 0; y < rows; ++y) {
            const float* mptr = mono_depth.ptr<float>(y);
            float* outp = mono_calibrated_out.ptr<float>(y);
            for (int x = 0; x < cols; ++x) {
                float mv = mptr[x];
                if (mv > 0.0f && std::isfinite(mv)) outp[x] = mapPWL(mv);
                else outp[x] = 0.0f;
            }
        }

        // 可选：若PWL效果不佳（方差缩小过多），退回线性
        cv::Scalar meanS, stdS, meanP, stdP;
        cv::meanStdDev(stereo_depth_mm, meanS, stdS, stereo_depth_mm > 0);
        cv::meanStdDev(mono_calibrated_out, meanP, stdP, mono_calibrated_out > 0);
        if (stdP[0] < 0.5 * stdS[0]) {
            mono_calibrated_out = linCalibrated; // 退回线性结果以避免过度压缩
        }
        r.success = true; // 标记成功
        return r;
    }

    // 样本不足：使用线性结果
    mono_calibrated_out = linCalibrated;
    r.success = true;
    return r;
}

cv::Mat ComprehensiveDepthProcessor::fuseDepthMaps(const cv::Mat& stereo_depth,
                                                 const cv::Mat& mono_depth,
                                                 const cv::Mat& confidence_map) {
    if (stereo_depth.empty() || mono_depth.empty()) {
        return stereo_depth;
    }
    CV_Assert(stereo_depth.type() == CV_32F);
    CV_Assert(mono_depth.type() == CV_32F);
    if (!confidence_map.empty()) {
        CV_Assert(confidence_map.type() == CV_32F);
        CV_Assert(confidence_map.size() == stereo_depth.size());
    }
    
    cv::Mat fused_depth = cv::Mat::zeros(stereo_depth.size(), CV_32F);
    
    const bool use_conf = !confidence_map.empty();
    for (int y = 0; y < stereo_depth.rows; ++y) {
        const float* stereoPtr = stereo_depth.ptr<float>(y);
        const float* monoPtr = mono_depth.ptr<float>(y);
        const float* confPtr = use_conf ? confidence_map.ptr<float>(y) : nullptr;
        float* fusedPtr = fused_depth.ptr<float>(y);
        
        for (int x = 0; x < stereo_depth.cols; ++x) {
            float stereo_val = stereoPtr[x];
            float mono_val = monoPtr[x];
            float conf_val = use_conf ? confPtr[x] : 100.0f; // 无置信度时默认100
            
            if (stereo_val > 0 && mono_val > 0) {
                float weight = std::min(conf_val / 100.0f, 1.0f);
                fusedPtr[x] = weight * stereo_val + (1.0f - weight) * mono_val;
            } else if (stereo_val > 0) {
                fusedPtr[x] = stereo_val;
            } else if (mono_val > 0) {
                fusedPtr[x] = mono_val;
            }
        }
    }
    
    return fused_depth;
}

cv::Mat ComprehensiveDepthProcessor::getIntermediateResult(const std::string& step_name) const {
    if (step_name == "preprocessed") {
        return last_left_preprocessed_.clone();
    } else if (step_name == "disparity") {
        return last_disparity_.clone();
    } else if (step_name == "stereo_depth") {
        return last_stereo_depth_.clone();
    } else if (step_name == "mono_depth") {
        return last_mono_depth_raw_.clone();
    } else if (step_name == "calibrated") {
        return last_mono_depth_calibrated_.clone();
    } else if (step_name == "fused") {
        return last_final_fused_depth_.clone();
    } else if (step_name == "calibration_mask") {
        return last_calibration_mask_.clone();
    } else if (step_name == "confidence") {
        // 需要重新计算置信度图
        if (!last_disparity_.empty() && !last_stereo_depth_.empty()) {
            cv::Mat confidence_map = cv::Mat::zeros(last_disparity_.size(), CV_32F);
            cv::Mat gradient;
            cv::Mat gradX, gradY;
            cv::Sobel(last_stereo_depth_, gradX, CV_32F, 1, 0);
            cv::Sobel(last_stereo_depth_, gradY, CV_32F, 0, 1);
            cv::magnitude(gradX, gradY, gradient);
            
            cv::Mat valid = (last_disparity_ > options_.min_disparity);
            for (int y = 0; y < last_disparity_.rows; ++y) {
                const float* dptr = last_disparity_.ptr<float>(y);
                const float* depthPtr = last_stereo_depth_.ptr<float>(y);
                const float* gradPtr = gradient.ptr<float>(y);
                float* confPtr = confidence_map.ptr<float>(y);
                
                for (int x = 0; x < last_disparity_.cols; ++x) {
                    if (true) { // 移除valid检查
                        confPtr[x] = calculateConfidenceWeight(dptr[x], depthPtr[x], gradPtr[x]);
                    }
                }
            }
            return confidence_map;
        }
    }
    
    return cv::Mat();
}

bool ComprehensiveDepthProcessor::saveRGBPointCloud(const cv::Mat& color_image,
                                                   const cv::Mat& depth_image,
                                                   const std::string& filename,
                                                   const std::string& comment) const {
    if (color_image.empty() || depth_image.empty()) {
        std::cerr << "保存点云失败：输入图像为空" << std::endl;
        return false;
    }
    
    if (color_image.size() != depth_image.size()) {
        std::cerr << "保存点云失败：彩色图像和深度图像尺寸不匹配" << std::endl;
        return false;
    }
    
    // 确保Q矩阵是3x4的投影矩阵；允许Q为空时退化为简单相机模型
    cv::Mat Q_3x4;
    if (!Q_.empty()) {
    if (Q_.rows == 4 && Q_.cols == 4) {
        Q_3x4 = Q_.rowRange(0, 3);
    } else if (Q_.rows == 3 && Q_.cols == 4) {
        Q_3x4 = Q_.clone();
        }
    }
    bool useQ = !Q_3x4.empty();
    if (useQ && Q_3x4.type() != CV_32F) {
        cv::Mat Q32F;
        Q_3x4.convertTo(Q32F, CV_32F);
        Q_3x4 = Q32F;
    }
    
    std::ofstream plyFile(filename);
    if (!plyFile.is_open()) {
        std::cerr << "无法创建PLY文件: " << filename << std::endl;
        return false;
    }
    
    // 统计有效点数量
    int validPoints = 0;
    const int rows = depth_image.rows;
    const int cols = depth_image.cols;
    
    for (int y = 0; y < rows; ++y) {
        const float* depthPtr = depth_image.ptr<float>(y);
        for (int x = 0; x < cols; ++x) {
            float depth = depthPtr[x];
            if (depth > options_.min_depth_mm && depth < options_.max_depth_mm && std::isfinite(depth)) {
                validPoints++;
            }
        }
    }
    
    if (validPoints == 0) {
        std::cerr << "没有有效的深度点" << std::endl;
        return false;
    }
    
    // 写入PLY文件头
    plyFile << "ply" << std::endl;
    plyFile << "format ascii 1.0" << std::endl;
    if (!comment.empty()) {
        plyFile << "comment " << comment << std::endl;
    }
    plyFile << "element vertex " << validPoints << std::endl;
    plyFile << "property float x" << std::endl;
    plyFile << "property float y" << std::endl;
    plyFile << "property float z" << std::endl;
    plyFile << "property uchar red" << std::endl;
    plyFile << "property uchar green" << std::endl;
    plyFile << "property uchar blue" << std::endl;
    plyFile << "end_header" << std::endl;
    
    // 优先使用投影矩阵P1_（来自立体校正），用深度(mm)与内参直接还原3D坐标
    bool haveP = !P1_.empty();
    double fx = 0, fy = 0, cx = 0, cy = 0;
    if (haveP) {
        fx = P1_.at<double>(0, 0);
        fy = P1_.at<double>(1, 1);
        cx = P1_.at<double>(0, 2);
        cy = P1_.at<double>(1, 2);
        if (fx <= 0 || fy <= 0) haveP = false;
    }
    
    // 写入点云数据
    for (int y = 0; y < rows; ++y) {
        const float* depthPtr = depth_image.ptr<float>(y);
        const cv::Vec3b* colorPtr = color_image.ptr<cv::Vec3b>(y);
        
        for (int x = 0; x < cols; ++x) {
            float Z = depthPtr[x]; // 毫米
            
            if (Z > options_.min_depth_mm && Z < options_.max_depth_mm && std::isfinite(Z)) {
                float x3d, y3d, z3d;
                if (haveP) {
                    // 以P1_为准：X = (x-cx)*Z/fx, Y = (y-cy)*Z/fy, Z = Z（单位：mm）
                    x3d = static_cast<float>((static_cast<double>(x) - cx) * (double)Z / fx);
                    y3d = static_cast<float>((static_cast<double>(y) - cy) * (double)Z / fy);
                    z3d = Z;
                } else if (useQ) {
                    // 退化：若无P1_但有Q，仍可使用Q + 视差方式，但此处只有深度，无法可靠反推视差
                    // 因兼容性，仅将Z作为齐次w的倒数替代会产生尺度错误，故回退为近似：直接以像素为XY
                    x3d = static_cast<float>(x);
                    y3d = static_cast<float>(y);
                    z3d = Z;
                } else {
                    // 最差兜底：像素坐标+深度
                    x3d = static_cast<float>(x);
                    y3d = static_cast<float>(y);
                    z3d = Z;
                }
                
                // 获取RGB颜色
                cv::Vec3b color = colorPtr[x];
                int r = color[2]; // OpenCV使用BGR顺序
                int g = color[1];
                int b = color[0];
                
                // 写入点云数据
                plyFile << x3d << " " << y3d << " " << z3d << " " 
                       << r << " " << g << " " << b << std::endl;
            }
        }
    }
    
    plyFile.close();
    std::cout << "点云已保存到: " << filename << " (共 " << validPoints << " 个点)" << std::endl;
    return true;
}

cv::Mat ComprehensiveDepthProcessor::getQMatrix() const {
    return Q_.clone();
}

std::pair<cv::Rect, cv::Rect> ComprehensiveDepthProcessor::getROI() const {
    return {roi1_, roi2_};
}

void ComprehensiveDepthProcessor::updateOptions(const ComprehensiveDepthOptions& options) {
    options_ = options;
    
    // 重新初始化SGBM
    if (initialized_) {
        sgbm_ = cv::StereoSGBM::create(options_.min_disparity, options_.num_disparities, options_.block_size);
        sgbm_->setP1(8 * options_.block_size * options_.block_size);
        sgbm_->setP2(32 * options_.block_size * options_.block_size);
        sgbm_->setDisp12MaxDiff(options_.disp12_max_diff);
        sgbm_->setUniquenessRatio(options_.uniqueness_ratio);
        sgbm_->setSpeckleWindowSize(options_.speckle_window);
        sgbm_->setSpeckleRange(options_.speckle_range);
        sgbm_->setPreFilterCap(options_.prefilter_cap);
        sgbm_->setMode(cv::StereoSGBM::MODE_SGBM_3WAY);
    }
}

cv::Mat ComprehensiveDepthProcessor::refineStereoWithMonoLocalFit(const cv::Mat& stereo_depth_mm,
                                                                 const cv::Mat& mono_depth_ref,
                                                                 int block_size,
                                                                 int overlap,
                                                                 float residual_thresh,
                                                                 bool replace_outliers) {
    if (stereo_depth_mm.empty() || mono_depth_ref.empty()) return stereo_depth_mm;
    CV_Assert(stereo_depth_mm.type() == CV_32F);
    CV_Assert(mono_depth_ref.type() == CV_32F);
    CV_Assert(stereo_depth_mm.size() == mono_depth_ref.size());

    const int rows = stereo_depth_mm.rows;
    const int cols = stereo_depth_mm.cols;

    // 有效掩码（仅使用正且合理范围内的像素）
    cv::Mat validStereo = (stereo_depth_mm > 0) & (stereo_depth_mm < 1e7f);
    cv::Mat validMono = (mono_depth_ref > 0) & (mono_depth_ref < 1e7f);
    cv::Mat valid = validStereo & validMono;

    // 输出初始化为原始双目深度
    cv::Mat refined = stereo_depth_mm.clone();

    // 滑动窗口分块（带重叠）
    const int step = std::max(1, block_size - overlap);
    for (int by = 0; by < rows; by += step) {
        for (int bx = 0; bx < cols; bx += step) {
            int y0 = by;
            int x0 = bx;
            int y1 = std::min(rows, by + block_size);
            int x1 = std::min(cols, bx + block_size);
            cv::Rect roi(x0, y0, x1 - x0, y1 - y0);
            if (roi.width <= 2 || roi.height <= 2) continue;

            cv::Mat sBlk = stereo_depth_mm(roi);
            cv::Mat mBlk = mono_depth_ref(roi);
            cv::Mat vBlk = valid(roi);

            // 收集块内有效点用于拟合（x=mono, y=stereo）
            std::vector<std::tuple<float,float,float>> points;
            points.reserve(roi.area() / 2);

            // 使用梯度作为权重（单目平滑，双目细节多；此处用双目梯度抑制边缘影响）
            cv::Mat gradX, gradY, grad;
            cv::Sobel(sBlk, gradX, CV_32F, 1, 0);
            cv::Sobel(sBlk, gradY, CV_32F, 0, 1);
            cv::magnitude(gradX, gradY, grad);

            for (int y = 0; y < sBlk.rows; ++y) {
                const float* sp = sBlk.ptr<float>(y);
                const float* mp = mBlk.ptr<float>(y);
                const uchar* vp = vBlk.ptr<uchar>(y);
                const float* gp = grad.ptr<float>(y);
                for (int x = 0; x < sBlk.cols; ++x) {
                    if (vp[x]) {
                        float monoVal = mp[x];
                        float stereoVal = sp[x];
                        float g = gp[x];
                        // 权重：梯度越大权重略低，避免边缘误差影响
                        float w = std::exp(-g / std::max(1.0f, options_.gradient_weight_scale));
                        w = std::max(0.05f, std::min(w, 1.0f));
                        points.emplace_back(monoVal, stereoVal, w);
                    }
                }
            }

            if (points.size() < (size_t)options_.min_samples) {
                continue; // 样本太少，跳过此块
            }

            // 先RANSAC找大致比例+偏置，再加权最小二乘精修
            double s_ransac = 1.0, b_ransac = 0.0;
            bool rOk = ransacLinearFit(points, s_ransac, b_ransac);
            if (!rOk) continue;

            // 基于RANSAC结果筛选内点
            std::vector<std::tuple<float,float,float>> inliers;
            inliers.reserve(points.size());
            for (const auto& t : points) {
                float x = std::get<0>(t);
                float y = std::get<1>(t);
                float w = std::get<2>(t);
                float pred = (float)(s_ransac * x + b_ransac);
                if (std::abs(y - pred) < options_.ransac_threshold) {
                    inliers.emplace_back(x, y, w);
                }
            }
            if (inliers.size() < (size_t)options_.min_samples) {
                continue;
            }
            double s_w = s_ransac, b_w = b_ransac;
            (void)weightedLinearFit(inliers, s_w, b_w);

            // 用拟合关系计算预测值，并按照残差阈值筛出异常；异常点替换或置零
            for (int y = 0; y < sBlk.rows; ++y) {
                float* rp = refined(roi).ptr<float>(y);
                const float* sp = sBlk.ptr<float>(y);
                const float* mp = mBlk.ptr<float>(y);
                const uchar* vp = vBlk.ptr<uchar>(y);
                for (int x = 0; x < sBlk.cols; ++x) {
                    if (!vp[x]) continue; // 无效点不处理
                    float pred = (float)(s_w * mp[x] + b_w);
                    float resid = std::abs(sp[x] - pred);
                    if (resid > residual_thresh) {
                        if (replace_outliers) {
                            rp[x] = pred; // 用拟合值替换异常的双目深度
                        } else {
                            rp[x] = 0.0f; // 置零
                        }
                    }
                }
            }
        }
    }

    return refined;
}

// 新增：基于平面结构的分层校准（改进版）
DepthCalibrationResult ComprehensiveDepthProcessor::calibrateDepthPlanarLayered(const cv::Mat& mono_depth,
                                                                                const cv::Mat& stereo_depth_mm,
                                                                                const cv::Mat& disparity,
                                                                                const cv::Mat& valid_mask,
                                                                                int left_bound_x) {
    DepthCalibrationResult result;
    
    if (mono_depth.empty() || stereo_depth_mm.empty() || disparity.empty()) {
        return result;
    }
    
    // 检测异常值
    cv::Mat anomalies = detectAnomalies(stereo_depth_mm, disparity, 2.0f, 5);
    
    // 检测孔洞区域
    cv::Mat hole_mask = detectHoleRegions(stereo_depth_mm, disparity, 500.0f, 50);
    
    // 计算自适应权重
    cv::Mat adaptive_weights = calculateAdaptiveWeights(mono_depth, stereo_depth_mm, disparity, anomalies);
    
    // 仅使用强连通区域且深度不超过120mm的点进行校准
    // 1) 深度阈值与基础有效性
    cv::Mat base_valid = (stereo_depth_mm > 0) & (stereo_depth_mm <= 120.0f) & (anomalies == 0);
    if (!valid_mask.empty()) {
        base_valid = base_valid & valid_mask;
    }
    base_valid.convertTo(base_valid, CV_8U, 255.0); // 转为0/255掩码

    // 2) 连通组件分析，保留面积足够大的强连通区域
    cv::Mat labels, stats, centroids;
    int numLabels = 0;
    if (cv::countNonZero(base_valid) > 0) {
        numLabels = cv::connectedComponentsWithStats(base_valid, labels, stats, centroids, 8, CV_32S);
    }
    const int MIN_CC_AREA = 200; // 强连通区域的最小像素数，可按分辨率调整
    cv::Mat strong_conn = cv::Mat::zeros(base_valid.size(), CV_8U);
    for (int i = 1; i < numLabels; ++i) { // 0是背景
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area >= MIN_CC_AREA) {
            strong_conn |= (labels == i);
        }
    }
    // 若没有任何强连通区域，则使用基础有效掩码兜底
    if (cv::countNonZero(strong_conn) == 0) {
        strong_conn = base_valid;
    }

    // 保存掩码供调试展示
    last_calibration_mask_ = strong_conn.clone();

    // 检测平面结构
    std::vector<cv::Vec4f> planes = detectPlanes(stereo_depth_mm, valid_mask, 
                                                options_.plane_detection_threshold, 
                                                options_.plane_min_points);
    
    // 估计相机倾斜角度
    float camera_tilt = estimateCameraTilt(stereo_depth_mm, valid_mask);
    
    std::vector<DepthCalibrationResult> layer_results;
    
    if (options_.enable_plane_based_layering && !planes.empty()) {
        // 基于平面结构分层
        std::vector<cv::Mat> planar_layers = createPlanarLayers(stereo_depth_mm, valid_mask, planes);
        
        for (size_t i = 0; i < planar_layers.size(); ++i) {
            cv::Mat layer_mask = planar_layers[i] & strong_conn;
            
            int sample_count = cv::countNonZero(layer_mask);
            if (sample_count > 50) {
                auto layer_result = calibrateDepthLayer(mono_depth, stereo_depth_mm, 
                                                      disparity, layer_mask, adaptive_weights);
                
                // 计算平面信息
                cv::Rect roi = cv::boundingRect(layer_mask);
                cv::Vec3f plane_normal = calculatePlaneNormal(stereo_depth_mm, layer_mask, roi);
                
                layer_result.layer_index = i;
                layer_result.is_planar_region = true;
                layer_result.plane_normal = plane_normal;
                layer_result.camera_tilt_angle = camera_tilt;
                layer_result.sample_count = sample_count;
                
                // 计算平面角度（相对于相机光轴）
                float plane_angle = std::acos(std::abs(plane_normal[2])) * 180.0f / CV_PI;
                layer_result.plane_angle = plane_angle;
                
                // 计算平面中心
                cv::Moments moments = cv::moments(layer_mask);
                if (moments.m00 > 0) {
                    float center_x = moments.m10 / moments.m00;
                    float center_y = moments.m01 / moments.m00;
                    float center_z = stereo_depth_mm.at<float>(center_y, center_x);
                    layer_result.plane_center = cv::Point3f(center_x, center_y, center_z);
                }
                
                layer_results.push_back(layer_result);
            }
        }
    } else {
        // 回退到深度范围分层（注意：strong_conn 已经限制 ≤120mm 且强连通）
        std::vector<float> depth_ranges = {0, 100, 300, 800, 2000, 10000};
        
        for (size_t i = 0; i < depth_ranges.size() - 1; ++i) {
            cv::Mat range_mask = (stereo_depth_mm >= depth_ranges[i]) & (stereo_depth_mm < depth_ranges[i + 1]);
            range_mask.convertTo(range_mask, CV_8U, 255.0);
            cv::Mat layer_mask = range_mask & strong_conn;
            
            int sample_count = cv::countNonZero(layer_mask);
            if (sample_count > 50) {
                auto layer_result = calibrateDepthLayer(mono_depth, stereo_depth_mm, 
                                                      disparity, layer_mask, adaptive_weights);
                layer_result.layer_index = i;
                layer_result.depth_range_min = depth_ranges[i];
                layer_result.depth_range_max = depth_ranges[i + 1];
                layer_result.sample_count = sample_count;
                layer_result.is_planar_region = false;
                layer_results.push_back(layer_result);
            }
        }
    }
    
    // 孔洞区域特殊校准
    if (cv::countNonZero(hole_mask) > 20) {
        auto hole_result = calibrateHoleRegions(mono_depth, stereo_depth_mm, disparity, hole_mask);
        hole_result.layer_index = -1;
        hole_result.is_planar_region = false;
        layer_results.push_back(hole_result);
    }
    
    // 融合各层结果
    if (layer_results.empty()) {
        return calibrateDepth(mono_depth, stereo_depth_mm, disparity, valid_mask, left_bound_x);
    }
    
    return fuseLayerResults(layer_results, stereo_depth_mm);
}

// 平面检测算法
std::vector<cv::Vec4f> ComprehensiveDepthProcessor::detectPlanes(const cv::Mat& depth_mm,
                                                                 const cv::Mat& valid_mask,
                                                                 float threshold,
                                                                 int min_points) {
    std::vector<cv::Vec4f> planes;
    
    if (depth_mm.empty()) return planes;
    
    const int rows = depth_mm.rows;
    const int cols = depth_mm.cols;
    
    // 创建有效点云
    std::vector<cv::Point3f> points;
    points.reserve(rows * cols / 4);
    
    for (int y = 0; y < rows; y += 2) { // 采样以减少计算量
        for (int x = 0; x < cols; x += 2) {
            if (!valid_mask.empty() && valid_mask.at<uchar>(y, x) == 0) continue;
            
            float depth = depth_mm.at<float>(y, x);
            if (!std::isfinite(depth) || depth <= 0) continue;
            
            // 将像素坐标转换为3D坐标（简化版本）
            float z = depth;
            float x_3d = (x - cols/2.0f) * z / 1000.0f; // 假设焦距为1000
            float y_3d = (y - rows/2.0f) * z / 1000.0f;
            
            points.emplace_back(x_3d, y_3d, z);
        }
    }
    
    if (points.size() < min_points) return planes;
    
    // 使用RANSAC进行平面检测
    cv::Vec4f best_plane;
    int max_inliers = 0;
    
    for (int iter = 0; iter < 100; ++iter) {
        // 随机选择3个点
        std::vector<int> indices;
        for (int i = 0; i < 3; ++i) {
            indices.push_back(rand() % points.size());
        }
        
        // 计算平面方程 ax + by + cz + d = 0
        cv::Point3f p1 = points[indices[0]];
        cv::Point3f p2 = points[indices[1]];
        cv::Point3f p3 = points[indices[2]];
        
        cv::Vec3f v1 = p2 - p1;
        cv::Vec3f v2 = p3 - p1;
        cv::Vec3f normal = v1.cross(v2);
        
        float norm = cv::norm(normal);
        if (norm < 1e-6) continue;
        
        normal = normal / norm;
        float d = -normal.dot(cv::Vec3f(p1.x, p1.y, p1.z));
        
        cv::Vec4f plane(normal[0], normal[1], normal[2], d);
        
        // 计算内点数量
        int inliers = 0;
        for (const auto& point : points) {
            float distance = std::abs(plane[0] * point.x + plane[1] * point.y + 
                                    plane[2] * point.z + plane[3]);
            if (distance < threshold) {
                inliers++;
            }
        }
        
        if (inliers > max_inliers && inliers >= min_points) {
            max_inliers = inliers;
            best_plane = plane;
        }
    }
    
    if (max_inliers >= min_points) {
        planes.push_back(best_plane);
    }
    
    return planes;
}

// 计算平面法向量
cv::Vec3f ComprehensiveDepthProcessor::calculatePlaneNormal(const cv::Mat& depth_mm,
                                                           const cv::Mat& valid_mask,
                                                           const cv::Rect& roi) {
    cv::Vec3f normal(0, 0, 1); // 默认垂直于相机光轴
    
    if (depth_mm.empty() || roi.empty()) return normal;
    
    // 在ROI区域内采样点
    std::vector<cv::Point3f> points;
    for (int y = roi.y; y < roi.y + roi.height; y += 4) {
        for (int x = roi.x; x < roi.x + roi.width; x += 4) {
            if (x >= depth_mm.cols || y >= depth_mm.rows) continue;
            if (!valid_mask.empty() && valid_mask.at<uchar>(y, x) == 0) continue;
            
            float depth = depth_mm.at<float>(y, x);
            if (!std::isfinite(depth) || depth <= 0) continue;
            
            float z = depth;
            float x_3d = (x - depth_mm.cols/2.0f) * z / 1000.0f;
            float y_3d = (y - depth_mm.rows/2.0f) * z / 1000.0f;
            
            points.emplace_back(x_3d, y_3d, z);
        }
    }
    
    if (points.size() < 3) return normal;
    
    // 使用PCA计算主方向
    cv::Mat points_mat(points.size(), 3, CV_32F);
    for (size_t i = 0; i < points.size(); ++i) {
        points_mat.at<float>(i, 0) = points[i].x;
        points_mat.at<float>(i, 1) = points[i].y;
        points_mat.at<float>(i, 2) = points[i].z;
    }
    
    cv::PCA pca(points_mat, cv::Mat(), cv::PCA::DATA_AS_ROW);
    
    // 最小特征值对应的特征向量就是法向量
    normal = cv::Vec3f(pca.eigenvectors.at<float>(2, 0),
                      pca.eigenvectors.at<float>(2, 1),
                      pca.eigenvectors.at<float>(2, 2));
    
    // 确保法向量指向相机
    if (normal[2] < 0) {
        normal = -normal;
    }
    
    return normal;
}

// 估计相机倾斜角度
float ComprehensiveDepthProcessor::estimateCameraTilt(const cv::Mat& depth_mm,
                                                     const cv::Mat& valid_mask) {
    if (depth_mm.empty()) return 0.0f;
    
    // 计算深度梯度
    cv::Mat grad_x, grad_y;
    cv::Sobel(depth_mm, grad_x, CV_32F, 1, 0);
    cv::Sobel(depth_mm, grad_y, CV_32F, 0, 1);
    
    // 计算平均梯度方向
    float sum_grad_x = 0.0f, sum_grad_y = 0.0f;
    int count = 0;
    
    for (int y = 0; y < depth_mm.rows; y += 4) {
        for (int x = 0; x < depth_mm.cols; x += 4) {
            if (!valid_mask.empty() && valid_mask.at<uchar>(y, x) == 0) continue;
            
            float depth = depth_mm.at<float>(y, x);
            if (!std::isfinite(depth) || depth <= 0) continue;
            
            sum_grad_x += grad_x.at<float>(y, x);
            sum_grad_y += grad_y.at<float>(y, x);
            count++;
        }
    }
    
    if (count == 0) return 0.0f;
    
    float avg_grad_x = sum_grad_x / count;
    float avg_grad_y = sum_grad_y / count;
    
    // 计算倾斜角度
    float tilt_angle = std::atan2(std::sqrt(avg_grad_x * avg_grad_x + avg_grad_y * avg_grad_y), 1.0f);
    return tilt_angle * 180.0f / CV_PI;
}

// 基于平面结构的分层
std::vector<cv::Mat> ComprehensiveDepthProcessor::createPlanarLayers(const cv::Mat& depth_mm,
                                                                     const cv::Mat& valid_mask,
                                                                     const std::vector<cv::Vec4f>& planes) {
    std::vector<cv::Mat> layers;
    
    if (depth_mm.empty() || planes.empty()) return layers;
    
    const int rows = depth_mm.rows;
    const int cols = depth_mm.cols;
    
    for (const auto& plane : planes) {
        cv::Mat layer_mask = cv::Mat::zeros(rows, cols, CV_8U);
        
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (!valid_mask.empty() && valid_mask.at<uchar>(y, x) == 0) continue;
                
                float depth = depth_mm.at<float>(y, x);
                if (!std::isfinite(depth) || depth <= 0) continue;
                
                // 将像素坐标转换为3D坐标
                float z = depth;
                float x_3d = (x - cols/2.0f) * z / 1000.0f;
                float y_3d = (y - rows/2.0f) * z / 1000.0f;
                
                // 计算点到平面的距离
                float distance = std::abs(plane[0] * x_3d + plane[1] * y_3d + 
                                        plane[2] * z + plane[3]);
                
                // 如果距离小于阈值，认为属于这个平面
                if (distance < options_.plane_detection_threshold) {
                    layer_mask.at<uchar>(y, x) = 255;
                }
            }
        }
        
        if (cv::countNonZero(layer_mask) > options_.plane_min_points) {
            layers.push_back(layer_mask);
        }
    }
    
    return layers;
}

// ==================== 非线性深度校准实现 ====================

DepthCalibrationResult ComprehensiveDepthProcessor::calibrateDepthNonlinear(const cv::Mat& mono_depth,
                                                                           const cv::Mat& stereo_depth_mm,
                                                                           const cv::Mat& disparity,
                                                                           const cv::Mat& valid_mask,
                                                                           int left_bound_x,
                                                                           NonlinearCalibrationType type) {
    DepthCalibrationResult result;

    if (mono_depth.empty() || stereo_depth_mm.empty()) {
        return result;
    }

    // 首先尝试线性校准作为基准
    DepthCalibrationResult linear_result = calibrateDepth(mono_depth, stereo_depth_mm, disparity, valid_mask, left_bound_x);

    // 根据指定类型进行非线性校准
    switch (type) {
        case NonlinearCalibrationType::POLYNOMIAL: {
            // 收集有效点对
            std::vector<std::tuple<float, float, float>> validPoints;
            collectValidPoints(mono_depth, stereo_depth_mm, disparity, valid_mask, left_bound_x, validPoints);
            result = calibratePolynomial(validPoints, options_.polynomial_degree);
            break;
        }
        case NonlinearCalibrationType::RADIAL: {
            cv::Point2f center(mono_depth.cols / 2.0f, mono_depth.rows / 2.0f);
            result = calibrateRadial(mono_depth, stereo_depth_mm, valid_mask, center);
            break;
        }
        case NonlinearCalibrationType::GRID_BASED: {
            result = calibrateGridBased(mono_depth, stereo_depth_mm, valid_mask, options_.grid_size);
            break;
        }
        case NonlinearCalibrationType::ADAPTIVE: {
            // 自适应选择最佳方法
            std::vector<DepthCalibrationResult> candidates;

            // 尝试多项式校准
            std::vector<std::tuple<float, float, float>> validPoints;
            collectValidPoints(mono_depth, stereo_depth_mm, disparity, valid_mask, left_bound_x, validPoints);
            auto poly_result = calibratePolynomial(validPoints, 2);
            if (poly_result.success) candidates.push_back(poly_result);

            // 尝试径向校准
            cv::Point2f center(mono_depth.cols / 2.0f, mono_depth.rows / 2.0f);
            auto radial_result = calibrateRadial(mono_depth, stereo_depth_mm, valid_mask, center);
            if (radial_result.success) candidates.push_back(radial_result);

            // 选择RMS误差最小的方法
            if (!candidates.empty()) {
                result = *std::min_element(candidates.begin(), candidates.end(),
                    [](const DepthCalibrationResult& a, const DepthCalibrationResult& b) {
                        return a.nonlinear_rms_error < b.nonlinear_rms_error;
                    });
            }
            break;
        }
        default:
            result = linear_result;
            break;
    }

    // 如果非线性校准失败或效果不佳，回退到线性校准
    if (!result.success || (linear_result.success && result.nonlinear_rms_error > linear_result.rms_error * 1.1)) {
        result = linear_result;
        result.calibration_type = NonlinearCalibrationType::LINEAR;
    }

    return result;
}

DepthCalibrationResult ComprehensiveDepthProcessor::calibratePolynomial(const std::vector<std::tuple<float, float, float>>& validPoints,
                                                                       int degree) {
    DepthCalibrationResult result;
    result.calibration_type = NonlinearCalibrationType::POLYNOMIAL;

    if (validPoints.size() < (degree + 1) * 10) { // 需要足够的点
        return result;
    }

    // 构建多项式拟合矩阵
    int n = validPoints.size();
    cv::Mat A(n, degree + 1, CV_64F);
    cv::Mat b(n, 1, CV_64F);

    for (int i = 0; i < n; ++i) {
        float mono_val = std::get<0>(validPoints[i]);
        float stereo_val = std::get<1>(validPoints[i]);
        float weight = std::get<2>(validPoints[i]);

        // 构建多项式基函数 [1, x, x^2, x^3, ...]
        for (int j = 0; j <= degree; ++j) {
            A.at<double>(i, j) = std::pow(mono_val, j) * weight;
        }
        b.at<double>(i, 0) = stereo_val * weight;
    }

    // 最小二乘求解
    cv::Mat coeffs;
    if (cv::solve(A, b, coeffs, cv::DECOMP_SVD)) {
        result.polynomial_coeffs.clear();
        for (int i = 0; i <= degree; ++i) {
            result.polynomial_coeffs.push_back(coeffs.at<double>(i, 0));
        }

        // 计算RMS误差
        double sum_error = 0.0;
        for (const auto& point : validPoints) {
            float mono_val = std::get<0>(point);
            float stereo_val = std::get<1>(point);

            double predicted = 0.0;
            for (int j = 0; j <= degree; ++j) {
                predicted += result.polynomial_coeffs[j] * std::pow(mono_val, j);
            }

            double error = predicted - stereo_val;
            sum_error += error * error;
        }

        result.nonlinear_rms_error = std::sqrt(sum_error / validPoints.size());
        result.success = true;
        result.total_points = validPoints.size();
    }

    return result;
}

DepthCalibrationResult ComprehensiveDepthProcessor::calibrateRadial(const cv::Mat& mono_depth,
                                                                   const cv::Mat& stereo_depth_mm,
                                                                   const cv::Mat& valid_mask,
                                                                   const cv::Point2f& center) {
    DepthCalibrationResult result;
    result.calibration_type = NonlinearCalibrationType::RADIAL;
    result.image_center = center;

    // 收集径向距离和深度误差的关系
    std::vector<std::tuple<float, float, float>> radial_data; // (radius, depth_ratio, weight)

    for (int y = 0; y < mono_depth.rows; ++y) {
        const float* mono_ptr = mono_depth.ptr<float>(y);
        const float* stereo_ptr = stereo_depth_mm.ptr<float>(y);
        const uchar* mask_ptr = valid_mask.empty() ? nullptr : valid_mask.ptr<uchar>(y);

        for (int x = 0; x < mono_depth.cols; ++x) {
            if (mask_ptr && mask_ptr[x] == 0) continue;

            float mono_val = mono_ptr[x];
            float stereo_val = stereo_ptr[x];

            if (mono_val <= 0 || stereo_val <= 0 || !std::isfinite(mono_val) || !std::isfinite(stereo_val)) continue;

            // 计算到中心的径向距离
            float dx = x - center.x;
            float dy = y - center.y;
            float radius = std::sqrt(dx * dx + dy * dy);

            // 归一化径向距离
            float max_radius = std::sqrt(center.x * center.x + center.y * center.y);
            float norm_radius = radius / max_radius;

            float depth_ratio = stereo_val / mono_val;
            radial_data.emplace_back(norm_radius, depth_ratio, 1.0f);
        }
    }

    if (radial_data.size() < 100) {
        return result;
    }

    // 拟合径向校正模型: correction = 1 + k1*r^2 + k2*r^4 + k3*r^6
    int n = radial_data.size();
    int terms = options_.radial_terms;
    cv::Mat A(n, terms + 1, CV_64F);
    cv::Mat b(n, 1, CV_64F);

    for (int i = 0; i < n; ++i) {
        float radius = std::get<0>(radial_data[i]);
        float ratio = std::get<1>(radial_data[i]);

        A.at<double>(i, 0) = 1.0; // 常数项
        for (int j = 1; j <= terms; ++j) {
            A.at<double>(i, j) = std::pow(radius, 2 * j); // r^2, r^4, r^6, ...
        }
        b.at<double>(i, 0) = ratio;
    }

    cv::Mat coeffs;
    if (cv::solve(A, b, coeffs, cv::DECOMP_SVD)) {
        result.radial_coeffs.clear();
        for (int i = 0; i <= terms; ++i) {
            result.radial_coeffs.push_back(coeffs.at<double>(i, 0));
        }

        // 计算RMS误差
        double sum_error = 0.0;
        for (const auto& data : radial_data) {
            float radius = std::get<0>(data);
            float actual_ratio = std::get<1>(data);

            double predicted_ratio = result.radial_coeffs[0];
            for (int j = 1; j <= terms; ++j) {
                predicted_ratio += result.radial_coeffs[j] * std::pow(radius, 2 * j);
            }

            double error = predicted_ratio - actual_ratio;
            sum_error += error * error;
        }

        result.nonlinear_rms_error = std::sqrt(sum_error / radial_data.size());
        result.success = true;
        result.total_points = radial_data.size();
    }

    return result;
}

DepthCalibrationResult ComprehensiveDepthProcessor::calibrateGridBased(const cv::Mat& mono_depth,
                                                                      const cv::Mat& stereo_depth_mm,
                                                                      const cv::Mat& valid_mask,
                                                                      int grid_size) {
    DepthCalibrationResult result;
    result.calibration_type = NonlinearCalibrationType::GRID_BASED;

    if (mono_depth.empty() || stereo_depth_mm.empty()) {
        return result;
    }

    int rows = mono_depth.rows;
    int cols = mono_depth.cols;
    int grid_rows = grid_size;
    int grid_cols = grid_size;

    // 创建网格校正图
    result.grid_correction = cv::Mat::ones(grid_rows, grid_cols, CV_32F);

    int cell_height = rows / grid_rows;
    int cell_width = cols / grid_cols;

    double total_error = 0.0;
    int total_cells = 0;

    // 对每个网格单元计算校正因子
    for (int gy = 0; gy < grid_rows; ++gy) {
        for (int gx = 0; gx < grid_cols; ++gx) {
            int y_start = gy * cell_height;
            int y_end = std::min((gy + 1) * cell_height, rows);
            int x_start = gx * cell_width;
            int x_end = std::min((gx + 1) * cell_width, cols);

            // 收集该网格单元内的有效点
            std::vector<float> mono_vals, stereo_vals;
            for (int y = y_start; y < y_end; ++y) {
                const float* mono_ptr = mono_depth.ptr<float>(y);
                const float* stereo_ptr = stereo_depth_mm.ptr<float>(y);
                const uchar* mask_ptr = valid_mask.empty() ? nullptr : valid_mask.ptr<uchar>(y);

                for (int x = x_start; x < x_end; ++x) {
                    if (mask_ptr && mask_ptr[x] == 0) continue;

                    float mono_val = mono_ptr[x];
                    float stereo_val = stereo_ptr[x];

                    if (mono_val > 0 && stereo_val > 0 &&
                        std::isfinite(mono_val) && std::isfinite(stereo_val)) {
                        mono_vals.push_back(mono_val);
                        stereo_vals.push_back(stereo_val);
                    }
                }
            }

            // 计算该网格的校正因子
            if (mono_vals.size() >= 10) {
                double sum_mono = 0.0, sum_stereo = 0.0;
                for (size_t i = 0; i < mono_vals.size(); ++i) {
                    sum_mono += mono_vals[i];
                    sum_stereo += stereo_vals[i];
                }

                double mean_mono = sum_mono / mono_vals.size();
                double mean_stereo = sum_stereo / stereo_vals.size();

                if (mean_mono > 0) {
                    float correction_factor = static_cast<float>(mean_stereo / mean_mono);
                    result.grid_correction.at<float>(gy, gx) = correction_factor;

                    // 计算该网格的误差
                    double cell_error = 0.0;
                    for (size_t i = 0; i < mono_vals.size(); ++i) {
                        double predicted = mono_vals[i] * correction_factor;
                        double error = predicted - stereo_vals[i];
                        cell_error += error * error;
                    }
                    total_error += cell_error;
                    total_cells++;
                }
            }
        }
    }

    if (total_cells > 0) {
        result.nonlinear_rms_error = std::sqrt(total_error / (total_cells * cell_height * cell_width));
        result.success = true;
        result.total_points = total_cells;
    }

    return result;
}

float ComprehensiveDepthProcessor::detectPlaneCurvature(const cv::Mat& depth_mm, const cv::Mat& valid_mask) {
    if (depth_mm.empty()) return 0.0f;

    // 计算深度图的二阶导数来检测曲率
    cv::Mat depth_f;
    depth_mm.convertTo(depth_f, CV_32F);

    // 计算拉普拉斯算子
    cv::Mat laplacian;
    cv::Laplacian(depth_f, laplacian, CV_32F, 3);

    // 计算有效区域的拉普拉斯值的标准差作为曲率指标
    cv::Scalar mean, stddev;
    cv::meanStdDev(laplacian, mean, stddev, valid_mask);

    return static_cast<float>(stddev[0]);
}

cv::Mat ComprehensiveDepthProcessor::applyNonlinearCalibration(const cv::Mat& mono_depth,
                                                              const DepthCalibrationResult& calibration) {
    cv::Mat result = mono_depth.clone();

    switch (calibration.calibration_type) {
        case NonlinearCalibrationType::POLYNOMIAL: {
            // 应用多项式校准
            for (int y = 0; y < result.rows; ++y) {
                float* ptr = result.ptr<float>(y);
                for (int x = 0; x < result.cols; ++x) {
                    if (ptr[x] > 0) {
                        double corrected = 0.0;
                        for (size_t i = 0; i < calibration.polynomial_coeffs.size(); ++i) {
                            corrected += calibration.polynomial_coeffs[i] * std::pow(ptr[x], i);
                        }
                        ptr[x] = static_cast<float>(corrected);
                    }
                }
            }
            break;
        }
        case NonlinearCalibrationType::RADIAL: {
            // 应用径向校准
            cv::Point2f center = calibration.image_center;
            float max_radius = std::sqrt(center.x * center.x + center.y * center.y);

            for (int y = 0; y < result.rows; ++y) {
                float* ptr = result.ptr<float>(y);
                for (int x = 0; x < result.cols; ++x) {
                    if (ptr[x] > 0) {
                        // 计算径向距离
                        float dx = x - center.x;
                        float dy = y - center.y;
                        float radius = std::sqrt(dx * dx + dy * dy);
                        float norm_radius = radius / max_radius;

                        // 计算径向校正因子
                        double correction = calibration.radial_coeffs[0];
                        for (size_t i = 1; i < calibration.radial_coeffs.size(); ++i) {
                            correction += calibration.radial_coeffs[i] * std::pow(norm_radius, 2 * i);
                        }

                        ptr[x] = static_cast<float>(ptr[x] * correction);
                    }
                }
            }
            break;
        }
        case NonlinearCalibrationType::GRID_BASED: {
            // 应用网格校准（如果有网格校正图）
            if (!calibration.grid_correction.empty()) {
                cv::Mat correction_resized;
                cv::resize(calibration.grid_correction, correction_resized, result.size());
                result = result.mul(correction_resized);
            }
            break;
        }
        default:
            // 线性校准
            result = result * static_cast<float>(calibration.scale_factor) + static_cast<float>(calibration.bias);
            break;
    }

    return result;
}

void ComprehensiveDepthProcessor::collectValidPoints(const cv::Mat& mono_depth,
                                                    const cv::Mat& stereo_depth_mm,
                                                    const cv::Mat& disparity,
                                                    const cv::Mat& valid_mask,
                                                    int left_bound_x,
                                                    std::vector<std::tuple<float, float, float>>& validPoints) {
    validPoints.clear();
    validPoints.reserve(mono_depth.rows * mono_depth.cols / 4);

    // 计算梯度用于权重
    cv::Mat gradX, gradY, gradient;
    cv::Sobel(stereo_depth_mm, gradX, CV_32F, 1, 0);
    cv::Sobel(stereo_depth_mm, gradY, CV_32F, 0, 1);
    cv::magnitude(gradX, gradY, gradient);

    for (int y = 0; y < mono_depth.rows; ++y) {
        const float* mono_ptr = mono_depth.ptr<float>(y);
        const float* stereo_ptr = stereo_depth_mm.ptr<float>(y);
        const float* disp_ptr = disparity.ptr<float>(y);
        const float* grad_ptr = gradient.ptr<float>(y);
        const uchar* mask_ptr = valid_mask.empty() ? nullptr : valid_mask.ptr<uchar>(y);

        for (int x = std::max(0, left_bound_x); x < mono_depth.cols; ++x) {
            if (mask_ptr && mask_ptr[x] == 0) continue;

            float mono_val = mono_ptr[x];
            float stereo_val = stereo_ptr[x];
            float disp_val = disp_ptr[x];
            float grad_val = grad_ptr[x];

            if (!std::isfinite(mono_val) || !std::isfinite(stereo_val) || !std::isfinite(disp_val)) continue;
            if (mono_val <= 0.0f || stereo_val <= 0.0f || disp_val <= 0.0f) continue;

            float weight = calculateConfidenceWeight(disp_val, stereo_val, grad_val);
            validPoints.emplace_back(mono_val, stereo_val, weight);
        }
    }
}

} // namespace stereo_depth