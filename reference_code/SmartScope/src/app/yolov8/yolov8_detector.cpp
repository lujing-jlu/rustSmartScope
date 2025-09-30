#include "yolov8_detector.h"
#include "rknn_inference/include/yolov8_inference.h"

YOLOv8Detector::YOLOv8Detector()
    : yolo_(nullptr), initialized_(false) {
    // 创建YOLOv8推理引擎实例
    yolo_ = new yolov8::YOLOv8Inference();
}

YOLOv8Detector::~YOLOv8Detector() {
    release();
}

bool YOLOv8Detector::initialize(const std::string& model_path, const std::string& label_path) {
    if (!yolo_) return false;
    
    // 调用YOLOv8推理引擎的初始化方法
    initialized_ = yolo_->initialize(model_path, label_path);
    return initialized_;
}

std::vector<YOLOv8Detection> YOLOv8Detector::detect(const cv::Mat& image, float min_confidence) {
    std::vector<YOLOv8Detection> detections;
    if (!initialized_ || !yolo_) return detections;
    
    // 调用YOLOv8推理引擎进行推理
    std::vector<yolov8::DetectionResult> results = yolo_->inference(image, min_confidence);
    
    // 转换检测结果
    for (const auto& result : results) {
        YOLOv8Detection detection;
        detection.class_id = result.class_id;
        detection.confidence = result.confidence;
        detection.box = result.box;
        detection.class_name = result.class_name;
        detections.push_back(detection);
    }
    
    return detections;
}

void YOLOv8Detector::drawDetections(cv::Mat& image, const std::vector<YOLOv8Detection>& detections) {
    if (!initialized_ || !yolo_) return;
    
    // 创建与yolov8::DetectionResult兼容的结果列表
    std::vector<yolov8::DetectionResult> results;
    for (const auto& detection : detections) {
        yolov8::DetectionResult result;
        result.class_id = detection.class_id;
        result.confidence = detection.confidence;
        result.box = detection.box;
        result.class_name = detection.class_name;
        results.push_back(result);
    }
    
    // 调用YOLOv8推理引擎的绘制方法
    yolo_->drawResults(image, results);
}

cv::Size YOLOv8Detector::getInputSize() const {
    if (!initialized_ || !yolo_) return cv::Size(0, 0);
    return yolo_->getInputSize();
}

void YOLOv8Detector::setNMSThreshold(float threshold) {
    if (!yolo_) return;
    yolo_->setNMSThreshold(threshold);
}

void YOLOv8Detector::release() {
    if (yolo_) {
        yolo_->release();
        delete yolo_;
        yolo_ = nullptr;
    }
    initialized_ = false;
}

bool YOLOv8Detector::isInitialized() const {
    return initialized_;
} 