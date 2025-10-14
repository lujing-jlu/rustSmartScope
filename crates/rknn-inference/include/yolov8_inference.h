#ifndef YOLOV8_INFERENCE_H
#define YOLOV8_INFERENCE_H

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

namespace yolov8 {

// 检测结果结构体
struct DetectionResult {
    int class_id;             // 类别ID
    float confidence;         // 置信度
    cv::Rect_<float> box;     // 边界框 (x, y, width, height)
    std::string class_name;   // 类别名称
};

class YOLOv8Inference {
public:
    /**
     * 构造函数
     */
    YOLOv8Inference();

    /**
     * 析构函数
     */
    ~YOLOv8Inference();

    /**
     * 初始化YOLO模型
     * 
     * @param model_path RKNN模型文件路径
     * @param label_path 标签文件路径
     * @return 是否初始化成功
     */
    bool initialize(const std::string& model_path, const std::string& label_path = "");

    /**
     * 对单张图像进行推理
     * 
     * @param image 输入图像 (BGR格式，cv::Mat)
     * @param min_confidence 最小置信度阈值，低于此值的检测结果会被过滤
     * @return 检测结果向量
     */
    std::vector<DetectionResult> inference(const cv::Mat& image, float min_confidence = 0.5f);
    
    /**
     * 在图像上绘制检测结果
     * 
     * @param image 输入/输出图像
     * @param results 检测结果向量
     */
    void drawResults(cv::Mat& image, const std::vector<DetectionResult>& results);

    /**
     * 设置NMS（非极大值抑制）阈值
     * 
     * @param nms_threshold NMS阈值 (0.0-1.0)
     */
    void setNMSThreshold(float nms_threshold);

    /**
     * 获取模型输入尺寸
     * 
     * @return 输入尺寸 (宽，高)
     */
    cv::Size getInputSize() const;

    /**
     * 释放资源
     */
    void release();

private:
    void* model_;                 // 模型指针（不透明类型）
    std::vector<std::string> labels_; // 标签列表
    float nms_threshold_;         // NMS阈值
    bool initialized_;            // 是否已初始化
};

} // namespace yolov8

#endif // YOLOV8_INFERENCE_H 