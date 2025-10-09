#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include "../include/usb_camera.h"

// 帧计数器
std::atomic<int> no_camera_count{0};
std::atomic<int> single_camera_count{0};
std::atomic<int> stereo_camera_count{0};

// 模式转换为字符串
const char* mode_to_string(int mode) {
    switch (mode) {
        case CAMERA_MODE_NO_CAMERA: return "无相机";
        case CAMERA_MODE_SINGLE_CAMERA: return "单相机";
        case CAMERA_MODE_STEREO_CAMERA: return "立体相机";
        default: return "未知";
    }
}

// 格式转换为字符串
const char* format_to_string(ImageFormat format) {
    switch (format) {
        case FORMAT_YUYV: return "YUYV";
        case FORMAT_MJPG: return "MJPG";
        case FORMAT_RGB24: return "RGB24";
        default: return "Unknown";
    }
}

// 统一数据回调函数
void unified_data_callback(const CCameraData* camera_data, void* user_data) {
    const char* test_name = static_cast<const char*>(user_data);
    
    switch (camera_data->mode) {
        case CAMERA_MODE_NO_CAMERA: {
            int count = no_camera_count.fetch_add(1) + 1;
            const CNoCameraData& data = camera_data->data.no_camera;
            
            if (count <= 3 || count % 20 == 0) {
                std::cout << "[" << test_name << "] 无相机数据 #" << count 
                          << ": 检测尝试=" << data.detection_attempts
                          << ", 错误=" << data.error_message << std::endl;
            }
            break;
        }
        
        case CAMERA_MODE_SINGLE_CAMERA: {
            int count = single_camera_count.fetch_add(1) + 1;
            const CSingleCameraData& data = camera_data->data.single_camera;
            
            if (count <= 3 || count % 50 == 0) {
                std::cout << "[" << test_name << "] 单相机数据 #" << count 
                          << ": " << data.frame.width << "x" << data.frame.height
                          << ", " << (data.frame.size / 1024) << " KB"
                          << ", 格式=" << format_to_string(data.frame.format)
                          << ", 相机=" << data.camera_status.name
                          << ", FPS=" << data.camera_status.fps << std::endl;
            }
            break;
        }
        
        case CAMERA_MODE_STEREO_CAMERA: {
            int count = stereo_camera_count.fetch_add(1) + 1;
            const CStereoCameraData& data = camera_data->data.stereo_camera;
            
            if (count <= 3 || count % 50 == 0) {
                std::cout << "[" << test_name << "] 立体相机数据 #" << count << std::endl;
                std::cout << "  左相机: " << data.left_frame.width << "x" << data.left_frame.height
                          << ", " << (data.left_frame.size / 1024) << " KB, ID=" << data.left_frame.frame_id << std::endl;
                std::cout << "  右相机: " << data.right_frame.width << "x" << data.right_frame.height
                          << ", " << (data.right_frame.size / 1024) << " KB, ID=" << data.right_frame.frame_id << std::endl;
                std::cout << "  同步差: " << data.sync_delta_us << " μs"
                          << ", 基线: " << data.baseline_mm << " mm" << std::endl;
            }
            break;
        }
        
        default:
            std::cout << "[" << test_name << "] 未知模式: " << camera_data->mode << std::endl;
            break;
    }
}

int main() {
    std::cout << "🎥 USB相机统一数据格式测试" << std::endl;
    std::cout << "============================" << std::endl;

    // 1. 创建相机管理器
    CameraStreamHandle handle = camera_stream_create();
    if (!handle) {
        std::cerr << "❌ 创建相机管理器失败" << std::endl;
        return 1;
    }
    std::cout << "✅ 相机管理器已创建" << std::endl;

    // 2. 注册统一数据回调
    const char* test_name = "统一数据测试";
    CameraStreamError error = camera_stream_register_data_callback(handle, unified_data_callback, (void*)test_name);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "❌ 注册回调失败: " << error << std::endl;
        camera_stream_destroy(handle);
        return 1;
    }
    std::cout << "✅ 统一数据回调已注册" << std::endl;

    // 3. 启动相机流
    error = camera_stream_start(handle);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "❌ 启动相机流失败: " << error << std::endl;
        camera_stream_destroy(handle);
        return 1;
    }
    std::cout << "🚀 相机流已启动" << std::endl;

    // 4. 获取初始相机模式
    int mode = camera_stream_get_mode(handle);
    std::cout << "📋 初始相机模式: " << mode_to_string(mode) << std::endl;

    // 5. 运行8秒并显示统计
    std::cout << "\n📡 运行8秒，接收不同格式的数据..." << std::endl;
    std::cout << "💡 尝试插拔相机观察不同模式的数据格式" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    int last_mode = mode;
    
    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time);
        
        if (elapsed.count() >= 8) {
            break;
        }
        
        // 检查模式变化
        int current_mode = camera_stream_get_mode(handle);
        if (current_mode != last_mode) {
            std::cout << "🔄 模式变化: " << mode_to_string(last_mode) 
                      << " -> " << mode_to_string(current_mode) << std::endl;
            last_mode = current_mode;
        }
        
        // 每2秒显示统计
        if (elapsed.count() % 2 == 0 && elapsed.count() > 0) {
            std::cout << "\n📊 数据统计 (at " << elapsed.count() << "s):" << std::endl;
            if (no_camera_count.load() > 0) {
                std::cout << "   📵 无相机数据: " << no_camera_count.load() << " 次" << std::endl;
            }
            if (single_camera_count.load() > 0) {
                std::cout << "   📷 单相机数据: " << single_camera_count.load() << " 帧" << std::endl;
            }
            if (stereo_camera_count.load() > 0) {
                std::cout << "   📷📷 立体相机数据: " << stereo_camera_count.load() << " 帧对" << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 6. 显示最终统计
    std::cout << "\n📊 最终统计:" << std::endl;
    std::cout << "   📵 无相机数据: " << no_camera_count.load() << " 次" << std::endl;
    std::cout << "   📷 单相机数据: " << single_camera_count.load() << " 帧" << std::endl;
    std::cout << "   📷📷 立体相机数据: " << stereo_camera_count.load() << " 帧对" << std::endl;
    
    // 计算总体统计
    int total_data = no_camera_count.load() + single_camera_count.load() + stereo_camera_count.load();
    if (total_data > 0) {
        double data_rate = total_data / 8.0;
        std::cout << "   📈 数据接收率: " << data_rate << " 次/秒" << std::endl;
    }

    // 7. 停止并清理
    camera_stream_stop(handle);
    camera_stream_destroy(handle);
    std::cout << "🛑 相机管理器已停止并清理" << std::endl;
    
    std::cout << "🎉 统一数据格式测试完成！" << std::endl;
    return 0;
}