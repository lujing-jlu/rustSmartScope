#include <iostream>
#include <opencv2/opencv.hpp>
#include "depth_anything_inference.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <image_path>" << std::endl;
        std::cout << "Example: " << argv[0] << " test_data/20250728_100708_左相机.jpg" << std::endl;
        return -1;
    }

    std::string image_path = argv[1];
    std::string model_path = "models/depth_anything_v2_vits.rknn";

    try {
        std::cout << "=== Depth Anything RKNN Inference ===" << std::endl;
        std::cout << "Loading image: " << image_path << std::endl;
        
        // 加载图像
        cv::Mat image = cv::imread(image_path);
        if (image.empty()) {
            std::cerr << "Failed to load image: " << image_path << std::endl;
            return -1;
        }

        std::cout << "Image size: " << image.size() << std::endl;

        // 创建RKNN推理引擎
        std::cout << "Creating RKNN inference engine..." << std::endl;
        auto engine = depth_anything::CreateRknnInferCore(model_path);
        
        // 创建深度估计模型
        std::cout << "Creating depth estimation model..." << std::endl;
        auto model = depth_anything::CreateDepthAnythingModel(engine, 518, 518);

        // 执行推理
        std::cout << "Running inference..." << std::endl;
        cv::Mat depth;
        bool success = model->ComputeDepth(image, depth);
        
        if (!success) {
            std::cerr << "Inference failed!" << std::endl;
            return -1;
        }

        std::cout << "Inference completed successfully!" << std::endl;
        std::cout << "Depth map size: " << depth.size() << std::endl;
        std::cout << "Depth map type: " << depth.type() << std::endl;

        // 获取深度图的范围
        double minVal, maxVal;
        cv::minMaxLoc(depth, &minVal, &maxVal);
        std::cout << "Depth range: " << minVal << " - " << maxVal << std::endl;

        // 归一化深度图到0-255范围并保存
        cv::Mat normalized_depth;
        if (maxVal > minVal) {
            depth.convertTo(normalized_depth, CV_8UC1, 255.0 / (maxVal - minVal),
                           -minVal * 255.0 / (maxVal - minVal));
        } else {
            // 如果深度值都相同，设置为128
            normalized_depth = cv::Mat(depth.size(), CV_8UC1, 128);
        }

        // 保存归一化后的深度图
        std::string output_path = "depth_result.png";
        cv::imwrite(output_path, normalized_depth);
        std::cout << "Normalized depth result saved to: " << output_path << std::endl;

        // 创建彩色可视化
        cv::Mat color_depth;
        cv::applyColorMap(normalized_depth, color_depth, cv::COLORMAP_JET);
        
        std::string color_output_path = "depth_result_color.png";
        cv::imwrite(color_output_path, color_depth);
        std::cout << "Color depth result saved to: " << color_output_path << std::endl;

        // 保存原始浮点深度图（用于调试）
        // 方法1：保存为16位TIFF
        cv::Mat depth_16bit;
        if (maxVal > minVal) {
            depth.convertTo(depth_16bit, CV_16UC1, 65535.0 / (maxVal - minVal),
                           -minVal * 65535.0 / (maxVal - minVal));
        } else {
            depth_16bit = cv::Mat(depth.size(), CV_16UC1, 32768);
        }
        
        std::string raw_output_path = "depth_result_raw.tiff";
        cv::imwrite(raw_output_path, depth_16bit);
        std::cout << "Raw depth result (16-bit) saved to: " << raw_output_path << std::endl;

        // 方法2：保存为32位浮点TIFF（保持原始精度）
        std::string float_output_path = "depth_result_float.tiff";
        cv::imwrite(float_output_path, depth);
        std::cout << "Float depth result saved to: " << float_output_path << std::endl;

        // 方法3：保存为EXR格式（更好的浮点支持）
        std::string exr_output_path = "depth_result.exr";
        cv::imwrite(exr_output_path, depth);
        std::cout << "EXR depth result saved to: " << exr_output_path << std::endl;

        std::cout << "All done!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
