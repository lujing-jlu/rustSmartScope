#pragma once

#include <memory>
#include <future>
#include <opencv2/opencv.hpp>

namespace depth_anything {

/**
 * @brief 推理引擎接口
 */
class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;

    /**
     * @brief 执行深度估计推理
     * @param image 输入图像
     * @param depth 输出深度图
     * @return 是否成功
     */
    virtual bool ComputeDepth(const cv::Mat& image, cv::Mat& depth) = 0;

    /**
     * @brief 异步执行深度估计推理
     * @param image 输入图像
     * @return 包含深度图的future
     */
    virtual std::future<cv::Mat> ComputeDepthAsync(const cv::Mat& image) = 0;

    /**
     * @brief 初始化异步管道
     */
    virtual void InitPipeline() = 0;

    /**
     * @brief 停止异步管道
     */
    virtual void StopPipeline() = 0;

    /**
     * @brief 关闭异步管道
     */
    virtual void ClosePipeline() = 0;
};

/**
 * @brief 创建RKNN推理引擎
 * @param model_path 模型文件路径
 * @param mem_buf_size 内存缓冲区大小
 * @param parallel_ctx_num 并行上下文数量
 * @return 推理引擎指针
 */
std::shared_ptr<InferenceEngine> CreateRknnInferCore(
    const std::string& model_path,
    int mem_buf_size = 5,
    int parallel_ctx_num = 3
);

/**
 * @brief 创建深度估计模型
 * @param engine 推理引擎
 * @param input_height 输入高度
 * @param input_width 输入宽度
 * @return 深度估计模型指针
 */
std::shared_ptr<InferenceEngine> CreateDepthAnythingModel(
    std::shared_ptr<InferenceEngine> engine,
    int input_height = 518,
    int input_width = 518
);

} // namespace depth_anything
