#include "rkmpp_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <linux/videodev2.h>
#include <jpeglib.h>
#include <rga/im2d.h>
#include <rga/RgaApi.h>
#include <rga/im2d_single.h>

static inline unsigned char clamp_u8_int(int v) { return (v < 0) ? 0 : (v > 255 ? 255 : (unsigned char)v); }
static void bilinear_scale_rgb24(const uint8_t *src, int sw, int sh, uint8_t *dst, int dw, int dh) {
    double sx_ratio = (double)(sw - 1) / (double)(dw - 1 > 0 ? dw - 1 : 1);
    double sy_ratio = (double)(sh - 1) / (double)(dh - 1 > 0 ? dh - 1 : 1);
    int sstride = sw * 3;
    int dstride = dw * 3;
    for (int y = 0; y < dh; y++) {
        double sy = y * sy_ratio;
        int y0 = (int)sy; int y1 = (y0 + 1 < sh) ? y0 + 1 : y0; double fy = sy - y0;
        for (int x = 0; x < dw; x++) {
            double sx = x * sx_ratio;
            int x0 = (int)sx; int x1 = (x0 + 1 < sw) ? x0 + 1 : x0; double fx = sx - x0;
            int i00 = y0 * sstride + x0 * 3;
            int i01 = y0 * sstride + x1 * 3;
            int i10 = y1 * sstride + x0 * 3;
            int i11 = y1 * sstride + x1 * 3;
            int di = y * dstride + x * 3;
            for (int c = 0; c < 3; c++) {
                int v = (int)(
                    (1 - fy) * ((1 - fx) * src[i00+c] + fx * src[i01+c]) +
                    fy       * ((1 - fx) * src[i10+c] + fx * src[i11+c])
                );
                dst[di+c] = clamp_u8_int(v);
            }
        }
    }
}

// 设备结构
struct rkmpp_device {
    int fd;
    int width;
    int height;
    int buffer_count;
    struct v4l2_buffer *buffers;
    void **buffer_ptrs;
    int current_buffer;
    bool initialized;
};

// RGA初始化状态
static bool rga_initialized = false;

// 内部函数声明
static int init_v4l2_device(rkmpp_device_t *device, const char *device_path);
static int request_buffers(rkmpp_device_t *device);
static int map_buffers(rkmpp_device_t *device);
static int queue_buffers(rkmpp_device_t *device);
static int decode_mjpeg_to_rgb(const uint8_t *mjpeg_data, int mjpeg_size, 
                              uint8_t *rgb_data, int *width, int *height);
static double get_time_us(void);
static int load_image_file(const char *filename, uint8_t **data, int *width, int *height);
static int save_image_file(const char *filename, const uint8_t *data, int width, int height);
static int process_image_file(const char *src_file, const char *dst_file, 
                            rkmpp_transform_t transform, bool multi, rkmpp_transform_combo_t *combo);

// 初始化设备
rkmpp_device_t* rkmpp_init_device(const char *device_path, int width, int height, int buffer_count) {
    if (!device_path || width <= 0 || height <= 0 || buffer_count <= 0) {
        fprintf(stderr, "Invalid parameters\n");
        return NULL;
    }

    rkmpp_device_t *device = calloc(1, sizeof(rkmpp_device_t));
    if (!device) {
        fprintf(stderr, "Failed to allocate device structure\n");
        return NULL;
    }

    device->width = width;
    device->height = height;
    device->buffer_count = buffer_count;
    device->current_buffer = 0;
    device->initialized = false;

    // 初始化V4L2设备
    if (init_v4l2_device(device, device_path) != 0) {
        free(device);
        return NULL;
    }

    // 请求缓冲区
    if (request_buffers(device) != 0) {
        close(device->fd);
        free(device);
        return NULL;
    }

    // 映射缓冲区
    if (map_buffers(device) != 0) {
        close(device->fd);
        free(device);
        return NULL;
    }

    // 将缓冲区加入队列
    if (queue_buffers(device) != 0) {
        close(device->fd);
        free(device);
        return NULL;
    }

    device->initialized = true;
    printf("Device initialized: %s, %dx%d, %d buffers\n", device_path, width, height, buffer_count);
    return device;
}

