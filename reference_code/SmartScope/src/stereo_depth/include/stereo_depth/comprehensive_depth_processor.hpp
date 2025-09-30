#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include "stereo_depth/enhanced_postprocessing.h"

// 前向声明
namespace depth_anything {
    class InferenceEngine;
}

namespace stereo_depth {

enum class FusionMode {
    None = 0,
    MonoSmoothStereo = 1
};

/**
 * @brief 非线性校准类型
 */
enum class NonlinearCalibrationType {
    LINEAR = 0,           // 线性校准（当前方法）
    POLYNOMIAL = 1,       // 多项式校准
    RADIAL = 2,          // 径向校准
    GRID_BASED = 3,      // 基于网格的校准
    ADAPTIVE = 4         // 自适应校准
};

/**
 * @brief 综合深度处理器配置选项
 */
struct ComprehensiveDepthOptions {
    // SGBM参数
    int min_disparity = 0;
    int num_disparities = 16 * 8; // 必须是16倍数
    int block_size = 5;           // 奇数
    int uniqueness_ratio = 10;
    int speckle_window = 100;
    int speckle_range = 32;
    int prefilter_cap = 63;
    int disp12_max_diff = 1;
    
    // 深度校准参数
    int min_samples = 1000;
    int ransac_max_iterations = 50;
    float ransac_threshold = 30.0f;
    int min_inliers_ratio = 10; // 最小内点比例（百分比）
    
    // 后处理控制
    bool enable_enhanced_postprocessing = false;
    
    // 融合与修复（新策略）
    FusionMode fusion_mode = FusionMode::MonoSmoothStereo;
    float fusion_confidence_gamma = 1.0f;    // α = conf^gamma
    float fusion_confidence_thresh = 0.3f;   // τc：注入Hstereo的置信度阈值
    int fusion_bilateral_d = 5;              // 双边核
    double fusion_bilateral_sigma_s = 7.0;   // 空间sigma
    double fusion_bilateral_sigma_r = 50.0;  // 颜色/值sigma（以mm尺度）
    float fusion_edge_grad_thresh = 0.02f;   // 边缘梯度阈值（归一化或按mm梯度）
    
    // 新增：平面检测参数
    float plane_detection_threshold = 5.0f;    // 平面检测阈值（毫米）
    int plane_min_points = 100;                // 最小平面点数
    float max_plane_angle = 15.0f;             // 最大平面倾斜角度（度）
    float camera_tilt_threshold = 10.0f;       // 相机倾斜阈值（度）
    bool enable_plane_based_layering = true;   // 启用基于平面的分层

    // 新增：非线性校准参数
    NonlinearCalibrationType nonlinear_type = NonlinearCalibrationType::ADAPTIVE;
    int polynomial_degree = 2;                 // 多项式阶数（2或3）
    int radial_terms = 2;                      // 径向校准项数
    int grid_size = 8;                         // 网格校准的网格大小
    float nonlinear_threshold = 5.0f;          // 非线性校准阈值（毫米）
    bool enable_nonlinear_calibration = true;  // 启用非线性校准
    float curvature_detection_threshold = 2.0f; // 曲率检测阈值
    
    // 置信度权重参数
    float disparity_weight_scale = 30.0f;
    float depth_weight_scale = 1500.0f;
    float gradient_weight_scale = 5.0f;
    
    // 点云生成参数
    float min_depth_mm = 0.0f;
    float max_depth_mm = 10000.0f;
    
    // 预处理参数
    double scale = 1.0;
    int bilateral_d = 9;
    double bilateral_sigma = 75.0;
    int gauss_kernel = 5;
    int median_kernel = 3;
};

/**
 * @brief 深度校准结果
 */
struct DepthCalibrationResult {
    double scale_factor = 1.0;
    double bias = 0.0;
    bool success = false;
    int total_points = 0;
    int inlier_points = 0;
    double rms_error = 0.0;

    // 新增：分层校准相关字段
    int layer_index = -1;           // 层索引，-1表示孔洞区域
    float depth_range_min = 0.0f;   // 深度范围最小值
    float depth_range_max = 0.0f;   // 深度范围最大值
    int sample_count = 0;           // 样本数量

