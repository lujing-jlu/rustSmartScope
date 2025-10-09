#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include "../include/usb_camera.h"

// Global counters for statistics
std::atomic<int> left_frame_count{0};
std::atomic<int> right_frame_count{0};
std::atomic<int> single_frame_count{0};

// User data structure for callbacks
struct CallbackData {
    const char* camera_name;
    std::atomic<int>* counter;
};

// Left camera callback function
void left_camera_callback(const CFrameData* frame_data, void* user_data) {
    CallbackData* data = static_cast<CallbackData*>(user_data);
    data->counter->fetch_add(1);
    
    // Log first few frames and then every 50th frame
    int count = data->counter->load();
    if (count <= 5 || count % 50 == 0) {
        std::cout << "[" << data->camera_name << "] Frame " << count 
                  << ": " << frame_data->width << "x" << frame_data->height
                  << ", " << (frame_data->size / 1024) << " KB"
                  << ", ID: " << frame_data->frame_id 
                  << ", Type: " << frame_data->camera_type << std::endl;
    }
}

// Right camera callback function  
void right_camera_callback(const CFrameData* frame_data, void* user_data) {
    CallbackData* data = static_cast<CallbackData*>(user_data);
    data->counter->fetch_add(1);
    
    // Log first few frames and then every 50th frame
    int count = data->counter->load();
    if (count <= 5 || count % 50 == 0) {
        std::cout << "[" << data->camera_name << "] Frame " << count 
                  << ": " << frame_data->width << "x" << frame_data->height
                  << ", " << (frame_data->size / 1024) << " KB"
                  << ", ID: " << frame_data->frame_id 
                  << ", Type: " << frame_data->camera_type << std::endl;
    }
}

// Single camera callback function
void single_camera_callback(const CFrameData* frame_data, void* user_data) {
    CallbackData* data = static_cast<CallbackData*>(user_data);
    data->counter->fetch_add(1);
    
    // Log first few frames and then every 50th frame
    int count = data->counter->load();
    if (count <= 5 || count % 50 == 0) {
        std::cout << "[" << data->camera_name << "] Frame " << count 
                  << ": " << frame_data->width << "x" << frame_data->height
                  << ", " << (frame_data->size / 1024) << " KB"
                  << ", ID: " << frame_data->frame_id 
                  << ", Type: " << frame_data->camera_type << std::endl;
    }
}

// Convert camera mode enum to string
const char* camera_mode_to_string(int mode) {
    switch (mode) {
        case CAMERA_MODE_NO_CAMERA: return "NoCamera";
        case CAMERA_MODE_SINGLE_CAMERA: return "SingleCamera";
        case CAMERA_MODE_STEREO_CAMERA: return "StereoCamera";
        default: return "Unknown";
    }
}

// Convert error code to string
const char* error_to_string(CameraStreamError error) {
    switch (error) {
        case CAMERA_STREAM_SUCCESS: return "Success";
        case CAMERA_STREAM_INVALID_INSTANCE: return "Invalid Instance";
        case CAMERA_STREAM_INITIALIZATION_FAILED: return "Initialization Failed";
        case CAMERA_STREAM_DEVICE_NOT_FOUND: return "Device Not Found";
        case CAMERA_STREAM_START_FAILED: return "Stream Start Failed";
        case CAMERA_STREAM_STOP_FAILED: return "Stream Stop Failed";
        case CAMERA_STREAM_NO_FRAME_AVAILABLE: return "No Frame Available";
        case CAMERA_STREAM_PIPE_WRITE_FAILED: return "Pipe Write Failed";
        case CAMERA_STREAM_INVALID_PARAMETER: return "Invalid Parameter";
        default: return "Unknown Error";
    }
}

