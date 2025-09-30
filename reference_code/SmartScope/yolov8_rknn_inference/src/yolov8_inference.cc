#include "../include/yolov8_inference.h"
#include "../include/yolov8.h"
#include "../include/postprocess.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cassert>

namespace yolov8 {

// 使用来自yolov8.h的实际模型结构
struct YOLOv8Model {
    // 继承rknn_app_context_t所有字段
    rknn_context rknn_ctx;
    rknn_input_output_num io_num;
    rknn_tensor_attr* input_attrs;
    rknn_tensor_attr* output_attrs;
    int model_channel;
    int model_width;
    int model_height;
    bool is_quant;
};

YOLOv8Inference::YOLOv8Inference() 
    : model_(nullptr), nms_threshold_(0.45f), initialized_(false) {
}

YOLOv8Inference::~YOLOv8Inference() {
    release();
}

bool YOLOv8Inference::initialize(const std::string& model_path, const std::string& label_path) {
    // 如果已经初始化，先释放资源
    if (initialized_) {
        release();
    }

    // 加载标签文件
    if (!label_path.empty()) {
        std::ifstream file(label_path);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                labels_.push_back(line);
            }
            file.close();
        } else {
            printf("无法打开标签文件: %s\n", label_path.c_str());
        }
    }

    // 分配并初始化模型
    YOLOv8Model* yolo_model = new YOLOv8Model();
    model_ = yolo_model;

    // 初始化RKNN环境
    int ret = init_yolov8_model(model_path.c_str(), (rknn_app_context_t*)yolo_model);
    if (ret < 0) {
        printf("初始化模型失败\n");
        delete yolo_model;
        model_ = nullptr;
        return false;
    }

    // 获取模型输入大小
    rknn_input_output_num io_num;
    ret = rknn_query(yolo_model->rknn_ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0) {
        printf("查询模型输入输出数量失败\n");
        release();
        return false;
    }

    // 获取并配置输入信息
    rknn_tensor_attr input_attr;
    memset(&input_attr, 0, sizeof(input_attr));
    input_attr.index = 0;
    ret = rknn_query(yolo_model->rknn_ctx, RKNN_QUERY_INPUT_ATTR, &input_attr, sizeof(input_attr));
    if (ret < 0) {
        printf("查询输入属性失败\n");
        release();
        return false;
    }

    // 模型的尺寸已经在init_yolov8_model中设置好了
    printf("模型输入大小: %dx%dx%d\n", yolo_model->model_width, yolo_model->model_height, yolo_model->model_channel);

    initialized_ = true;
    return true;
}

std::vector<DetectionResult> YOLOv8Inference::inference(const cv::Mat& image, float min_confidence) {
    std::vector<DetectionResult> results;
    if (!initialized_ || model_ == nullptr) {
        printf("模型未初始化\n");
        return results;
    }

    YOLOv8Model* yolo_model = static_cast<YOLOv8Model*>(model_);

    // 创建图像缓冲区
    image_buffer_t img_buffer;
    memset(&img_buffer, 0, sizeof(image_buffer_t));
    img_buffer.width = image.cols;
    img_buffer.height = image.rows;
    img_buffer.format = IMAGE_FORMAT_RGB888;
    img_buffer.virt_addr = image.data;
    img_buffer.size = image.cols * image.rows * 3;

    // 创建结果列表
    object_detect_result_list od_results;
    memset(&od_results, 0, sizeof(object_detect_result_list));

    // 调用RKNN推理
    int ret = inference_yolov8_model((rknn_app_context_t*)yolo_model, &img_buffer, &od_results);
    if (ret < 0) {
        printf("推理失败\n");
        return results;
    }

    // 将后处理结果转换为输出格式
    for (int i = 0; i < od_results.count; i++) {
        object_detect_result& det = od_results.results[i];
        
        // 检查置信度阈值
        if (det.prop < min_confidence) {
            continue;
        }
        
        DetectionResult result;
        result.class_id = det.cls_id;
        result.confidence = det.prop;
        
        // 设置边界框
        result.box.x = det.box.left;
        result.box.y = det.box.top;
        result.box.width = det.box.right - det.box.left;
        result.box.height = det.box.bottom - det.box.top;
        
        // 设置类别名称
        if (det.cls_id < labels_.size()) {
            result.class_name = labels_[det.cls_id];
        } else {
            char* name = coco_cls_to_name(det.cls_id);
            result.class_name = name ? name : "unknown";
        }
        
        results.push_back(result);
    }

    return results;
}

void YOLOv8Inference::drawResults(cv::Mat& image, const std::vector<DetectionResult>& results) {
    static const cv::Scalar colors[] = {
        cv::Scalar(255, 0, 0),     // 红色
        cv::Scalar(0, 255, 0),     // 绿色
        cv::Scalar(0, 0, 255),     // 蓝色
        cv::Scalar(255, 255, 0),   // 黄色
        cv::Scalar(0, 255, 255),   // 青色
        cv::Scalar(255, 0, 255),   // 品红色
        cv::Scalar(255, 127, 0),   // 橙色
        cv::Scalar(127, 0, 255),   // 紫色
        cv::Scalar(0, 127, 255),   // 天蓝色
        cv::Scalar(127, 255, 0),   // 黄绿色
    };

    for (size_t i = 0; i < results.size(); i++) {
        const DetectionResult& result = results[i];
        const cv::Scalar& color = colors[result.class_id % (sizeof(colors) / sizeof(colors[0]))];
        
        // 绘制矩形框
        cv::rectangle(image, cv::Point(result.box.x, result.box.y), 
                      cv::Point(result.box.x + result.box.width, result.box.y + result.box.height), 
                      color, 2);
        
        // 创建标签文本
        std::stringstream ss;
        ss << result.class_name << " " << std::fixed << std::setprecision(2) << result.confidence;
        std::string label = ss.str();
        
        // 绘制文本背景
        int baseline = 0;
        cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        cv::rectangle(image, 
                     cv::Point(result.box.x, result.box.y - text_size.height - 5),
                     cv::Point(result.box.x + text_size.width, result.box.y),
                     color, -1);
        
        // 绘制文本
        cv::putText(image, label, cv::Point(result.box.x, result.box.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    }
}

void YOLOv8Inference::setNMSThreshold(float nms_threshold) {
    nms_threshold_ = nms_threshold;
}

cv::Size YOLOv8Inference::getInputSize() const {
    if (!initialized_ || model_ == nullptr) {
        return cv::Size(0, 0);
    }
    
    YOLOv8Model* yolo_model = static_cast<YOLOv8Model*>(model_);
    return cv::Size(yolo_model->model_width, yolo_model->model_height);
}

void YOLOv8Inference::release() {
    if (model_ != nullptr) {
        YOLOv8Model* yolo_model = static_cast<YOLOv8Model*>(model_);
        release_yolov8_model((rknn_app_context_t*)yolo_model);
        delete yolo_model;
        model_ = nullptr;
    }
    initialized_ = false;
    labels_.clear();
}

} // namespace yolov8 