    // 新增：平面结构信息
    bool is_planar_region = false;  // 是否为平面区域
    cv::Vec3f plane_normal;         // 平面法向量
    float plane_angle = 0.0f;       // 平面倾斜角度（度）
    float camera_tilt_angle = 0.0f; // 相机倾斜角度（度）
    cv::Point3f plane_center;       // 平面中心点

    // 新增：非线性校准参数
    NonlinearCalibrationType calibration_type = NonlinearCalibrationType::LINEAR;
    std::vector<double> polynomial_coeffs;  // 多项式系数 [a0, a1, a2, a3, ...]
    std::vector<double> radial_coeffs;      // 径向校准系数 [k1, k2, k3, ...]
    cv::Point2f image_center;               // 图像中心点（用于径向校准）
    cv::Mat grid_correction;                // 网格校正图（CV_32FC1）
    double nonlinear_rms_error = 0.0;       // 非线性校准的RMS误差
};

/**
 * @brief 综合深度处理结果
 */
struct ComprehensiveDepthResult {
    cv::Mat stereo_depth_mm;           // 双目深度图（毫米）
    cv::Mat mono_depth_raw;            // 单目原始深度图
    cv::Mat mono_depth_calibrated_mm;  // 单目校准后深度图（毫米）
    cv::Mat disparity;                 // 视差图
    cv::Mat confidence_map;            // 置信度图
    DepthCalibrationResult calibration; // 校准结果
    bool success = false;
    
    // 新增：细粒度中间结果
    cv::Mat left_preprocessed;         // 预处理后的左图
    cv::Mat right_preprocessed;        // 预处理后的右图
    cv::Mat left_gray;                 // 灰度左图
    cv::Mat right_gray;                // 灰度右图
    cv::Mat disparity_raw;             // 原始视差图（16位）
    cv::Mat points_3d;                 // 3D点云数据
    cv::Mat valid_mask;                // 有效像素掩码
    cv::Mat gradient_magnitude;        // 梯度幅值图
    cv::Mat final_fused_depth;         // 最终融合深度图
};

/**
 * @brief 细粒度处理选项
 */
struct FineGrainedOptions {
    bool save_preprocessed_images = true;    // 保存预处理图像
    bool save_gray_images = true;           // 保存灰度图像
    bool save_raw_disparity = true;         // 保存原始视差图
    bool save_3d_points = false;            // 保存3D点云数据
    bool save_valid_mask = true;            // 保存有效像素掩码
    bool save_gradient = true;              // 保存梯度图
    bool enable_depth_fusion = true;        // 启用深度融合
    bool save_final_fused_depth = true;     // 保存最终融合深度图
};

/**
 * @brief 综合深度处理器
 * 
 * 功能包括：
 * 1. 双目立体视觉深度计算
 * 2. 单目深度估计（Depth Anything）
 * 3. 深度校准和融合
 * 4. 点云生成
 */
class ComprehensiveDepthProcessor {
public:
    /**
     * @brief 构造函数
     * @param camera_param_dir 相机参数目录
     * @param mono_model_path 单目深度模型路径
     * @param options 配置选项
     */
    ComprehensiveDepthProcessor(const std::string& camera_param_dir,
                               const std::string& mono_model_path,
                               const ComprehensiveDepthOptions& options = ComprehensiveDepthOptions{});
    
    /**
     * @brief 析构函数
     */
    ~ComprehensiveDepthProcessor();
    
    /**
     * @brief 处理校正后的左右图像
     * @param left_rectified 校正后的左图
     * @param right_rectified 校正后的右图
     * @return 综合深度处理结果
     */
    ComprehensiveDepthResult processRectifiedImages(const cv::Mat& left_rectified,
                                                   const cv::Mat& right_rectified);
    
    /**
     * @brief 处理已经校正过的图像（跳过立体校正步骤）
     * @param left_rectified 已经校正的左图
     * @param right_rectified 已经校正的右图
     * @param Q_matrix 重投影矩阵（用于视差到深度的转换）
     * @return 综合深度处理结果
     */
    ComprehensiveDepthResult processAlreadyRectifiedImages(const cv::Mat& left_rectified,
                                                          const cv::Mat& right_rectified,
                                                          const cv::Mat& Q_matrix = cv::Mat());
    
