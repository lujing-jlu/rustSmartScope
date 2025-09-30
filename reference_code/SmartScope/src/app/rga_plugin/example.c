#include "rkmpp_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/videodev2.h>

int main() {
    printf("=== libv4l-rkmpp 包装库使用示例 ===\n\n");

    // 1. 初始化设备
    printf("1. 初始化视频设备...\n");
    rkmpp_device_t *device = rkmpp_init_device("/dev/video1", 1280, 720, 4);
    if (!device) {
        fprintf(stderr, "设备初始化失败\n");
        return -1;
    }
    printf("设备初始化成功\n\n");

    // 2. 获取一帧原始数据
    printf("2. 获取原始帧数据...\n");
    rkmpp_frame_t src_frame = {0};
    if (rkmpp_get_frame(device, &src_frame) != 0) {
        fprintf(stderr, "获取帧数据失败\n");
        rkmpp_close_device(device);
        return -1;
    }
    printf("获取帧数据成功: %dx%d\n\n", src_frame.width, src_frame.height);

    // 3. 保存原始帧
    printf("3. 保存原始帧...\n");
    if (rkmpp_save_frame_ppm(&src_frame, "output/original.ppm") != 0) {
        fprintf(stderr, "保存原始帧失败\n");
        rkmpp_free_frame(&src_frame);
        rkmpp_close_device(device);
        return -1;
    }
    printf("原始帧已保存到 output/original.ppm\n\n");

    // 4. 应用单个变换 - 旋转90度
    printf("4. 应用单个变换 - 旋转90度...\n");
    rkmpp_frame_t dst_frame = {0};
    if (rkmpp_alloc_frame(&dst_frame, src_frame.width, src_frame.height, V4L2_PIX_FMT_RGB24) != 0) {
        fprintf(stderr, "分配目标帧内存失败\n");
        rkmpp_free_frame(&src_frame);
        rkmpp_close_device(device);
        return -1;
    }

    if (rkmpp_apply_transform(&src_frame, &dst_frame, RKMPP_TRANSFORM_ROTATE_90) != 0) {
        fprintf(stderr, "应用旋转变换失败\n");
        rkmpp_free_frame(&dst_frame);
        rkmpp_free_frame(&src_frame);
        rkmpp_close_device(device);
        return -1;
    }
    printf("旋转90度变换完成: %dx%d\n", dst_frame.width, dst_frame.height);

    if (rkmpp_save_frame_ppm(&dst_frame, "output/rotated_90.ppm") != 0) {
        fprintf(stderr, "保存旋转帧失败\n");
    } else {
        printf("旋转帧已保存到 output/rotated_90.ppm\n");
    }
    printf("\n");

    // 5. 应用多个变换组合 - 水平翻转+垂直翻转
    printf("5. 应用多个变换组合 - 水平翻转+垂直翻转...\n");
    rkmpp_transform_t transforms[] = {RKMPP_TRANSFORM_FLIP_H, RKMPP_TRANSFORM_FLIP_V};
    rkmpp_transform_combo_t combo = {0};
    rkmpp_create_transform_combo(&combo, transforms, 2);

    rkmpp_frame_t multi_dst_frame = {0};
    if (rkmpp_alloc_frame(&multi_dst_frame, src_frame.width, src_frame.height, V4L2_PIX_FMT_RGB24) != 0) {
        fprintf(stderr, "分配多变换目标帧内存失败\n");
        rkmpp_free_frame(&dst_frame);
        rkmpp_free_frame(&src_frame);
        rkmpp_close_device(device);
        return -1;
    }

    if (rkmpp_apply_multi_transform(&src_frame, &multi_dst_frame, &combo) != 0) {
        fprintf(stderr, "应用多变换组合失败\n");
        rkmpp_free_frame(&multi_dst_frame);
        rkmpp_free_frame(&dst_frame);
        rkmpp_free_frame(&src_frame);
        rkmpp_close_device(device);
        return -1;
    }
    printf("多变换组合完成: %dx%d\n", multi_dst_frame.width, multi_dst_frame.height);

    if (rkmpp_save_frame_ppm(&multi_dst_frame, "output/flip_h_v.ppm") != 0) {
        fprintf(stderr, "保存多变换帧失败\n");
    } else {
        printf("多变换帧已保存到 output/flip_h_v.ppm\n");
    }
    printf("\n");

    // 6. 应用反色变换
    printf("6. 应用反色变换...\n");
    rkmpp_frame_t invert_frame = {0};
    if (rkmpp_alloc_frame(&invert_frame, src_frame.width, src_frame.height, V4L2_PIX_FMT_RGB24) != 0) {
        fprintf(stderr, "分配反色帧内存失败\n");
        rkmpp_free_frame(&multi_dst_frame);
        rkmpp_free_frame(&dst_frame);
        rkmpp_free_frame(&src_frame);
        rkmpp_close_device(device);
        return -1;
    }

    if (rkmpp_apply_transform(&src_frame, &invert_frame, RKMPP_TRANSFORM_INVERT) != 0) {
        fprintf(stderr, "应用反色变换失败\n");
        rkmpp_free_frame(&invert_frame);
        rkmpp_free_frame(&multi_dst_frame);
        rkmpp_free_frame(&dst_frame);
        rkmpp_free_frame(&src_frame);
        rkmpp_close_device(device);
        return -1;
    }
    printf("反色变换完成: %dx%d\n", invert_frame.width, invert_frame.height);

    if (rkmpp_save_frame_ppm(&invert_frame, "output/inverted.ppm") != 0) {
        fprintf(stderr, "保存反色帧失败\n");
    } else {
        printf("反色帧已保存到 output/inverted.ppm\n");
    }
    printf("\n");

    // 7. 性能测试
    printf("7. 性能测试 - 旋转90度+水平翻转+缩小一半...\n");
    rkmpp_transform_t perf_transforms[] = {
        RKMPP_TRANSFORM_ROTATE_90, 
        RKMPP_TRANSFORM_FLIP_H, 
        RKMPP_TRANSFORM_SCALE_HALF
    };
    rkmpp_transform_combo_t perf_combo = {0};
    rkmpp_create_transform_combo(&perf_combo, perf_transforms, 3);

    rkmpp_performance_stats_t stats = {0};
    if (rkmpp_performance_test(device, &perf_combo, 3, &stats) != 0) {
        fprintf(stderr, "性能测试失败\n");
    } else {
        rkmpp_print_performance_stats(&stats, "旋转90度+水平翻转+缩小一半");
    }
    printf("\n");

    // 8. 清理资源
    printf("8. 清理资源...\n");
    rkmpp_free_frame(&invert_frame);
    rkmpp_free_frame(&multi_dst_frame);
    rkmpp_free_frame(&dst_frame);
    rkmpp_free_frame(&src_frame);
    rkmpp_close_device(device);
    printf("资源清理完成\n\n");

    printf("=== 示例运行完成 ===\n");
    printf("生成的文件:\n");
    printf("  - output/original.ppm (原始帧)\n");
    printf("  - output/rotated_90.ppm (旋转90度)\n");
    printf("  - output/flip_h_v.ppm (水平+垂直翻转)\n");
    printf("  - output/inverted.ppm (反色)\n");

    return 0;
} 