int main() {
    std::cout << "🎥 USB Camera Callback Test (C++)" << std::endl;
    std::cout << "=================================" << std::endl;

    // Create callback data structures
    CallbackData left_data = {"LEFT_CAMERA", &left_frame_count};
    CallbackData right_data = {"RIGHT_CAMERA", &right_frame_count};
    CallbackData single_data = {"SINGLE_CAMERA", &single_frame_count};

    // Create camera stream manager
    CameraStreamHandle handle = camera_stream_create();
    if (!handle) {
        std::cerr << "❌ Failed to create camera stream manager" << std::endl;
        return 1;
    }

    std::cout << "✅ Camera stream manager created" << std::endl;

    // Register callbacks
    CameraStreamError error;

    error = camera_stream_register_left_callback(handle, left_camera_callback, &left_data);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "❌ Failed to register left callback: " << error_to_string(error) << std::endl;
        camera_stream_destroy(handle);
        return 1;
    }
    std::cout << "✅ Left camera callback registered" << std::endl;

    error = camera_stream_register_right_callback(handle, right_camera_callback, &right_data);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "❌ Failed to register right callback: " << error_to_string(error) << std::endl;
        camera_stream_destroy(handle);
        return 1;
    }
    std::cout << "✅ Right camera callback registered" << std::endl;

    error = camera_stream_register_single_callback(handle, single_camera_callback, &single_data);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "❌ Failed to register single callback: " << error_to_string(error) << std::endl;
        camera_stream_destroy(handle);
        return 1;
    }
    std::cout << "✅ Single camera callback registered" << std::endl;

    // Start camera stream manager
    error = camera_stream_start(handle);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "❌ Failed to start camera stream: " << error_to_string(error) << std::endl;
        camera_stream_destroy(handle);
        return 1;
    }

    std::cout << "🚀 Camera stream manager started" << std::endl;

    // Get initial camera mode
    int mode = camera_stream_get_mode(handle);
    std::cout << "📋 Initial camera mode: " << camera_mode_to_string(mode) << std::endl;

    // Run for 15 seconds with statistics
    const int duration_seconds = 15;
    auto start_time = std::chrono::steady_clock::now();
    int last_left_count = 0;
    int last_right_count = 0;
    int last_single_count = 0;

    std::cout << "\n📡 Receiving frames for " << duration_seconds << " seconds..." << std::endl;
    std::cout << "💡 Try plugging/unplugging cameras to see mode changes" << std::endl;

    while (true) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
        
        if (elapsed.count() >= duration_seconds) {
            break;
        }

        // Check for mode changes
        int current_mode = camera_stream_get_mode(handle);
        if (current_mode != mode) {
            std::cout << "🔄 Camera mode changed: " << camera_mode_to_string(mode) 
                      << " -> " << camera_mode_to_string(current_mode) << std::endl;
            mode = current_mode;
        }

        // Print statistics every 3 seconds
        if (elapsed.count() % 3 == 0 && elapsed.count() > 0) {
            int current_left = left_frame_count.load();
            int current_right = right_frame_count.load();
            int current_single = single_frame_count.load();

            if (current_left != last_left_count || current_right != last_right_count || 
                current_single != last_single_count) {
                
                std::cout << "\n📊 Frame Statistics (at " << elapsed.count() << "s):" << std::endl;
                
                if (current_left > 0) {
                    double left_fps = (current_left - last_left_count) / 3.0;
                    std::cout << "   📷 Left Camera: " << current_left << " frames (" 
                              << left_fps << " FPS)" << std::endl;
                }
                
                if (current_right > 0) {
                    double right_fps = (current_right - last_right_count) / 3.0;
                    std::cout << "   📷 Right Camera: " << current_right << " frames (" 
                              << right_fps << " FPS)" << std::endl;
                }
                
                if (current_single > 0) {
                    double single_fps = (current_single - last_single_count) / 3.0;
                    std::cout << "   📷 Single Camera: " << current_single << " frames (" 
                              << single_fps << " FPS)" << std::endl;
                }

                last_left_count = current_left;
                last_right_count = current_right;
                last_single_count = current_single;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Final statistics
    std::cout << "\n📊 Final Statistics:" << std::endl;
    std::cout << "   📷 Left Camera: " << left_frame_count.load() << " total frames" << std::endl;
    std::cout << "   📷 Right Camera: " << right_frame_count.load() << " total frames" << std::endl;
    std::cout << "   📷 Single Camera: " << single_frame_count.load() << " total frames" << std::endl;

    // Calculate average FPS
    if (left_frame_count.load() > 0) {
        double left_avg_fps = left_frame_count.load() / static_cast<double>(duration_seconds);
        std::cout << "   📈 Left Camera Average FPS: " << left_avg_fps << std::endl;
    }
    
    if (right_frame_count.load() > 0) {
        double right_avg_fps = right_frame_count.load() / static_cast<double>(duration_seconds);
        std::cout << "   📈 Right Camera Average FPS: " << right_avg_fps << std::endl;
    }
    
    if (single_frame_count.load() > 0) {
        double single_avg_fps = single_frame_count.load() / static_cast<double>(duration_seconds);
        std::cout << "   📈 Single Camera Average FPS: " << single_avg_fps << std::endl;
    }

    // Stop camera stream manager
    error = camera_stream_stop(handle);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "⚠️  Failed to stop camera stream: " << error_to_string(error) << std::endl;
    } else {
        std::cout << "\n🛑 Camera stream manager stopped" << std::endl;
    }

    // Destroy camera stream manager
    error = camera_stream_destroy(handle);
    if (error != CAMERA_STREAM_SUCCESS) {
        std::cerr << "⚠️  Failed to destroy camera stream: " << error_to_string(error) << std::endl;
    } else {
        std::cout << "🗑️  Camera stream manager destroyed" << std::endl;
    }

    std::cout << "\n🎉 Test completed!" << std::endl;
    return 0;
}