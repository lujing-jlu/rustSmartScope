#include "depth_anything_inference.hpp"
#include "rknn_core/rknn_core.h"
#include "deploy_core/base_stereo.h"
#include "deploy_core/base_detection.h"
#include "detection_2d_util/detection_2d_util.h"
#include "mono_stereo_depth_anything/depth_anything.hpp"
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <future>
#include <memory>

namespace depth_anything {

// 深度估计模型适配器
class DepthAnythingModelAdapter : public InferenceEngine {
public:
    DepthAnythingModelAdapter(std::shared_ptr<stereo::BaseMonoStereoModel> stereo_model)
        : stereo_model_(stereo_model) {
        if (!stereo_model_) {
            throw std::runtime_error("Stereo model is null");
        }
    }

    bool ComputeDepth(const cv::Mat& image, cv::Mat& depth) override {
        try {
            if (image.empty()) {
                return false;
            }
            
            return stereo_model_->ComputeDepth(image, depth);
        } catch (const std::exception& e) {
            return false;
        }
    }

    std::future<cv::Mat> ComputeDepthAsync(const cv::Mat& image) override {
        if (!stereo_model_) {
            std::promise<cv::Mat> promise;
            promise.set_exception(std::make_exception_ptr(std::runtime_error("Model not available")));
            return promise.get_future();
        }

        return stereo_model_->ComputeDepthAsync(image);
    }

    void InitPipeline() override {
        if (stereo_model_) {
            stereo_model_->InitPipeline();
        }
    }

    void StopPipeline() override {
        if (stereo_model_) {
            stereo_model_->StopPipeline();
        }
    }

    void ClosePipeline() override {
        if (stereo_model_) {
            stereo_model_->ClosePipeline();
        }
    }

private:
    std::shared_ptr<stereo::BaseMonoStereoModel> stereo_model_;
};

// RKNN推理引擎适配器
class RknnInferenceEngineAdapter : public InferenceEngine {
public:
    RknnInferenceEngineAdapter(const std::string& model_path, 
                              int mem_buf_size = 5, 
                              int parallel_ctx_num = 3)
        : model_path_(model_path), mem_buf_size_(mem_buf_size), parallel_ctx_num_(parallel_ctx_num) {
        InitializeEngine();
    }

    bool ComputeDepth(const cv::Mat& image, cv::Mat& depth) override {
        try {
            if (!infer_core_) {
                return false;
            }

            // 创建输入缓冲区
            auto buffer = infer_core_->AllocBlobsBuffer();
            if (!buffer) {
                return false;
            }

            // 设置输入数据
            // 这里需要根据具体的模型输入格式来设置数据
            // 暂时返回一个简单的深度图作为示例
            depth = cv::Mat(image.rows, image.cols, CV_32F, 1.0f);
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    std::future<cv::Mat> ComputeDepthAsync(const cv::Mat& image) override {
        return std::async(std::launch::async, [this, image]() {
            cv::Mat depth;
            ComputeDepth(image, depth);
            return depth;
        });
    }

    void InitPipeline() override {
        // RKNN引擎不需要特殊的初始化操作
    }

    void StopPipeline() override {
        // RKNN引擎不需要特殊的停止操作
    }

    void ClosePipeline() override {
        if (infer_core_) {
            infer_core_->Release();
        }
    }

    // 获取推理核心，供深度估计模型使用
    std::shared_ptr<inference_core::BaseInferCore> GetInferCore() const {
        return infer_core_;
    }

private:
    void InitializeEngine() {
        try {
            // 创建RKNN推理核心
            std::unordered_map<std::string, inference_core::RknnInputTensorType> input_types = {
                {"images", inference_core::RknnInputTensorType::RK_UINT8}
            };

            infer_core_ = inference_core::CreateRknnInferCore(
                model_path_, input_types, mem_buf_size_, parallel_ctx_num_);

            if (!infer_core_) {
                throw std::runtime_error("Failed to create RKNN inference core");
            }

        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to initialize RKNN inference engine: " + std::string(e.what()));
        }
    }

    std::string model_path_;
    int mem_buf_size_;
    int parallel_ctx_num_;
    std::shared_ptr<inference_core::BaseInferCore> infer_core_;
};

// 工厂函数实现
std::shared_ptr<InferenceEngine> CreateRknnInferCore(
    const std::string& model_path,
    int mem_buf_size,
    int parallel_ctx_num) {
    
    try {
        return std::make_shared<RknnInferenceEngineAdapter>(
            model_path, mem_buf_size, parallel_ctx_num);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create RKNN inference engine: " + std::string(e.what()));
    }
}

std::shared_ptr<InferenceEngine> CreateDepthAnythingModel(
    std::shared_ptr<InferenceEngine> engine,
    int input_height,
    int input_width) {
    
    try {
        // 如果传入的是RKNN引擎，创建完整的深度估计模型
        if (auto rknn_engine = std::dynamic_pointer_cast<RknnInferenceEngineAdapter>(engine)) {
            // 创建预处理块
            auto preprocess_block = detection_2d::CreateCpuDetPreProcess(
                {123.675, 116.28, 103.53}, 
                {58.395, 57.12, 57.375}, 
                false, false);

            // 获取推理核心
            auto infer_core = rknn_engine->GetInferCore();
            if (!infer_core) {
                throw std::runtime_error("Failed to get inference core from RKNN engine");
            }

            // 创建深度估计模型
            auto stereo_model = stereo::CreateDepthAnythingModel(
                infer_core, preprocess_block, input_height, input_width,
                {"images"}, {"depth"});

            return std::make_shared<DepthAnythingModelAdapter>(stereo_model);
        }

        // 如果传入的是深度估计模型适配器，直接返回
        if (auto depth_adapter = std::dynamic_pointer_cast<DepthAnythingModelAdapter>(engine)) {
            return depth_adapter;
        }

        // 其他情况，返回原始引擎
        return engine;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create depth anything model: " + std::string(e.what()));
    }
}

} // namespace depth_anything
