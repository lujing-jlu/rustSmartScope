#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include <string>

// 前向声明
namespace stereo_depth {
    class ComprehensiveDepthProcessor;
    struct ComprehensiveDepthOptions;
}

namespace SmartScope {

/**
 * @brief 立体深度适配器类
 * 
 * 将 stereo_depth_lib 的 ComprehensiveDepthProcessor 包装成与 StereoInference 兼容的接口
 */
class StereoDepthAdapter {
public:
    /**
     * @brief 性能模式枚举（兼容 StereoInference）
     */
    enum PerformanceMode {
        HIGH_QUALITY,   // 高质量模式
        BALANCED,       // 平衡模式
        FAST,           // 快速模式
        ULTRA_FAST      // 超快模式
    };

    /**
     * @brief 构造函数
     * @param camera_param_dir 相机参数目录
     * @param mono_model_path 单目深度模型路径
     */
    StereoDepthAdapter(const std::string& camera_param_dir, 
                      const std::string& mono_model_path);
    
    /**
     * @brief 析构函数
     */
    ~StereoDepthAdapter();

    /**
     * @brief 主要推理接口（兼容 StereoInference）
     * @param left_img 左图像
     * @param right_img 右图像
     * @return 视差图
     */
    cv::Mat inference(const cv::Mat& left_img, const cv::Mat& right_img);
    
    /**
     * @brief 设置性能模式
     * @param mode 性能模式
     */
    void setPerformanceMode(PerformanceMode mode);
    
    /**
     * @brief 获取当前性能模式
     * @return 当前性能模式
     */
    PerformanceMode getPerformanceMode() const;

    /**
     * @brief 保存视差图
     * @param disparity 视差图
     * @param filename 文件名
     */
    void saveDisparity(const cv::Mat& disparity, const std::string& filename);

    /**
     * @brief 保存点云
     * @param disparity 视差图
     * @param color 彩色图像
     * @param filename 文件名
     * @param baseline 基线长度
     * @param focal_length 焦距
     */
    void savePointCloud(const cv::Mat& disparity, const cv::Mat& color, 
                       const std::string& filename,
                       float baseline = 0.23f, float focal_length = -1.0f);

private:
    std::unique_ptr<stereo_depth::ComprehensiveDepthProcessor> processor_;
    PerformanceMode current_mode_;
    stereo_depth::ComprehensiveDepthOptions options_;
    
    /**
     * @brief 根据性能模式更新配置选项
     * @param mode 性能模式
     */
    void updateOptionsForMode(PerformanceMode mode);
};

} // namespace SmartScope 