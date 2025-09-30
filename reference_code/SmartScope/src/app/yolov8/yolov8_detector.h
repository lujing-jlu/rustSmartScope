#ifndef YOLOV8_DETECTOR_H
#define YOLOV8_DETECTOR_H

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

// 前置声明YOLOv8Inference类，避免暴露内部实现
namespace yolov8 {
    class YOLOv8Inference;
    struct DetectionResult;
}

// 检测结果结构体
struct YOLOv8Detection {
    int class_id;             // 类别ID
    float confidence;         // 置信度
    cv::Rect_<float> box;     // 边界框 (x, y, width, height)
    std::string class_name;   // 类别名称
};

// YOLOv8检测器接口类
class YOLOv8Detector {
public:
    // 构造函数
    YOLOv8Detector();
    
    // 析构函数
    ~YOLOv8Detector();
    
    // 初始化检测器
    bool initialize(const std::string& model_path, const std::string& label_path = "");
    
    // 执行目标检测
    std::vector<YOLOv8Detection> detect(const cv::Mat& image, float min_confidence = 0.5f);
    
    // 在图像上绘制检测结果
    void drawDetections(cv::Mat& image, const std::vector<YOLOv8Detection>& detections);
    
    // 获取模型输入尺寸
    cv::Size getInputSize() const;
    
    // 设置NMS阈值
    void setNMSThreshold(float threshold);
    
    // 释放资源
    void release();
    
    // 是否已初始化
    bool isInitialized() const;

private:
    yolov8::YOLOv8Inference* yolo_; // YOLOv8推理引擎指针
    bool initialized_;              // 初始化状态
};

#endif // YOLOV8_DETECTOR_H 