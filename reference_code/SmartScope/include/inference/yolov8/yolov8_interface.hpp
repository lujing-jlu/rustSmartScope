#ifndef YOLOV8_INTERFACE_HPP
#define YOLOV8_INTERFACE_HPP

#include "inference/abstract/inference_interface.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

namespace SmartScope {
namespace Inference {

// YOLOv8检测结果
struct YOLOv8Detection : public DetectionResult {
    // 继承自DetectionResult，添加YOLOv8特有属性
    float nmsScore;  // NMS后的分数
    std::vector<cv::Point2f> keypoints;  // 关键点（如果有）
    
    YOLOv8Detection() : nmsScore(0.0f) {}
};

// YOLOv8推理引擎接口
class IYOLOv8Engine : public IInferenceEngine {
public:
    virtual ~IYOLOv8Engine() = default;
    
    // YOLOv8特有方法
    virtual void setConfidenceThreshold(float threshold) = 0;
    virtual void setNMSThreshold(float threshold) = 0;
    virtual float getConfidenceThreshold() const = 0;
    virtual float getNMSThreshold() const = 0;
    
    // 加载标签文件
    virtual bool loadLabels(const std::string& labelPath) = 0;
    
    // 获取类别数量
    virtual int getNumClasses() const = 0;
    
    // 获取类别名称
    virtual std::string getClassName(int classId) const = 0;
};

// YOLOv8服务接口
class IYOLOv8Service : public IInferenceService {
public:
    virtual ~IYOLOv8Service() = default;
    
    // YOLOv8服务特有方法
    virtual void setDetectionCallback(std::function<void(const std::vector<YOLOv8Detection>&)> callback) = 0;
    virtual void setErrorCallback(std::function<void(const std::string&)> callback) = 0;
    
    // 批量检测
    virtual void requestBatchDetection(const std::vector<cv::Mat>& images, 
                                     std::function<void(const std::vector<std::vector<YOLOv8Detection>>&)> callback) = 0;
    
    // 服务配置
    virtual void setMaxBatchSize(int maxBatchSize) = 0;
    virtual void setThreadPoolSize(int threadPoolSize) = 0;
    virtual int getMaxBatchSize() const = 0;
    virtual int getThreadPoolSize() const = 0;
};

} // namespace Inference
} // namespace SmartScope

#endif // YOLOV8_INTERFACE_HPP 