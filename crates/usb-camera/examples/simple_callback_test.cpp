#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include "../include/usb_camera.h"

// 简单的帧计数器
std::atomic<int> total_frames{0};

// 简单的回调函数
void frame_callback(const CFrameData* frame, void* user_data) {
    const char* camera_name = static_cast<const char*>(user_data);
    int count = total_frames.fetch_add(1) + 1;
    
    // 显示前几帧和每50帧的信息
    if (count <= 3 || count % 50 == 0) {
        std::cout << "[" << camera_name << "] 帧 " << count 
                  << ": " << frame->width << "x" << frame->height
                  << ", " << (frame->size / 1024) << " KB" << std::endl;
    }
}

int main() {
    std::cout << "🎥 USB相机简单回调测试" << std::endl;
    std::cout << "========================" << std::endl;

    // 1. 创建相机管理器
    CameraStreamHandle handle = camera_stream_create();
    if (!handle) {
        std::cerr << "❌ 创建相机管理器失败" << std::endl;
        return 1;
    }
    std::cout << "✅ 相机管理器已创建" << std::endl;

    // 2. 注册回调函数
    const char* left_name = "左相机";
    const char* right_name = "右相机";
    const char* single_name = "单相机";

    camera_stream_register_left_callback(handle, frame_callback, (void*)left_name);
    camera_stream_register_right_callback(handle, frame_callback, (void*)right_name);
    camera_stream_register_single_callback(handle, frame_callback, (void*)single_name);
    std::cout << "✅ 回调函数已注册" << std::endl;

    // 3. 启动相机流
    CameraStreamError error = camera_stream_start(handle);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "❌ 启动相机流失败: " << error << std::endl;
        camera_stream_destroy(handle);
        return 1;
    }
    std::cout << "🚀 相机流已启动" << std::endl;

    // 4. 获取相机模式
    int mode = camera_stream_get_mode(handle);
    const char* mode_str = (mode == 0) ? "无相机" : 
                          (mode == 1) ? "单相机" : 
                          (mode == 2) ? "立体相机" : "未知";
    std::cout << "📋 相机模式: " << mode_str << std::endl;

    // 5. 运行5秒并显示统计
    std::cout << "\n📡 运行5秒，接收帧数据..." << std::endl;
    auto start_time = std::chrono::steady_clock::now();
    
    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time);
        
        if (elapsed.count() >= 5) {
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 6. 显示最终统计
    int final_count = total_frames.load();
    double fps = final_count / 5.0;
    std::cout << "\n📊 最终统计:" << std::endl;
    std::cout << "   总帧数: " << final_count << std::endl;
    std::cout << "   平均FPS: " << fps << std::endl;

    // 7. 停止并清理
    camera_stream_stop(handle);
    camera_stream_destroy(handle);
    std::cout << "🛑 相机管理器已停止并清理" << std::endl;
    
    std::cout << "🎉 测试完成！" << std::endl;
    return 0;
}