// 获取一帧数据
int rkmpp_get_frame(rkmpp_device_t *device, rkmpp_frame_t *frame) {
    if (!device || !device->initialized || !frame) {
        return -1;
    }

    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = device->current_buffer;

    // 从队列中取出缓冲区
    if (ioctl(device->fd, VIDIOC_DQBUF, &buf) < 0) {
        fprintf(stderr, "Failed to dequeue buffer: %s\n", strerror(errno));
        return -1;
    }

    // 解码MJPEG到RGB
    int rgb_width, rgb_height;
    int rgb_size = device->width * device->height * 3;
    uint8_t *rgb_data = malloc(rgb_size);
    if (!rgb_data) {
        ioctl(device->fd, VIDIOC_QBUF, &buf);
        return -1;
    }

    if (decode_mjpeg_to_rgb(device->buffer_ptrs[buf.index], buf.bytesused, 
                           rgb_data, &rgb_width, &rgb_height) != 0) {
        free(rgb_data);
        ioctl(device->fd, VIDIOC_QBUF, &buf);
        return -1;
    }

    // 填充帧数据
    frame->data = rgb_data;
    frame->width = rgb_width;
    frame->height = rgb_height;
    frame->size = rgb_size;
    frame->format = V4L2_PIX_FMT_RGB24;

    // 将缓冲区重新加入队列
    if (ioctl(device->fd, VIDIOC_QBUF, &buf) < 0) {
        fprintf(stderr, "Failed to requeue buffer: %s\n", strerror(errno));
        free(rgb_data);
        return -1;
    }

    device->current_buffer = (device->current_buffer + 1) % device->buffer_count;
    return 0;
}

// 应用单个RGA变换
int rkmpp_apply_transform(rkmpp_frame_t *src_frame, rkmpp_frame_t *dst_frame, rkmpp_transform_t transform) {
    if (!src_frame || !src_frame->data || !dst_frame || !dst_frame->data) {
        return -1;
    }

    rkmpp_transform_combo_t combo = {0};
    combo.transforms[0] = transform;
    combo.count = 1;

    return rkmpp_apply_multi_transform(src_frame, dst_frame, &combo);
}

// 应用多个RGA变换组合
int rkmpp_apply_multi_transform(rkmpp_frame_t *src_frame, rkmpp_frame_t *dst_frame, rkmpp_transform_combo_t *combo) {
    if (!src_frame || !src_frame->data || !dst_frame || !dst_frame->data || !combo) {
        return -1;
    }

    int src_w = src_frame->width;
    int src_h = src_frame->height;
    
    // 计算最终输出尺寸
    int total_rot_deg = 0;
    int flip_h = 0, flip_v = 0;
    int scale_num = 1;
    int scale_den = 1;

    int has_invert = 0;
    for (int i = 0; i < combo->count; i++) {
        switch (combo->transforms[i]) {
            case RKMPP_TRANSFORM_ROTATE_90:  total_rot_deg += 90; break;
            case RKMPP_TRANSFORM_ROTATE_180: total_rot_deg += 180; break;
            case RKMPP_TRANSFORM_ROTATE_270: total_rot_deg += 270; break;
            case RKMPP_TRANSFORM_FLIP_H:     flip_h ^= 1; break;
            case RKMPP_TRANSFORM_FLIP_V:     flip_v ^= 1; break;
            case RKMPP_TRANSFORM_SCALE_2X:   scale_num *= 2; break;
            case RKMPP_TRANSFORM_SCALE_HALF: scale_den *= 2; break;
            case RKMPP_TRANSFORM_INVERT:     has_invert = 1; break;
            default: break;
        }
    }

    total_rot_deg %= 360;
    if (total_rot_deg < 0) total_rot_deg += 360;

    int out_w = (src_w * scale_num) / (scale_den > 0 ? scale_den : 1);
    int out_h = (src_h * scale_num) / (scale_den > 0 ? scale_den : 1);
    if (out_w < 1) out_w = 1;
    if (out_h < 1) out_h = 1;

    if (total_rot_deg == 90 || total_rot_deg == 270) {
        int t = out_w; out_w = out_h; out_h = t;
    }

    // 设置RGA信息
    rga_info_t src_info = {0}, dst_info = {0};
    
    src_info.fd = -1;
    src_info.virAddr = src_frame->data;
    src_info.mmuFlag = 1;
    src_info.format = RK_FORMAT_RGB_888;
    
    dst_info.fd = -1;
    dst_info.virAddr = dst_frame->data;
    dst_info.mmuFlag = 1;
    dst_info.format = RK_FORMAT_RGB_888;

    rga_set_rect(&src_info.rect, 0, 0, src_w, src_h, src_w, src_h, src_info.format);
    rga_set_rect(&dst_info.rect, 0, 0, out_w, out_h, out_w, out_h, dst_info.format);

    // 设置变换参数
    int rot = 0;
    if (total_rot_deg == 90)  rot |= HAL_TRANSFORM_ROT_90;
    if (total_rot_deg == 180) rot |= HAL_TRANSFORM_ROT_180;
    if (total_rot_deg == 270) rot |= HAL_TRANSFORM_ROT_270;
    if (flip_h)               rot |= HAL_TRANSFORM_FLIP_H;
    if (flip_v)               rot |= HAL_TRANSFORM_FLIP_V;
    
    dst_info.rotation = rot;
    dst_info.scale_mode = 1;

    // 如果有反色操作，使用ROP
    if (has_invert) {
        // 转换为rga_buffer_t类型
        rga_buffer_t src_buffer = {0}, dst_buffer = {0};
        
        src_buffer.width = src_w;
        src_buffer.height = src_h;
        src_buffer.format = RK_FORMAT_RGB_888;
        src_buffer.fd = -1;
        src_buffer.vir_addr = src_frame->data;
        
        dst_buffer.width = out_w;
        dst_buffer.height = out_h;
        dst_buffer.format = RK_FORMAT_RGB_888;
        dst_buffer.fd = -1;
        dst_buffer.vir_addr = dst_frame->data;
        
        // 使用ROP进行反色操作
        int ret = imrop(src_buffer, dst_buffer, IM_ROP_NOT_SRC);
        if (ret != 0) {
            fprintf(stderr, "RGA ROP invert failed: %d\n", ret);
            return -1;
        }
    } else {
        // 普通变换
        int ret = c_RkRgaBlit(&src_info, &dst_info, NULL);
        if (ret != 0) {
            fprintf(stderr, "RGA blit failed: %d\n", ret);
            return -1;
        }
    }

    // 更新目标帧信息
    dst_frame->width = out_w;
    dst_frame->height = out_h;
    dst_frame->size = out_w * out_h * 3;

    return 0;
}