    /**
     * @brief 细粒度处理校正后的左右图像（包含所有中间结果）
     * @param left_rectified 校正后的左图
     * @param right_rectified 校正后的右图
     * @param fine_options 细粒度处理选项
     * @return 包含所有中间结果的综合深度处理结果
     */
    ComprehensiveDepthResult processRectifiedImagesFineGrained(const cv::Mat& left_rectified,
                                                              const cv::Mat& right_rectified,
                                                              const FineGrainedOptions& fine_options = FineGrainedOptions{});
    
    /**
     * @brief 仅计算视差图
     * @param left_rectified 校正后的左图
     * @param right_rectified 校正后的右图
     * @return 视差图
     */
    cv::Mat computeDisparityOnly(const cv::Mat& left_rectified,
                                const cv::Mat& right_rectified);
    
    /**
     * @brief 仅计算双目深度图
     * @param left_rectified 校正后的左图
     * @param right_rectified 校正后的右图
     * @return 双目深度图（毫米）
     */
    cv::Mat computeStereoDepthOnly(const cv::Mat& left_rectified,
                                  const cv::Mat& right_rectified);
    
    /**
     * @brief 仅计算单目深度图
     * @param left_rectified 校正后的左图
     * @return 单目深度图
     */
    cv::Mat computeMonoDepthOnly(const cv::Mat& left_rectified);
    
    /**
     * @brief 深度融合（双目深度 + 单目深度）
     * @param stereo_depth 双目深度图
     * @param mono_depth 单目深度图
     * @param confidence_map 置信度图
     * @return 融合后的深度图
     */
    cv::Mat fuseDepthMaps(const cv::Mat& stereo_depth,
                         const cv::Mat& mono_depth,
                         const cv::Mat& confidence_map);
    
    /**
     * @brief 获取处理步骤的中间结果
     * @param step_name 步骤名称 ("preprocessed", "disparity", "stereo_depth", "mono_depth", "calibrated", "fused")
     * @return 对应的中间结果图像
     */
    cv::Mat getIntermediateResult(const std::string& step_name) const;
    
    /**
     * @brief 保存RGB点云为PLY文件
     * @param color_image 彩色图像
     * @param depth_image 深度图像（毫米）
     * @param filename 输出文件名
     * @param comment 文件注释
     * @return 是否成功
     */
    bool saveRGBPointCloud(const cv::Mat& color_image,
                          const cv::Mat& depth_image,
                          const std::string& filename,
                          const std::string& comment = "") const;
    
    /**
     * @brief 获取Q矩阵（用于点云生成）
     * @return Q矩阵
     */
    cv::Mat getQMatrix() const;
    
    /**
     * @brief 获取ROI信息
     * @return ROI信息对 (left_roi, right_roi)
     */
    std::pair<cv::Rect, cv::Rect> getROI() const;
    
    /**
     * @brief 更新配置选项
     * @param options 新的配置选项
     */
    void updateOptions(const ComprehensiveDepthOptions& options);

    // 新增：外部注入Q矩阵（用于已校正图路径）
    void setQMatrix(const cv::Mat& Q_matrix);

    // 新增：模块化API，供主流程按步骤调用
    // 根据视差和Q计算米制深度
    cv::Mat depthFromDisparity(const cv::Mat& disparity32F, const cv::Mat& Q_matrix);
    // 简单深度过滤（可后续替换策略）
    cv::Mat filterDepth(const cv::Mat& depth_mm, const cv::Mat& valid_mask = cv::Mat());
    // 构建置信度图
    cv::Mat buildConfidenceMap(const cv::Mat& disparity32F, const cv::Mat& stereo_depth_mm);
    // 对单目深度进行标定
    DepthCalibrationResult calibrateMonoToStereo(const cv::Mat& mono_depth,
                                                 const cv::Mat& stereo_depth_mm,
                                                 const cv::Mat& disparity,
                                                 const cv::Mat& valid_mask,
                                                 int left_bound_x,
                                                 cv::Mat& mono_calibrated_out);

    /**
     * @brief 增强的分层校准方法（解决异常值和孔洞问题）
     */
    DepthCalibrationResult calibrateDepthLayered(const cv::Mat& mono_depth,
                                                 const cv::Mat& stereo_depth_mm,
                                                 const cv::Mat& disparity,
                                                 const cv::Mat& valid_mask,
                                                 int left_bound_x);

    /**
     * @brief 异常值检测
     */
    cv::Mat detectAnomalies(const cv::Mat& depth, const cv::Mat& disparity,
                           float local_threshold = 2.0f, int window_size = 5);

