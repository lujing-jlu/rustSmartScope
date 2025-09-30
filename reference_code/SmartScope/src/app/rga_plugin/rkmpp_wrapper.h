#ifndef RKMPP_WRAPPER_H
#define RKMPP_WRAPPER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 变换类型枚举
typedef enum {
    RKMPP_TRANSFORM_NONE = 0,
    RKMPP_TRANSFORM_ROTATE_90 = 1,
    RKMPP_TRANSFORM_ROTATE_180 = 2,
    RKMPP_TRANSFORM_ROTATE_270 = 3,
    RKMPP_TRANSFORM_FLIP_H = 4,
    RKMPP_TRANSFORM_FLIP_V = 5,
    RKMPP_TRANSFORM_SCALE_2X = 6,
    RKMPP_TRANSFORM_SCALE_HALF = 7,
    RKMPP_TRANSFORM_INVERT = 8
} rkmpp_transform_t;

// 变换组合结构
typedef struct {
    rkmpp_transform_t transforms[8];  // 最多8个变换
    int count;
} rkmpp_transform_combo_t;

// 帧数据结构
typedef struct {
    uint8_t *data;
    int width;
    int height;
    int stride;   // 每行字节数，RGB24为 width*3
    int size;     // data总字节数
    int format;   // V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_RGB24等
} rkmpp_frame_t;

// 性能统计结构
typedef struct {
    int total_frames;
    double total_time;
    double avg_fps;
    double min_fps;
    double max_fps;
    double total_data_mb;
    double avg_data_rate;
} rkmpp_performance_stats_t;

// 设备句柄
typedef struct rkmpp_device rkmpp_device_t;

// ==================== 视频流相关接口 ====================

// 初始化设备
// device_path: 设备路径，如"/dev/video1"
// width, height: 期望的分辨率
// buffer_count: 缓冲区数量
// 返回: 设备句柄，失败返回NULL
rkmpp_device_t* rkmpp_init_device(const char *device_path, int width, int height, int buffer_count);

// 获取一帧数据
// device: 设备句柄
// frame: 输出帧数据
// timeout_ms: 超时时间（毫秒）
// 返回: 0成功，-1失败
int rkmpp_get_frame(rkmpp_device_t *device, rkmpp_frame_t *frame);

// 关闭设备
// device: 设备句柄
void rkmpp_close_device(rkmpp_device_t *device);

// ==================== 单独RGA操作接口 ====================

// 初始化RGA（不依赖视频流）
// 返回: 0成功，-1失败
int rkmpp_rga_init(void);

// 反初始化RGA
void rkmpp_rga_deinit(void);

// 应用单个RGA变换到图像数据
// src_data: 源图像数据
// src_width, src_height: 源图像尺寸
// dst_data: 目标图像数据（需要预先分配足够内存）
// transform: 变换类型
// 返回: 0成功，-1失败
int rkmpp_rga_transform_image(const uint8_t *src_data, int src_width, int src_height,
                             uint8_t *dst_data, rkmpp_transform_t transform);

// 应用多个RGA变换组合到图像数据
// src_data: 源图像数据
// src_width, src_height: 源图像尺寸
// dst_data: 目标图像数据（需要预先分配足够内存）
// combo: 变换组合
// 返回: 0成功，-1失败
int rkmpp_rga_transform_image_multi(const uint8_t *src_data, int src_width, int src_height,
                                   uint8_t *dst_data, rkmpp_transform_combo_t *combo);

// 应用RGA变换到文件
// src_file: 源图像文件路径
// dst_file: 目标图像文件路径
// transform: 变换类型
// 返回: 0成功，-1失败
int rkmpp_rga_transform_file(const char *src_file, const char *dst_file, rkmpp_transform_t transform);

// 应用多个RGA变换组合到文件
// src_file: 源图像文件路径
// dst_file: 目标图像文件路径
// combo: 变换组合
// 返回: 0成功，-1失败
int rkmpp_rga_transform_file_multi(const char *src_file, const char *dst_file, rkmpp_transform_combo_t *combo);

// 批量处理图像文件
// src_dir: 源目录
// dst_dir: 目标目录
// transform: 变换类型
// file_pattern: 文件匹配模式（如"*.jpg"）
// 返回: 处理的文件数量，-1失败
int rkmpp_rga_batch_transform(const char *src_dir, const char *dst_dir, 
                             rkmpp_transform_t transform, const char *file_pattern);