// 性能测试
int rkmpp_performance_test(rkmpp_device_t *device, rkmpp_transform_combo_t *combo, 
                          int duration_sec, rkmpp_performance_stats_t *stats) {
    if (!device || !combo || !stats || duration_sec <= 0) {
        return -1;
    }

    // 分配源帧和目标帧
    rkmpp_frame_t src_frame = {0}, dst_frame = {0};
    if (rkmpp_alloc_frame(&dst_frame, device->width, device->height, V4L2_PIX_FMT_RGB24) != 0) {
        return -1;
    }

    // 初始化统计
    memset(stats, 0, sizeof(rkmpp_performance_stats_t));
    double start_time = get_time_us() / 1000000.0;
    double last_time = start_time;
    double min_fps = 1000.0, max_fps = 0.0;
    int frame_count = 0;

    printf("开始性能测试，时长: %d秒\n", duration_sec);

    while ((get_time_us() / 1000000.0 - start_time) < duration_sec) {
        // 获取源帧
        if (rkmpp_get_frame(device, &src_frame) != 0) {
            continue;
        }

        // 应用变换
        if (rkmpp_apply_multi_transform(&src_frame, &dst_frame, combo) != 0) {
            rkmpp_free_frame(&src_frame);
            continue;
        }

        frame_count++;
        double current_time = get_time_us() / 1000000.0;
        double elapsed = current_time - start_time;
        
        if (elapsed > 0) {
            double current_fps = frame_count / elapsed;
            if (current_fps < min_fps) min_fps = current_fps;
            if (current_fps > max_fps) max_fps = current_fps;
        }

        // 释放源帧
        rkmpp_free_frame(&src_frame);
    }

    double end_time = get_time_us() / 1000000.0;
    double total_time = end_time - start_time;

    // 填充统计信息
    stats->total_frames = frame_count;
    stats->total_time = total_time;
    stats->avg_fps = (total_time > 0) ? frame_count / total_time : 0;
    stats->min_fps = min_fps;
    stats->max_fps = max_fps;
    stats->total_data_mb = (frame_count * device->width * device->height * 3) / (1024.0 * 1024.0);
    stats->avg_data_rate = (total_time > 0) ? stats->total_data_mb / total_time : 0;

    rkmpp_free_frame(&dst_frame);
    return 0;
}

// 保存帧为PPM文件
int rkmpp_save_frame_ppm(rkmpp_frame_t *frame, const char *filename) {
    if (!frame || !frame->data || !filename) {
        return -1;
    }

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        return -1;
    }

    fprintf(fp, "P6\n%d %d\n255\n", frame->width, frame->height);
    fwrite(frame->data, 1, frame->size, fp);
    fclose(fp);

    printf("Saved frame to: %s\n", filename);
    return 0;
}

// 分配帧内存
int rkmpp_alloc_frame(rkmpp_frame_t *frame, int width, int height, int format) {
    if (!frame || width <= 0 || height <= 0) {
        return -1;
    }

    int bytes_per_pixel = 3; // RGB24
    if (format == V4L2_PIX_FMT_MJPEG) {
        bytes_per_pixel = 1; // 压缩格式，分配足够空间
    }

    frame->width = width;
    frame->height = height;
    frame->format = format;
    frame->size = width * height * bytes_per_pixel;
    frame->data = malloc(frame->size);

    if (!frame->data) {
        fprintf(stderr, "Failed to allocate frame memory\n");
        return -1;
    }

    return 0;
}