    /**
     * @brief 孔洞区域检测
     */
    cv::Mat detectHoleRegions(const cv::Mat& depth, const cv::Mat& disparity,
                             float hole_depth_threshold = 500.0f, int min_hole_size = 50);

    /**
     * @brief 自适应权重计算
     */
    cv::Mat calculateAdaptiveWeights(const cv::Mat& mono_depth,
                                    const cv::Mat& stereo_depth_mm,
                                    const cv::Mat& disparity,
                                    const cv::Mat& anomalies);

    /**
     * @brief 单层校准
     */
    DepthCalibrationResult calibrateDepthLayer(const cv::Mat& mono_depth,
                                              const cv::Mat& stereo_depth_mm,
                                              const cv::Mat& disparity,
                                              const cv::Mat& layer_mask,
                                              const cv::Mat& weights = cv::Mat());

    /**
     * @brief 孔洞区域校准
     */
    DepthCalibrationResult calibrateHoleRegions(const cv::Mat& mono_depth,
                                               const cv::Mat& stereo_depth_mm,
                                               const cv::Mat& disparity,
                                               const cv::Mat& hole_mask);

    /**
     * @brief 融合各层校准结果
     */
    DepthCalibrationResult fuseLayerResults(const std::vector<DepthCalibrationResult>& layer_results,
                                           const cv::Mat& stereo_depth_mm);

    /**
     * @brief 基于平面结构的分层校准（改进版）
     */
    DepthCalibrationResult calibrateDepthPlanarLayered(const cv::Mat& mono_depth,
                                                       const cv::Mat& stereo_depth_mm,
                                                       const cv::Mat& disparity,
                                                       const cv::Mat& valid_mask,
                                                       int left_bound_x);

    /**
     * @brief 非线性深度校准方法
     */
    DepthCalibrationResult calibrateDepthNonlinear(const cv::Mat& mono_depth,
                                                   const cv::Mat& stereo_depth_mm,
                                                   const cv::Mat& disparity,
                                                   const cv::Mat& valid_mask,
                                                   int left_bound_x,
                                                   NonlinearCalibrationType type = NonlinearCalibrationType::ADAPTIVE);

    /**
     * @brief 多项式校准
     */
    DepthCalibrationResult calibratePolynomial(const std::vector<std::tuple<float, float, float>>& validPoints,
                                              int degree = 2);

    /**
     * @brief 径向校准
     */
    DepthCalibrationResult calibrateRadial(const cv::Mat& mono_depth,
                                          const cv::Mat& stereo_depth_mm,
                                          const cv::Mat& valid_mask,
                                          const cv::Point2f& center);

    /**
     * @brief 网格校准
     */
    DepthCalibrationResult calibrateGridBased(const cv::Mat& mono_depth,
                                             const cv::Mat& stereo_depth_mm,
                                             const cv::Mat& valid_mask,
                                             int grid_size = 8);

    /**
     * @brief 检测平面曲率
     */
    float detectPlaneCurvature(const cv::Mat& depth_mm, const cv::Mat& valid_mask);

    /**
     * @brief 应用非线性校准
     */
    cv::Mat applyNonlinearCalibration(const cv::Mat& mono_depth,
                                     const DepthCalibrationResult& calibration);

    /**
     * @brief 收集有效点对用于校准
     */
    void collectValidPoints(const cv::Mat& mono_depth,
                           const cv::Mat& stereo_depth_mm,
                           const cv::Mat& disparity,
                           const cv::Mat& valid_mask,
                           int left_bound_x,
                           std::vector<std::tuple<float, float, float>>& validPoints);

    /**
     * @brief 平面检测
     */
    std::vector<cv::Vec4f> detectPlanes(const cv::Mat& depth_mm,
                                       const cv::Mat& valid_mask,
                                       float threshold = 5.0f,
                                       int min_points = 100);

    /**
     * @brief 计算平面法向量和角度
     */
    cv::Vec3f calculatePlaneNormal(const cv::Mat& depth_mm,
                                  const cv::Mat& valid_mask,
                                  const cv::Rect& roi);

    /**
     * @brief 估计相机倾斜角度
     */
    float estimateCameraTilt(const cv::Mat& depth_mm,
                            const cv::Mat& valid_mask);

