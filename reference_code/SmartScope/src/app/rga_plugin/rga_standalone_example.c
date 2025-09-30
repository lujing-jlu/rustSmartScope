#include "rkmpp_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("=== RGA独立操作示例 ===\n\n");

    // 1. 初始化RGA
    printf("1. 初始化RGA...\n");
    if (rkmpp_rga_init() != 0) {
        fprintf(stderr, "RGA初始化失败\n");
        return -1;
    }
    printf("RGA初始化成功\n\n");

    // 2. 创建测试图像数据
    printf("2. 创建测试图像数据...\n");
    int width = 640, height = 480;
    int image_size = width * height * 3;
    
    uint8_t *src_data = malloc(image_size);
    uint8_t *dst_data = malloc(image_size);
    
    if (!src_data || !dst_data) {
        fprintf(stderr, "内存分配失败\n");
        free(src_data);
        free(dst_data);
        rkmpp_rga_deinit();
        return -1;
    }
    
    // 创建简单的测试图像（渐变）
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            src_data[idx] = (x * 255) / width;     // R
            src_data[idx + 1] = (y * 255) / height; // G
            src_data[idx + 2] = 128;                // B
        }
    }
    printf("测试图像创建完成: %dx%d\n\n", width, height);

    // 3. 保存原始图像
    printf("3. 保存原始图像...\n");
    FILE *fp = fopen("output/test_original.ppm", "wb");
    if (fp) {
        fprintf(fp, "P6\n%d %d\n255\n", width, height);
        fwrite(src_data, 1, image_size, fp);
        fclose(fp);
        printf("原始图像已保存到 output/test_original.ppm\n");
    } else {
        fprintf(stderr, "保存原始图像失败\n");
    }
        fprintf(stderr, "保存原始图像失败\n");
    printf("\n");

    // 4. 应用单个变换 - 旋转90度
    printf("4. 应用单个变换 - 旋转90度...\n");
    if (rkmpp_rga_transform_image(src_data, width, height, dst_data, RKMPP_TRANSFORM_ROTATE_90) != 0) {
        fprintf(stderr, "旋转变换失败\n");
    } else {
        fp = fopen("output/test_rotated_90.ppm", "wb");
        if (fp) {
            fprintf(fp, "P6\n%d %d\n255\n", height, width);
            fwrite(dst_data, 1, image_size, fp);
            fclose(fp);
            printf("旋转90度图像已保存到 output/test_rotated_90.ppm\n");
        }
    }
    printf("\n");

    // 5. 应用单个变换 - 反色
    printf("5. 应用单个变换 - 反色...\n");
    if (rkmpp_rga_transform_image(src_data, width, height, dst_data, RKMPP_TRANSFORM_INVERT) != 0) {
        fprintf(stderr, "反色变换失败\n");
    } else {
        fp = fopen("output/test_inverted.ppm", "wb");
        if (fp) {
            fprintf(fp, "P6\n%d %d\n255\n", width, height);
            fwrite(dst_data, 1, image_size, fp);
            fclose(fp);
            printf("反色图像已保存到 output/test_inverted.ppm\n");
        }
    }
    printf("\n");

    // 6. 应用多个变换组合
    printf("6. 应用多个变换组合 - 水平翻转+垂直翻转...\n");
    rkmpp_transform_t transforms[] = {RKMPP_TRANSFORM_FLIP_H, RKMPP_TRANSFORM_FLIP_V};
    rkmpp_transform_combo_t combo = {0};
    rkmpp_create_transform_combo(&combo, transforms, 2);
    
    if (rkmpp_rga_transform_image_multi(src_data, width, height, dst_data, &combo) != 0) {
        fprintf(stderr, "多变换组合失败\n");
    } else {
        fp = fopen("output/test_flip_h_v.ppm", "wb");
        if (fp) {
            fprintf(fp, "P6\n%d %d\n255\n", width, height);
            fwrite(dst_data, 1, image_size, fp);
            fclose(fp);
            printf("多变换图像已保存到 output/test_flip_h_v.ppm\n");
        }
    }
    printf("\n");

    // 7. 文件变换测试
    printf("7. 文件变换测试...\n");
    if (rkmpp_rga_transform_file("output/test_original.ppm", "output/test_file_rotated_180.ppm", 
                                RKMPP_TRANSFORM_ROTATE_180) != 0) {
        fprintf(stderr, "文件变换失败\n");
    } else {
        printf("文件变换成功: test_file_rotated_180.ppm\n");
    }
    printf("\n");

    // 8. 批量处理测试（如果有多个文件）
    printf("8. 批量处理测试...\n");
    // 创建测试目录
    system("mkdir -p test_images");
    system("cp output/test_original.ppm test_images/");
    system("cp output/test_rotated_90.ppm test_images/");
    
    int processed = rkmpp_rga_batch_transform("test_images", "test_output", 
                                            RKMPP_TRANSFORM_SCALE_2X, "*.ppm");
    if (processed > 0) {
        printf("批量处理完成: %d个文件\n", processed);
    } else {
        printf("批量处理失败或无文件处理\n");
    }
    printf("\n");

    // 9. 清理资源
    printf("9. 清理资源...\n");
    free(src_data);
    free(dst_data);
    rkmpp_rga_deinit();
    printf("资源清理完成\n\n");

    printf("=== RGA独立操作示例完成 ===\n");
    printf("生成的文件:\n");
    printf("  - output/test_original.ppm (原始图像)\n");
    printf("  - output/test_rotated_90.ppm (旋转90度)\n");
    printf("  - output/test_inverted.ppm (反色)\n");
    printf("  - output/test_flip_h_v.ppm (水平+垂直翻转)\n");
    printf("  - output/test_file_rotated_180.ppm (文件变换)\n");
    printf("  - test_output/ (批量处理结果)\n");

    return 0;
} 