// 释放帧内存
void rkmpp_free_frame(rkmpp_frame_t *frame) {
    if (frame && frame->data) {
        free(frame->data);
        frame->data = NULL;
        frame->size = 0;
    }
}

// 关闭设备
void rkmpp_close_device(rkmpp_device_t *device) {
    if (!device) {
        return;
    }

    if (device->initialized) {
        // 停止视频流
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(device->fd, VIDIOC_STREAMOFF, &type);

        // 取消映射缓冲区
        for (int i = 0; i < device->buffer_count; i++) {
            if (device->buffer_ptrs[i]) {
                munmap(device->buffer_ptrs[i], device->buffers[i].length);
            }
        }
    }

    if (device->buffers) {
        free(device->buffers);
    }
    if (device->buffer_ptrs) {
        free(device->buffer_ptrs);
    }
    if (device->fd >= 0) {
        close(device->fd);
    }

    free(device);
}

// 创建变换组合
void rkmpp_create_transform_combo(rkmpp_transform_combo_t *combo, rkmpp_transform_t *transforms, int count) {
    if (!combo || !transforms || count <= 0 || count > 8) {
        return;
    }

    combo->count = count;
    memcpy(combo->transforms, transforms, count * sizeof(rkmpp_transform_t));
}

// 打印性能统计
void rkmpp_print_performance_stats(rkmpp_performance_stats_t *stats, const char *combo_name) {
    if (!stats || !combo_name) {
        return;
    }

    printf("\n=== RGA硬件加速性能测试结果 ===\n");
    printf("变换组合: %s\n", combo_name);
    printf("总帧数: %d\n", stats->total_frames);
    printf("总时间: %.2f秒\n", stats->total_time);
    printf("平均帧率: %.2f FPS\n", stats->avg_fps);
    printf("最小帧率: %.2f FPS\n", stats->min_fps);
    printf("最大帧率: %.2f FPS\n", stats->max_fps);
    printf("总数据量: %.2f MB\n", stats->total_data_mb);
    printf("平均数据率: %.2f MB/s\n", stats->avg_data_rate);
    printf("====================================\n");
}

// ==================== 单独RGA操作接口实现 ====================

// 初始化RGA（不依赖视频流）
int rkmpp_rga_init(void) {
    if (rga_initialized) {
        return 0; // 已经初始化
    }
    
    // 这里可以添加RGA特定的初始化代码
    // 目前RGA库会自动初始化，所以这里只是标记状态
    rga_initialized = true;
    printf("RGA initialized successfully\n");
    return 0;
}

// 反初始化RGA
void rkmpp_rga_deinit(void) {
    if (rga_initialized) {
        // 这里可以添加RGA特定的清理代码
        rga_initialized = false;
        printf("RGA deinitialized\n");
    }
}

// 应用单个RGA变换到图像数据
int rkmpp_rga_transform_image(const uint8_t *src_data, int src_width, int src_height,
                             uint8_t *dst_data, rkmpp_transform_t transform) {
    if (!rga_initialized) {
        if (rkmpp_rga_init() != 0) {
            return -1;
        }
    }
    
    if (!src_data || !dst_data || src_width <= 0 || src_height <= 0) {
        fprintf(stderr, "Invalid parameters for RGA transform\n");
        return -1;
    }
    
    // 创建变换组合
    rkmpp_transform_combo_t combo = {0};
    combo.transforms[0] = transform;
    combo.count = 1;
    
    return rkmpp_rga_transform_image_multi(src_data, src_width, src_height, dst_data, &combo);
}