    /**
     * @brief 基于平面结构的分层
     */
    std::vector<cv::Mat> createPlanarLayers(const cv::Mat& depth_mm,
                                           const cv::Mat& valid_mask,
                                           const std::vector<cv::Vec4f>& planes);

    /**
     * @brief 使用单目深度(图3)作为参考，对双目深度(图2)进行分块鲁棒拟合与异常点筛除/优化
     * @param stereo_depth_mm 双目深度图（毫米，CV_32F）
     * @param mono_depth_ref 单目参考深度图（相对深度，CV_32F）；将通过局部线性(缩放+偏置)拟合到双目量纲
     * @param block_size 分块大小（像素），如 64
     * @param overlap 分块重叠（像素），如 16
     * @param residual_thresh 残差阈值（毫米），大于阈值的点视为异常并用拟合值替换或置零
     * @param replace_outliers 是否用拟合值替换异常点（true）否则置零
     * @return 优化后的双目深度图（CV_32F, 毫米）
     */
    cv::Mat refineStereoWithMonoLocalFit(const cv::Mat& stereo_depth_mm,
                                         const cv::Mat& mono_depth_ref,
                                         int block_size = 64,
                                         int overlap = 16,
                                         float residual_thresh = 10.0f,
                                         bool replace_outliers = true);

private:
    // 相机参数读取
    bool readIntrinsics(const std::string& path, cv::Mat& K, cv::Mat& D);
    bool readRotTrans(const std::string& path, cv::Mat& R, cv::Mat& T);
    
    // 初始化
    void initialize();
    
    // 图像预处理
    void preprocessImage(const cv::Mat& src, cv::Mat& dst);
    
    // 置信度权重计算
    float calculateConfidenceWeight(float disparity, float depth, float gradient = 0.0f) const;
    
    // RANSAC鲁棒拟合
    bool ransacLinearFit(const std::vector<std::tuple<float, float, float>>& points,
                        double& s_out, double& b_out);
    
    // 加权最小二乘拟合
    bool weightedLinearFit(const std::vector<std::tuple<float, float, float>>& points,
                          double& s_out, double& b_out);
    
    // 深度校准
    DepthCalibrationResult calibrateDepth(const cv::Mat& mono_depth,
                                         const cv::Mat& stereo_depth_mm,
                                         const cv::Mat& disparity,
                                         const cv::Mat& valid_mask,
                                         int left_bound_x);

private:
    std::string camera_param_dir_;
    std::string mono_model_path_;
    ComprehensiveDepthOptions options_;
    
    // 相机参数
    cv::Mat K0_, D0_, K1_, D1_, R_, T_;
    cv::Mat R1_, R2_, P1_, P2_, Q_;
    cv::Rect roi1_, roi2_;
    cv::Mat map1x_, map1y_, map2x_, map2y_;
    
    // SGBM
    cv::Ptr<cv::StereoSGBM> sgbm_;
    
    // 单目深度模型
    std::shared_ptr<depth_anything::InferenceEngine> mono_engine_;
    std::shared_ptr<depth_anything::InferenceEngine> mono_model_;
    
    // 状态
    bool initialized_ = false;
    cv::Size image_size_;
    
    // 增强后处理器
    std::unique_ptr<EnhancedPostProcessor> enhanced_postprocessor_;
    
    // 新增：中间结果存储（可通过 getIntermediateResult 获取："preprocessed", "disparity", "stereo_depth", "mono_depth", "calibrated", "fused", "calibration_mask"）
    mutable cv::Mat last_left_preprocessed_;
    mutable cv::Mat last_right_preprocessed_;
    mutable cv::Mat last_left_gray_;
    mutable cv::Mat last_right_gray_;
    mutable cv::Mat last_disparity_raw_;
    mutable cv::Mat last_disparity_;
    mutable cv::Mat last_stereo_depth_;
    mutable cv::Mat last_mono_depth_raw_;
    mutable cv::Mat last_mono_depth_calibrated_;
    mutable cv::Mat last_points_3d_;
    mutable cv::Mat last_valid_mask_;
    mutable cv::Mat last_gradient_magnitude_;
    mutable cv::Mat last_final_fused_depth_;
    // 供调试用：校准时使用的强连通且≤阈值深度的掩码
    mutable cv::Mat last_calibration_mask_;
};

} // namespace stereo_depth 
