#include <iostream>
#include <thread>
#include <chrono>
#include "../include/usb_camera.h"

// 简单的回调函数
void test_callback(const CCameraData* camera_data, void* user_data) {
    std::cout << "Received data in mode: " << camera_data->mode << std::endl;
    
    switch (camera_data->mode) {
        case CAMERA_MODE_NO_CAMERA:
            std::cout << "No camera mode" << std::endl;
            break;
        case CAMERA_MODE_SINGLE_CAMERA:
            std::cout << "Single camera mode" << std::endl;
            break;
        case CAMERA_MODE_STEREO_CAMERA:
            std::cout << "Stereo camera mode" << std::endl;
            break;
        default:
            std::cout << "Unknown mode" << std::endl;
            break;
    }
}

int main() {
    std::cout << "Testing USB Camera Interface" << std::endl;
    
    // 1. 创建相机管理器实例
    CameraStreamHandle handle = camera_stream_create();
    if (!handle) {
        std::cerr << "Failed to create camera stream manager" << std::endl;
        return 1;
    }
    std::cout << "Camera stream manager created successfully" << std::endl;
    
    // 2. 注册回调函数
    CameraStreamError error = camera_stream_register_data_callback(handle, test_callback, nullptr);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "Failed to register callback: " << error << std::endl;
        camera_stream_destroy(handle);
        return 1;
    }
    std::cout << "Callback registered successfully" << std::endl;
    
    // 3. 启动相机流
    error = camera_stream_start(handle);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "Failed to start camera stream: " << error << std::endl;
        camera_stream_destroy(handle);
        return 1;
    }
    std::cout << "Camera stream started successfully" << std::endl;
    
    // 4. 检查运行状态
    int32_t is_running = camera_stream_is_running(handle);
    std::cout << "Camera stream is " << (is_running ? "running" : "not running") << std::endl;
    
    // 5. 获取当前模式
    int32_t mode = camera_stream_get_mode(handle);
    std::cout << "Current mode: " << mode << std::endl;
    
    // 6. 运行几秒钟
    std::cout << "Running for 5 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // 7. 停止相机流
    error = camera_stream_stop(handle);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "Failed to stop camera stream: " << error << std::endl;
    } else {
        std::cout << "Camera stream stopped successfully" << std::endl;
    }
    
    // 8. 销毁实例
    error = camera_stream_destroy(handle);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "Failed to destroy camera stream manager: " << error << std::endl;
        return 1;
    }
    std::cout << "Camera stream manager destroyed successfully" << std::endl;
    
    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}