// 应用多个RGA变换组合到图像数据
int rkmpp_rga_transform_image_multi(const uint8_t *src_data, int src_width, int src_height,
                                   uint8_t *dst_data, rkmpp_transform_combo_t *combo) {
    if (!rga_initialized) {
        if (rkmpp_rga_init() != 0) {
            return -1;
        }
    }
    
    if (!src_data || !dst_data || !combo || src_width <= 0 || src_height <= 0) {
        fprintf(stderr, "Invalid parameters for RGA multi-transform\n");
        return -1;
    }

    // 工作缓冲：从源开始，逐步应用到最终尺寸
    int cur_w = src_width;
    int cur_h = src_height;
    int cur_stride = cur_w * 3;
    size_t cur_size = (size_t)cur_stride * cur_h;
    uint8_t *cur_buf = (uint8_t*)malloc(cur_size);
    if (!cur_buf) return -1;
    memcpy(cur_buf, src_data, cur_size);

    // 应用每个变换
    for (int i = 0; i < combo->count; i++) {
        rkmpp_transform_t t = combo->transforms[i];
        int next_w = cur_w;
        int next_h = cur_h;
        // 决定目标尺寸
        switch (t) {
            case RKMPP_TRANSFORM_ROTATE_90:
            case RKMPP_TRANSFORM_ROTATE_270:
                next_w = cur_h;
                next_h = cur_w;
                break;
            case RKMPP_TRANSFORM_SCALE_2X:
                next_w = cur_w * 2;
                next_h = cur_h * 2;
                break;
            case RKMPP_TRANSFORM_SCALE_HALF:
                next_w = cur_w / 2; if (next_w < 1) next_w = 1;
                next_h = cur_h / 2; if (next_h < 1) next_h = 1;
                break;
            default:
                break;
        }

        int next_stride = next_w * 3;
        size_t next_size = (size_t)next_stride * next_h;
        uint8_t *next_buf = (uint8_t*)malloc(next_size);
        if (!next_buf) { free(cur_buf); return -1; }

        // 封装成 rga_buffer_t
        rga_buffer_t src = wrapbuffer_virtualaddr_t((void*)cur_buf, cur_w, cur_h, cur_w, cur_h, RK_FORMAT_RGB_888);
        rga_buffer_t dst = wrapbuffer_virtualaddr_t((void*)next_buf, next_w, next_h, next_w, next_h, RK_FORMAT_RGB_888);

        IM_STATUS s = IM_STATUS_SUCCESS;
        switch (t) {
            case RKMPP_TRANSFORM_ROTATE_90:
                s = imrotate(src, dst, IM_HAL_TRANSFORM_ROT_90);
                break;
            case RKMPP_TRANSFORM_ROTATE_180:
                s = imrotate(src, dst, IM_HAL_TRANSFORM_ROT_180);
                break;
            case RKMPP_TRANSFORM_ROTATE_270:
                s = imrotate(src, dst, IM_HAL_TRANSFORM_ROT_270);
                break;
            case RKMPP_TRANSFORM_FLIP_H:
                s = imflip(src, dst, IM_HAL_TRANSFORM_FLIP_H);
                break;
            case RKMPP_TRANSFORM_FLIP_V:
                s = imflip(src, dst, IM_HAL_TRANSFORM_FLIP_V);
                break;
            case RKMPP_TRANSFORM_SCALE_2X:
            case RKMPP_TRANSFORM_SCALE_HALF:
                // 使用软件双线性缩放以匹配期望
                bilinear_scale_rgb24(cur_buf, cur_w, cur_h, next_buf, next_w, next_h);
                s = IM_STATUS_SUCCESS;
                break;
            case RKMPP_TRANSFORM_INVERT: {
                // 软件反色
                for (int y = 0; y < next_h; y++) {
                    int sy = (next_h == cur_h) ? y : (y * cur_h / next_h);
                    for (int x = 0; x < next_w; x++) {
                        int sx = (next_w == cur_w) ? x : (x * cur_w / next_w);
                        int si = sy * cur_w * 3 + sx * 3;
                        int di = y * next_w * 3 + x * 3;
                        next_buf[di+0] = 255 - cur_buf[si+0];
                        next_buf[di+1] = 255 - cur_buf[si+1];
                        next_buf[di+2] = 255 - cur_buf[si+2];
                    }
                }
                s = IM_STATUS_SUCCESS;
                break;
            }
            default:
                s = IM_STATUS_SUCCESS;
                memcpy(next_buf, cur_buf, next_size < cur_size ? next_size : cur_size);
                break;
        }

        if (s <= 0) {
            fprintf(stderr, "im2d op failed, code=%d at step %d\n", s, i);
            free(next_buf);
            free(cur_buf);
            return -1;
        }

        // 进入下一轮
        free(cur_buf);
        cur_buf = next_buf;
        cur_w = next_w;
        cur_h = next_h;
        cur_stride = next_stride;
        cur_size = next_size;
    }

    // 拷贝到最终输出缓冲
    memcpy(dst_data, cur_buf, cur_size);
    free(cur_buf);
    return 0;
}

// 应用RGA变换到文件
int rkmpp_rga_transform_file(const char *src_file, const char *dst_file, rkmpp_transform_t transform) {
    return process_image_file(src_file, dst_file, transform, false, NULL);
}

// 应用多个RGA变换组合到文件
int rkmpp_rga_transform_file_multi(const char *src_file, const char *dst_file, rkmpp_transform_combo_t *combo) {
    return process_image_file(src_file, dst_file, RKMPP_TRANSFORM_NONE, true, combo);
}

