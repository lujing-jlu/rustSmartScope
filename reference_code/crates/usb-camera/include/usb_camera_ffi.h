#ifndef USB_CAMERA_FFI_H
#define USB_CAMERA_FFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Error codes
typedef enum {
    USB_CAMERA_SUCCESS = 0,
    USB_CAMERA_ERROR_INVALID_HANDLE = -1,
    USB_CAMERA_ERROR_INIT_FAILED = -2,
    USB_CAMERA_ERROR_DEVICE_NOT_FOUND = -3,
    USB_CAMERA_ERROR_START_FAILED = -4,
    USB_CAMERA_ERROR_STOP_FAILED = -5,
    USB_CAMERA_ERROR_NO_FRAME = -6,
    USB_CAMERA_ERROR_INVALID_PARAM = -8
} UsbCameraError;

// Camera modes
typedef enum {
    USB_CAMERA_MODE_NO_CAMERA = 0,
    USB_CAMERA_MODE_SINGLE = 1,
    USB_CAMERA_MODE_STEREO = 2
} UsbCameraMode;

// Camera types
typedef enum {
    USB_CAMERA_TYPE_UNKNOWN = -1,
    USB_CAMERA_TYPE_LEFT = 0,
    USB_CAMERA_TYPE_RIGHT = 1,
    USB_CAMERA_TYPE_SINGLE = 2
} UsbCameraType;

// Image formats
typedef enum {
    USB_CAMERA_FORMAT_RGB888 = 0,
    USB_CAMERA_FORMAT_BGR888 = 1,
    USB_CAMERA_FORMAT_MJPG = 2,
    USB_CAMERA_FORMAT_YUYV = 3
} UsbCameraFormat;

// Camera status
typedef struct {
    UsbCameraMode mode;
    uint32_t camera_count;
    bool left_connected;
    bool right_connected;
    uint64_t timestamp;
} UsbCameraStatus;

// Frame metadata
typedef struct {
    const uint8_t* data;
    uint32_t size;
    uint32_t width;
    uint32_t height;
    UsbCameraFormat format;
    uint64_t frame_id;
    UsbCameraType camera_type;
    uint64_t timestamp;
} UsbCameraFrameData;

// Camera data types for unified callback
typedef struct {
    UsbCameraMode mode;
    uint64_t timestamp_ms;
    uint32_t detection_attempts;
    char error_message[256];
} UsbNoCameraData;

typedef struct {
    char name[64];
    char device_path[256];
    UsbCameraType camera_type;
    int32_t connected;
    float fps;
    uint64_t total_frames;
    uint64_t dropped_frames;
} UsbCameraInfo;

typedef struct {
    UsbCameraMode mode;
    uint64_t timestamp_ms;
    UsbCameraInfo camera_info;
    UsbCameraFrameData frame;
    float system_load;
} UsbSingleCameraData;

typedef struct {
    UsbCameraMode mode;
    uint64_t timestamp_ms;
    UsbCameraInfo left_camera_info;
    UsbCameraInfo right_camera_info;
    UsbCameraFrameData left_frame;
    UsbCameraFrameData right_frame;
    int32_t sync_delta_us;
    float baseline_mm;
    float system_load;
} UsbStereoCameraData;

// Unified camera data (union)
typedef union {
    UsbNoCameraData no_camera;
    UsbSingleCameraData single_camera;
    UsbStereoCameraData stereo_camera;
} UsbCameraDataUnion;

typedef struct {
    UsbCameraMode mode;
    UsbCameraDataUnion data;
} UsbCameraData;

// Callback function types
typedef void (*UsbCameraDataCallback)(const UsbCameraData* camera_data, void* user_data);

// Opaque handle type
typedef void* UsbCameraHandle;

// Core API functions
UsbCameraError usb_camera_init(void);
void usb_camera_cleanup(void);

UsbCameraHandle usb_camera_create_instance(void);
void usb_camera_destroy_instance(UsbCameraHandle handle);

UsbCameraError usb_camera_start(UsbCameraHandle handle);
UsbCameraError usb_camera_stop(UsbCameraHandle handle);

UsbCameraError usb_camera_register_data_callback(UsbCameraHandle handle, 
                                                  UsbCameraDataCallback callback, 
                                                  void* user_data);

UsbCameraError usb_camera_get_status(UsbCameraHandle handle, UsbCameraStatus* status);

// Frame reading API (optional, for direct access)
UsbCameraError usb_camera_get_left_frame(UsbCameraHandle handle, UsbCameraFrameData* frame);
UsbCameraError usb_camera_get_right_frame(UsbCameraHandle handle, UsbCameraFrameData* frame);

#ifdef __cplusplus
}
#endif

#endif // USB_CAMERA_FFI_H