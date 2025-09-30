#include <iostream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include "yolov8_detector.h"
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

// 检查文件是否存在
inline bool file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

int main(int argc, char** argv) {
    // 检查命令行参数
    if (argc < 2) {
        std::cerr << "用法: " << argv[0] << " <输入图像路径> [输出图像路径]" << std::endl;
        return -1;
    }

    std::string image_path = argv[1];
    std::string output_path = (argc > 2) ? argv[2] : "yolov8_output.jpg";
    
    // 获取当前工作目录
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        std::cerr << "获取当前工作目录失败" << std::endl;
        return -1;
    }
    std::string current_dir(cwd);
    std::cout << "当前工作目录: " << current_dir << std::endl;
    
    // 尝试多种可能的路径组合来查找模型和标签文件
    std::vector<std::string> possible_paths;
    // 当前目录
    possible_paths.push_back(current_dir);
    
    // 回退到项目根目录
    std::string project_root = current_dir;
    size_t pos = project_root.find("/build");
    if (pos != std::string::npos) {
        project_root = project_root.substr(0, pos);
        possible_paths.push_back(project_root);
    }
    
    // 回退到src/app/yolov8目录
    std::string yolov8_dir;
    pos = current_dir.find("/build");
    if (pos != std::string::npos) {
        yolov8_dir = current_dir.substr(0, pos);
        possible_paths.push_back(yolov8_dir);
    }
    
    // 回退到SmartScope根目录
    std::string smartscope_root;
    pos = current_dir.find("/src/app/yolov8");
    if (pos != std::string::npos) {
        smartscope_root = current_dir.substr(0, pos);
        possible_paths.push_back(smartscope_root);
    }
    
    // 查找模型和标签文件
    std::string model_path, label_path;
    bool model_found = false, label_found = false;
    
    for (const auto& base_path : possible_paths) {
        // 检查不同的相对路径组合
        std::vector<std::string> relative_paths = {
            "/models/yolov8m.rknn",
            "/models/coco_80_labels_list.txt",
            "/../models/yolov8m.rknn",
            "/../models/coco_80_labels_list.txt",
            "/../../models/yolov8m.rknn",
            "/../../models/coco_80_labels_list.txt"
        };
        
        for (size_t i = 0; i < relative_paths.size(); i += 2) {
            std::string potential_model = base_path + relative_paths[i];
            std::string potential_label = base_path + relative_paths[i+1];
            
            if (!model_found && file_exists(potential_model)) {
                model_path = potential_model;
                model_found = true;
            }
            
            if (!label_found && file_exists(potential_label)) {
                label_path = potential_label;
                label_found = true;
            }
            
            if (model_found && label_found) break;
        }
        
        if (model_found && label_found) break;
    }
    
    // 如果还没找到，尝试更具体的路径
    if (!model_found) {
        std::string specific_model_path = current_dir + "/../models/yolov8m.rknn";
        if (file_exists(specific_model_path)) {
            model_path = specific_model_path;
            model_found = true;
        }
    }
    
    if (!label_found) {
        std::string specific_label_path = current_dir + "/../models/coco_80_labels_list.txt";
        if (file_exists(specific_label_path)) {
            label_path = specific_label_path;
            label_found = true;
        }
    }
    
    // 如果仍然找不到，尝试直接从项目根目录查找
    if (!model_found || !label_found) {
        // 这是最后的备选项，使用绝对路径
        if (!model_found) {
            model_path = "/home/eddysun/App/Qt/SmartScope/src/app/yolov8/models/yolov8m.rknn";
        }
        if (!label_found) {
            label_path = "/home/eddysun/App/Qt/SmartScope/src/app/yolov8/models/coco_80_labels_list.txt";
        }
    }
    
    std::cout << "初始化YOLOv8检测器..." << std::endl;
    std::cout << "模型路径: " << model_path << std::endl;
    std::cout << "标签路径: " << label_path << std::endl;
    
    // 检查文件是否存在
    if (!file_exists(model_path)) {
        std::cerr << "模型文件不存在: " << model_path << std::endl;
        return -1;
    }
    
    if (!file_exists(label_path)) {
        std::cerr << "标签文件不存在: " << label_path << std::endl;
        return -1;
    }
    
    // 初始化YOLOv8检测器
    YOLOv8Detector detector;
    if (!detector.initialize(model_path, label_path)) {
        std::cerr << "初始化YOLOv8模型失败!" << std::endl;
        return -1;
    }
    
    std::cout << "读取输入图像: " << image_path << std::endl;
    
    // 读取输入图像
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "无法读取输入图像: " << image_path << std::endl;
        return -1;
    }
    
    std::cout << "执行目标检测..." << std::endl;
    
    // 执行目标检测
    std::vector<YOLOv8Detection> detections = detector.detect(image);
    
    // 输出检测结果
    std::cout << "检测到 " << detections.size() << " 个目标:" << std::endl;
    for (const auto& detection : detections) {
        std::cout << "类别: " << detection.class_name 
                  << ", 置信度: " << detection.confidence 
                  << ", 位置: [" << detection.box.x << ", " << detection.box.y 
                  << ", " << detection.box.width << ", " << detection.box.height << "]" << std::endl;
    }
    
    // 在图像上绘制检测结果
    detector.drawDetections(image, detections);
    
    // 保存输出图像
    cv::imwrite(output_path, image);
    std::cout << "结果已保存到: " << output_path << std::endl;
    
    // 释放资源
    detector.release();
    
    return 0;
} 