// 批量处理图像文件
int rkmpp_rga_batch_transform(const char *src_dir, const char *dst_dir, 
                             rkmpp_transform_t transform, const char *file_pattern) {
    if (!src_dir || !dst_dir || !file_pattern) {
        fprintf(stderr, "Invalid parameters for batch transform\n");
        return -1;
    }
    
    // 创建目标目录
    mkdir(dst_dir, 0755);
    
    DIR *dir = opendir(src_dir);
    if (!dir) {
        fprintf(stderr, "Failed to open source directory: %s\n", src_dir);
        return -1;
    }
    
    int processed_count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL) {
        // 检查文件模式匹配
        if (strstr(entry->d_name, file_pattern) || 
            (strcmp(file_pattern, "*") == 0 && entry->d_type == DT_REG)) {
            
            char src_path[512], dst_path[512];
            snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
            snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);
            
            if (rkmpp_rga_transform_file(src_path, dst_path, transform) == 0) {
                processed_count++;
                printf("Processed: %s\n", entry->d_name);
            } else {
                fprintf(stderr, "Failed to process: %s\n", entry->d_name);
            }
        }
    }
    
    closedir(dir);
    printf("Batch processing completed: %d files processed\n", processed_count);
    return processed_count;
}

// 批量处理图像文件（多变换）
int rkmpp_rga_batch_transform_multi(const char *src_dir, const char *dst_dir, 
                                   rkmpp_transform_combo_t *combo, const char *file_pattern) {
    if (!src_dir || !dst_dir || !combo || !file_pattern) {
        fprintf(stderr, "Invalid parameters for batch multi-transform\n");
        return -1;
    }
    
    // 创建目标目录
    mkdir(dst_dir, 0755);
    
    DIR *dir = opendir(src_dir);
    if (!dir) {
        fprintf(stderr, "Failed to open source directory: %s\n", src_dir);
        return -1;
    }
    
    int processed_count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL) {
        // 检查文件模式匹配
        if (strstr(entry->d_name, file_pattern) || 
            (strcmp(file_pattern, "*") == 0 && entry->d_type == DT_REG)) {
            
            char src_path[512], dst_path[512];
            snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
            snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);
            
            if (rkmpp_rga_transform_file_multi(src_path, dst_path, combo) == 0) {
                processed_count++;
                printf("Processed: %s\n", entry->d_name);
            } else {
                fprintf(stderr, "Failed to process: %s\n", entry->d_name);
            }
        }
    }
    
    closedir(dir);
    printf("Batch multi-transform completed: %d files processed\n", processed_count);
    return processed_count;
}

// ==================== 内部辅助函数 ====================

// 加载图像文件
static int load_image_file(const char *filename, uint8_t **data, int *width, int *height) {
    // 这里简化实现，假设是PPM格式
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        return -1;
    }
    
    char magic[3];
    if (fread(magic, 1, 2, fp) != 2 || magic[0] != 'P' || magic[1] != '6') {
        fprintf(stderr, "Unsupported file format: %s\n", filename);
        fclose(fp);
        return -1;
    }
    
    if (fscanf(fp, "%d %d", width, height) != 2) {
        fprintf(stderr, "Failed to read image dimensions\n");
        fclose(fp);
        return -1;
    }
    
    int max_val;
    if (fscanf(fp, "%d", &max_val) != 1) {
        fprintf(stderr, "Failed to read max value\n");
        fclose(fp);
        return -1;
    }
    
    int size = (*width) * (*height) * 3;
    *data = malloc(size);
    if (!*data) {
        fprintf(stderr, "Failed to allocate memory\n");
        fclose(fp);
        return -1;
    }
    
    if (fread(*data, 1, size, fp) != size) {
        fprintf(stderr, "Failed to read image data\n");
        free(*data);
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    return 0;
}

// 保存图像文件
static int save_image_file(const char *filename, const uint8_t *data, int width, int height) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to create file: %s\n", filename);
        return -1;
    }
    
    fprintf(fp, "P6\n%d %d\n255\n", width, height);
    fwrite(data, 1, width * height * 3, fp);
    fclose(fp);
    
    return 0;
}