// 批量处理图像文件（多变换）
// src_dir: 源目录
// dst_dir: 目标目录
// combo: 变换组合
// file_pattern: 文件匹配模式（如"*.jpg"）
// 返回: 处理的文件数量，-1失败
int rkmpp_rga_batch_transform_multi(const char *src_dir, const char *dst_dir, 
                                   rkmpp_transform_combo_t *combo, const char *file_pattern);

// ==================== 通用接口 ====================

// 应用单个RGA变换
// src_frame: 源帧
// dst_frame: 目标帧（需要预先分配内存）
// transform: 变换类型
// 返回: 0成功，-1失败
int rkmpp_apply_transform(rkmpp_frame_t *src_frame, rkmpp_frame_t *dst_frame, rkmpp_transform_t transform);

// 应用多个RGA变换组合（单次硬件调用）
// src_frame: 源帧
// dst_frame: 目标帧（需要预先分配内存）
// combo: 变换组合
// 返回: 0成功，-1失败
int rkmpp_apply_multi_transform(rkmpp_frame_t *src_frame, rkmpp_frame_t *dst_frame, rkmpp_transform_combo_t *combo);

// 性能测试
// device: 设备句柄
// combo: 变换组合
// duration_sec: 测试时长（秒）
// stats: 输出性能统计
// 返回: 0成功，-1失败
int rkmpp_performance_test(rkmpp_device_t *device, rkmpp_transform_combo_t *combo, int duration_sec, rkmpp_performance_stats_t *stats);

// 保存帧为PPM文件
// frame: 帧数据
// filename: 文件名
// 返回: 0成功，-1失败
int rkmpp_save_frame_ppm(rkmpp_frame_t *frame, const char *filename);

// 分配帧内存
// frame: 帧结构
// width, height: 尺寸
// format: 格式
// 返回: 0成功，-1失败
int rkmpp_alloc_frame(rkmpp_frame_t *frame, int width, int height, int format);

// 释放帧内存
// frame: 帧结构
void rkmpp_free_frame(rkmpp_frame_t *frame);

// 创建变换组合
// combo: 变换组合结构
// transforms: 变换数组
// count: 变换数量
void rkmpp_create_transform_combo(rkmpp_transform_combo_t *combo, rkmpp_transform_t *transforms, int count);

// 打印性能统计
// stats: 性能统计
// combo_name: 组合名称
void rkmpp_print_performance_stats(rkmpp_performance_stats_t *stats, const char *combo_name);

// ==================== JPEG 输入 / Qt 输出友好接口 ====================

// 将JPEG字节解码为RGB24缓冲（Qt友好：QImage::Format_RGB888）
// in_jpeg: JPEG数据指针
// in_size: JPEG数据长度
// out_rgb: 输出RGB缓冲区指针（由函数malloc分配，调用者负责free）
// out_w/out_h/out_stride: 输出尺寸与步长（stride=out_w*3）
// 返回0成功
int rkmpp_decode_jpeg_to_rgb(const uint8_t *in_jpeg, int in_size,
                            uint8_t **out_rgb, int *out_w, int *out_h, int *out_stride);

// 对RGB24缓冲应用RGA多变换（输入/输出均为Qt友好RGB24）
// in_rgb/in_w/in_h/in_stride
// out_rgb/out_w/out_h/out_stride（如传入非NULL尺寸，将据此分配/写入）
// combo: 变换组合
// 返回0成功
int rkmpp_rga_transform_rgb24(const uint8_t *in_rgb, int in_w, int in_h, int in_stride,
                             uint8_t **out_rgb, int *out_w, int *out_h, int *out_stride,
                             rkmpp_transform_combo_t *combo);

// 便捷：JPEG输入 -> RGA多变换 -> RGB24输出（直接给Qt）
int rkmpp_process_jpeg_to_rgb24(const uint8_t *in_jpeg, int in_size,
                               uint8_t **out_rgb, int *out_w, int *out_h, int *out_stride,
                               rkmpp_transform_combo_t *combo);

#ifdef __cplusplus
}
#endif

#endif // RKMPP_WRAPPER_H 