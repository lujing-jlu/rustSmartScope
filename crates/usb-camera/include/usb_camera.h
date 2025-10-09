#ifndef USB_CAMERA_H
#define USB_CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/// Error codes for camera stream operations
typedef enum {
    CAMERA_STREAM_SUCCESS = 0,
    CAMERA_STREAM_INVALID_INSTANCE = -1,
    CAMERA_STREAM_INITIALIZATION_FAILED = -2,
    CAMERA_STREAM_DEVICE_NOT_FOUND = -3,
    CAMERA_STREAM_START_FAILED = -4,
    CAMERA_STREAM_STOP_FAILED = -5,
    CAMERA_STREAM_NO_FRAME_AVAILABLE = -6,
    CAMERA_STREAM_PIPE_WRITE_FAILED = -7,
    CAMERA_STREAM_INVALID_PARAMETER = -8
} CameraStreamError;

/// Camera modes
typedef enum {
    CAMERA_MODE_NO_CAMERA = 0,
    CAMERA_MODE_SINGLE_CAMERA = 1,
    CAMERA_MODE_STEREO_CAMERA = 2
} CameraMode;

/// Camera types for stereo mode
typedef enum {
    CAMERA_TYPE_UNKNOWN = -1,
    CAMERA_TYPE_LEFT = 0,
    CAMERA_TYPE_RIGHT = 1,
    CAMERA_TYPE_SINGLE = 2
} CameraType;

/// Frame status codes
typedef enum {
    FRAME_STATUS_OK = 0,
    FRAME_STATUS_DROPPED = 1,
    FRAME_STATUS_CORRUPTED = 2,
    FRAME_STATUS_TIMEOUT = 3
} FrameStatus;

/// Image format types (fourcc codes)
typedef enum {
    FORMAT_YUYV = 0x56595559,
    FORMAT_MJPG = 0x47504A4D,
    FORMAT_RGB24 = 0x32424752,
    FORMAT_UNKNOWN = 0
} ImageFormat;

/// Single frame metadata
typedef struct {
    /// Raw frame data pointer
    const uint8_t* data;
    /// Data size in bytes
    uint32_t size;
    /// Frame width in pixels
    uint32_t width;
    /// Frame height in pixels
    uint32_t height;
    /// Pixel format (ImageFormat enum)
    ImageFormat format;
    /// Frame ID for uniqueness tracking
    uint64_t frame_id;
    /// Camera type that captured this frame
    CameraType camera_type;
    /// Frame status
    FrameStatus status;
    /// Timestamp in milliseconds since epoch
    uint64_t timestamp_ms;
    /// Frame sequence number within current session
    uint64_t sequence_number;
    /// Exposure time in microseconds (if available)
    uint32_t exposure_us;
    /// Gain value (if available)
    uint32_t gain;
    /// Frame processing latency in microseconds
    uint32_t latency_us;
} CFrameMetadata;

/// Camera status information
typedef struct {
    /// Camera name/identifier
    char name[64];
    /// Device path
    char device_path[256];
    /// Camera type
    CameraType type;
    /// Is camera connected
    int connected;
    /// Current FPS
    float fps;
    /// Total frames captured
    uint64_t total_frames;
    /// Dropped frames count
    uint64_t dropped_frames;
    /// Camera temperature (if available)
    float temperature;
} CCameraStatus;

/// No camera data (empty payload)
typedef struct {
    /// Current mode
    CameraMode mode;
    /// Timestamp when no camera was detected
    uint64_t timestamp_ms;
    /// Detection attempt count
    uint32_t detection_attempts;
    /// Last error message
    char error_message[256];
} CNoCameraData;

/// Single camera data
typedef struct {
    /// Current mode
    CameraMode mode;
    /// Timestamp of data creation
    uint64_t timestamp_ms;
    /// Camera status
    CCameraStatus camera_status;
    /// Frame metadata
    CFrameMetadata frame;
    /// System load (CPU usage percentage)
    float system_load;
} CSingleCameraData;

/// Stereo camera data
typedef struct {
    /// Current mode
    CameraMode mode;
    /// Timestamp of data creation
    uint64_t timestamp_ms;
    /// Left camera status
    CCameraStatus left_camera_status;
    /// Right camera status
    CCameraStatus right_camera_status;
    /// Left frame metadata
    CFrameMetadata left_frame;
    /// Right frame metadata
    CFrameMetadata right_frame;
    /// Frame synchronization delta (microseconds)
    int32_t sync_delta_us;
    /// Stereo baseline distance (millimeters)
    float baseline_mm;
    /// System load (CPU usage percentage)
    float system_load;
} CStereoCameraData;

/// Unified camera data structure
typedef struct {
    /// Current camera mode
    CameraMode mode;
    /// Data variant based on mode
    union {
        CNoCameraData no_camera;
        CSingleCameraData single_camera;
        CStereoCameraData stereo_camera;
    } data;
} CCameraData;

/// Opaque handle for camera stream manager
typedef void* CameraStreamHandle;

/// Unified camera data callback function type
/// @param camera_data Pointer to unified camera data structure
/// @param user_data User-provided data pointer
typedef void (*CameraDataCallback)(const CCameraData* camera_data, void* user_data);



/// Create a new camera stream manager instance
/// @return Handle to the camera stream manager, or NULL on failure
CameraStreamHandle camera_stream_create(void);

/// Start the camera stream manager
/// @param handle Handle to the camera stream manager
/// @return Error code indicating success or failure
CameraStreamError camera_stream_start(CameraStreamHandle handle);

/// Stop the camera stream manager
/// @param handle Handle to the camera stream manager
/// @return Error code indicating success or failure
CameraStreamError camera_stream_stop(CameraStreamHandle handle);

/// Destroy the camera stream manager instance
/// @param handle Handle to the camera stream manager
/// @return Error code indicating success or failure
CameraStreamError camera_stream_destroy(CameraStreamHandle handle);

/// Get current camera mode
/// @param handle Handle to the camera stream manager
/// @return Camera mode, or -1 on error
int32_t camera_stream_get_mode(CameraStreamHandle handle);

/// Register unified camera data callback (recommended)
/// @param handle Handle to the camera stream manager
/// @param callback Camera data callback function
/// @param user_data User data to pass to callback
/// @return Error code indicating success or failure
CameraStreamError camera_stream_register_data_callback(
    CameraStreamHandle handle,
    CameraDataCallback callback,
    void* user_data
);







/// Force update camera mode detection (useful for testing)
/// @param handle Handle to the camera stream manager
/// @return Error code indicating success or failure
CameraStreamError camera_stream_update_mode(CameraStreamHandle handle);

/// Check if camera stream manager is running
/// @param handle Handle to the camera stream manager
/// @return 1 if running, 0 if not running or on error
int32_t camera_stream_is_running(CameraStreamHandle handle);

#ifdef __cplusplus
}
#endif

#endif // USB_CAMERA_H