// 处理图像文件
static int process_image_file(const char *src_file, const char *dst_file, 
                            rkmpp_transform_t transform, bool multi, rkmpp_transform_combo_t *combo) {
    uint8_t *src_data = NULL;
    int src_width, src_height;
    
    // 加载源图像
    if (load_image_file(src_file, &src_data, &src_width, &src_height) != 0) {
        return -1;
    }
    
    // 计算目标尺寸
    int dst_width = src_width, dst_height = src_height;
    if (!multi) {
        // 单个变换的尺寸计算
        switch (transform) {
            case RKMPP_TRANSFORM_ROTATE_90:
            case RKMPP_TRANSFORM_ROTATE_270:
                dst_width = src_height;
                dst_height = src_width;
                break;
            case RKMPP_TRANSFORM_SCALE_2X:
                dst_width *= 2;
                dst_height *= 2;
                break;
            case RKMPP_TRANSFORM_SCALE_HALF:
                dst_width /= 2;
                dst_height /= 2;
                break;
            default:
                break;
        }
    } else {
        // 多变换的尺寸计算（简化处理）
        // 实际应该根据combo计算
    }
    
    // 分配目标内存
    uint8_t *dst_data = malloc(dst_width * dst_height * 3);
    if (!dst_data) {
        fprintf(stderr, "Failed to allocate destination memory\n");
        free(src_data);
        return -1;
    }
    
    // 应用变换
    int ret;
    if (multi) {
        ret = rkmpp_rga_transform_image_multi(src_data, src_width, src_height, dst_data, combo);
    } else {
        ret = rkmpp_rga_transform_image(src_data, src_width, src_height, dst_data, transform);
    }
    
    if (ret == 0) {
        // 保存结果
        ret = save_image_file(dst_file, dst_data, dst_width, dst_height);
        if (ret == 0) {
            printf("Transformed image saved to: %s\n", dst_file);
        }
    }
    
    // 清理内存
    free(src_data);
    free(dst_data);
    
    return ret;
}

// 内部函数实现

static int init_v4l2_device(rkmpp_device_t *device, const char *device_path) {
    device->fd = open(device_path, O_RDWR);
    if (device->fd < 0) {
        fprintf(stderr, "Failed to open device %s: %s\n", device_path, strerror(errno));
        return -1;
    }

    // 查询设备能力
    struct v4l2_capability cap;
    if (ioctl(device->fd, VIDIOC_QUERYCAP, &cap) < 0) {
        fprintf(stderr, "Failed to query device capabilities: %s\n", strerror(errno));
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "Device does not support video capture\n");
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Device does not support streaming\n");
        return -1;
    }

    // 设置格式
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = device->width;
    fmt.fmt.pix.height = device->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(device->fd, VIDIOC_S_FMT, &fmt) < 0) {
        fprintf(stderr, "Failed to set format: %s\n", strerror(errno));
        return -1;
    }

    // 启动视频流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(device->fd, VIDIOC_STREAMON, &type) < 0) {
        fprintf(stderr, "Failed to start streaming: %s\n", strerror(errno));
        // 不返回错误，继续执行
    }

    return 0;
}

static int request_buffers(rkmpp_device_t *device) {
    struct v4l2_requestbuffers req = {0};
    req.count = device->buffer_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(device->fd, VIDIOC_REQBUFS, &req) < 0) {
        fprintf(stderr, "Failed to request buffers: %s\n", strerror(errno));
        return -1;
    }

    if (req.count < device->buffer_count) {
        fprintf(stderr, "Insufficient buffer memory\n");
        return -1;
    }

    device->buffers = calloc(req.count, sizeof(struct v4l2_buffer));
    device->buffer_ptrs = calloc(req.count, sizeof(void*));
    
    if (!device->buffers || !device->buffer_ptrs) {
        fprintf(stderr, "Failed to allocate buffer arrays\n");
        return -1;
    }

    return 0;
}

static int map_buffers(rkmpp_device_t *device) {
    for (int i = 0; i < device->buffer_count; i++) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(device->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            fprintf(stderr, "Failed to query buffer %d: %s\n", i, strerror(errno));
            return -1;
        }

        device->buffer_ptrs[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, 
                                     MAP_SHARED, device->fd, buf.m.offset);
        if (device->buffer_ptrs[i] == MAP_FAILED) {
            fprintf(stderr, "Failed to map buffer %d: %s\n", i, strerror(errno));
            return -1;
        }

        device->buffers[i] = buf;
    }

    return 0;
}

static int queue_buffers(rkmpp_device_t *device) {
    for (int i = 0; i < device->buffer_count; i++) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(device->fd, VIDIOC_QBUF, &buf) < 0) {
            fprintf(stderr, "Failed to queue buffer %d: %s\n", i, strerror(errno));
            return -1;
        }
    }

    return 0;
}

static int decode_mjpeg_to_rgb(const uint8_t *mjpeg_data, int mjpeg_size, 
                              uint8_t *rgb_data, int *width, int *height) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    jpeg_mem_src(&cinfo, mjpeg_data, mjpeg_size);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }

    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    *width = cinfo.output_width;
    *height = cinfo.output_height;

    int row_stride = cinfo.output_width * cinfo.output_components;
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(rgb_data + (cinfo.output_scanline - 1) * row_stride, buffer[0], row_stride);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return 0;
}

