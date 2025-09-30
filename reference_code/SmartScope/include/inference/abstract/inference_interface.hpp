#ifndef INFERENCE_INTERFACE_HPP
#define INFERENCE_INTERFACE_HPP

#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <string>

namespace SmartScope {
namespace Inference {

// 推理结果基类
struct InferenceResult {
    virtual ~InferenceResult() = default;
    virtual bool isValid() const = 0;
    virtual std::string getType() const = 0;
};

// 检测结果
struct DetectionResult : public InferenceResult {
    int classId;
    float confidence;
    cv::Rect_<float> boundingBox;
    std::string className;
    
    bool isValid() const override { return confidence > 0.0f; }
    std::string getType() const override { return "detection"; }
};

// 深度结果
struct DepthResult : public InferenceResult {
    cv::Mat depthMap;
    cv::Mat confidenceMap;
    float maxDepth;
    float minDepth;
    
    bool isValid() const override { return !depthMap.empty(); }
    std::string getType() const override { return "depth"; }
};

// 推理接口抽象
class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;
    
    // 初始化推理引擎
    virtual bool initialize(const std::string& modelPath, const std::string& configPath = "") = 0;
    
    // 执行推理
    virtual std::shared_ptr<InferenceResult> infer(const cv::Mat& input) = 0;
    
    // 批量推理
    virtual std::vector<std::shared_ptr<InferenceResult>> inferBatch(const std::vector<cv::Mat>& inputs) = 0;
    
    // 获取模型信息
    virtual cv::Size getInputSize() const = 0;
    virtual std::string getModelType() const = 0;
    
    // 资源管理
    virtual void release() = 0;
    virtual bool isInitialized() const = 0;
};

// 推理服务接口
class IInferenceService {
public:
    virtual ~IInferenceService() = default;
    
    // 服务管理
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    
    // 异步推理
    virtual void requestInference(const cv::Mat& input, std::function<void(std::shared_ptr<InferenceResult>)> callback) = 0;
    
    // 同步推理
    virtual std::shared_ptr<InferenceResult> inferSync(const cv::Mat& input) = 0;
    
    // 服务状态
    virtual int getQueueSize() const = 0;
    virtual int getCompletedRequests() const = 0;
    virtual int getTotalRequests() const = 0;
};

} // namespace Inference
} // namespace SmartScope

#endif // INFERENCE_INTERFACE_HPP