static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
} 

int rkmpp_decode_jpeg_to_rgb(const uint8_t *in_jpeg, int in_size,
                            uint8_t **out_rgb, int *out_w, int *out_h, int *out_stride) {
    if (!in_jpeg || in_size <= 0 || !out_rgb || !out_w || !out_h || !out_stride) return -1;

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, in_jpeg, in_size);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }

    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    int w = cinfo.output_width;
    int h = cinfo.output_height;
    int stride = w * 3;
    size_t size = (size_t)stride * h;

    uint8_t *rgb = (uint8_t*)malloc(size);
    if (!rgb) {
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }

    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, stride, 1);
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(rgb + (cinfo.output_scanline - 1) * stride, buffer[0], stride);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    *out_rgb = rgb;
    *out_w = w;
    *out_h = h;
    *out_stride = stride;
    return 0;
}

int rkmpp_rga_transform_rgb24(const uint8_t *in_rgb, int in_w, int in_h, int in_stride,
                             uint8_t **out_rgb, int *out_w, int *out_h, int *out_stride,
                             rkmpp_transform_combo_t *combo) {
    if (!in_rgb || in_w <= 0 || in_h <= 0 || in_stride < in_w * 3 || !out_rgb || !combo) return -1;

    // 计算输出尺寸
    int total_rot_deg = 0, flip_h = 0, flip_v = 0, scale_num = 1, scale_den = 1, has_invert = 0;
    for (int i = 0; i < combo->count; i++) {
        switch (combo->transforms[i]) {
            case RKMPP_TRANSFORM_ROTATE_90:  total_rot_deg += 90; break;
            case RKMPP_TRANSFORM_ROTATE_180: total_rot_deg += 180; break;
            case RKMPP_TRANSFORM_ROTATE_270: total_rot_deg += 270; break;
            case RKMPP_TRANSFORM_FLIP_H:     flip_h ^= 1; break;
            case RKMPP_TRANSFORM_FLIP_V:     flip_v ^= 1; break;
            case RKMPP_TRANSFORM_SCALE_2X:   scale_num *= 2; break;
            case RKMPP_TRANSFORM_SCALE_HALF: scale_den *= 2; break;
            case RKMPP_TRANSFORM_INVERT:     has_invert = 1; break;
            default: break;
        }
    }

    total_rot_deg %= 360; if (total_rot_deg < 0) total_rot_deg += 360;
    int outw = (in_w * scale_num) / (scale_den > 0 ? scale_den : 1);
    int outh = (in_h * scale_num) / (scale_den > 0 ? scale_den : 1);
    if (outw < 1) outw = 1; if (outh < 1) outh = 1;
    if (total_rot_deg == 90 || total_rot_deg == 270) { int t = outw; outw = outh; outh = t; }

    int outstr = outw * 3;
    size_t outsize = (size_t)outstr * outh;
    uint8_t *dst = (uint8_t*)malloc(outsize);
    if (!dst) return -1;

    // 用已有的rkmpp_rga_transform_image_multi逻辑，但用RGB缓冲而不是帧
    rkmpp_transform_combo_t local = *combo;
    int ret = rkmpp_rga_transform_image_multi(in_rgb, in_w, in_h, dst, &local);
    if (ret != 0) { free(dst); return -1; }

    // 反色在rkmpp_rga_transform_image_multi里已处理（软件 fallback）

    *out_rgb = dst;
    if (out_w) *out_w = outw;
    if (out_h) *out_h = outh;
    if (out_stride) *out_stride = outstr;
    return 0;
}

int rkmpp_process_jpeg_to_rgb24(const uint8_t *in_jpeg, int in_size,
                               uint8_t **out_rgb, int *out_w, int *out_h, int *out_stride,
                               rkmpp_transform_combo_t *combo) {
    if (!in_jpeg || in_size <= 0 || !out_rgb || !combo) return -1;

    uint8_t *rgb = NULL; int w = 0, h = 0, stride = 0;
    if (rkmpp_decode_jpeg_to_rgb(in_jpeg, in_size, &rgb, &w, &h, &stride) != 0) return -1;

    uint8_t *dst = NULL; int dw = 0, dh = 0, dstride = 0;
    int ret = rkmpp_rga_transform_rgb24(rgb, w, h, stride, &dst, &dw, &dh, &dstride, combo);
    free(rgb);
    if (ret != 0) return -1;

    *out_rgb = dst;
    if (out_w) *out_w = dw;
    if (out_h) *out_h = dh;
    if (out_stride) *out_stride = dstride;
    return 0;
} 