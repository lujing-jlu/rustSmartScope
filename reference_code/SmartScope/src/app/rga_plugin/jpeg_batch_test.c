#include "rkmpp_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int save_ppm(const char *path, const uint8_t *data, int w, int h, int stride) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    if (stride == w * 3) {
        fwrite(data, 1, (size_t)w * h * 3, fp);
    } else {
        // 行拷贝
        for (int y = 0; y < h; y++) {
            fwrite(data + y * stride, 1, (size_t)w * 3, fp);
        }
    }
    fclose(fp);
    return 0;
}

int main() {
    system("mkdir -p output");

    // 读取JPEG到内存
    const char *jpegPath = "test.jpg";
    FILE *jf = fopen(jpegPath, "rb");
    if (!jf) {
        fprintf(stderr, "无法打开 %s\n", jpegPath);
        return -1;
    }
    fseek(jf, 0, SEEK_END);
    long jsize = ftell(jf);
    fseek(jf, 0, SEEK_SET);
    uint8_t *jbuf = (uint8_t*)malloc(jsize);
    if (!jbuf) { fclose(jf); return -1; }
    if (fread(jbuf, 1, jsize, jf) != (size_t)jsize) { fclose(jf); free(jbuf); return -1; }
    fclose(jf);

    // 解码为RGB24
    uint8_t *rgb = NULL; int w = 0, h = 0, stride = 0;
    if (rkmpp_decode_jpeg_to_rgb(jbuf, (int)jsize, &rgb, &w, &h, &stride) != 0) {
        fprintf(stderr, "JPEG解码失败\n");
        free(jbuf);
        return -1;
    }
    save_ppm("output/test_rgb.ppm", rgb, w, h, stride);

    // 单变换：旋转90
    rkmpp_transform_t t1[] = { RKMPP_TRANSFORM_ROTATE_90 };
    rkmpp_transform_combo_t c1 = {}; rkmpp_create_transform_combo(&c1, t1, 1);
    uint8_t *o1 = NULL; int w1=0,h1=0,s1=0;
    if (rkmpp_rga_transform_rgb24(rgb, w, h, stride, &o1, &w1, &h1, &s1, &c1) == 0) {
        save_ppm("output/test_rot90.ppm", o1, w1, h1, s1);
        free(o1);
    }

    // 单变换：旋转180
    rkmpp_transform_t t2[] = { RKMPP_TRANSFORM_ROTATE_180 };
    rkmpp_transform_combo_t c2 = {}; rkmpp_create_transform_combo(&c2, t2, 1);
    uint8_t *o2 = NULL; int w2=0,h2=0,s2=0;
    if (rkmpp_rga_transform_rgb24(rgb, w, h, stride, &o2, &w2, &h2, &s2, &c2) == 0) {
        save_ppm("output/test_rot180.ppm", o2, w2, h2, s2);
        free(o2);
    }

    // 单变换：旋转270
    rkmpp_transform_t t3[] = { RKMPP_TRANSFORM_ROTATE_270 };
    rkmpp_transform_combo_t c3 = {}; rkmpp_create_transform_combo(&c3, t3, 1);
    uint8_t *o3 = NULL; int w3=0,h3=0,s3=0;
    if (rkmpp_rga_transform_rgb24(rgb, w, h, stride, &o3, &w3, &h3, &s3, &c3) == 0) {
        save_ppm("output/test_rot270.ppm", o3, w3, h3, s3);
        free(o3);
    }

    // 单变换：水平翻转
    rkmpp_transform_t t4[] = { RKMPP_TRANSFORM_FLIP_H };
    rkmpp_transform_combo_t c4 = {}; rkmpp_create_transform_combo(&c4, t4, 1);
    uint8_t *o4 = NULL; int w4=0,h4=0,s4=0;
    if (rkmpp_rga_transform_rgb24(rgb, w, h, stride, &o4, &w4, &h4, &s4, &c4) == 0) {
        save_ppm("output/test_flip_h.ppm", o4, w4, h4, s4);
        free(o4);
    }

    // 单变换：垂直翻转
    rkmpp_transform_t t5[] = { RKMPP_TRANSFORM_FLIP_V };
    rkmpp_transform_combo_t c5 = {}; rkmpp_create_transform_combo(&c5, t5, 1);
    uint8_t *o5 = NULL; int w5=0,h5=0,s5=0;
    if (rkmpp_rga_transform_rgb24(rgb, w, h, stride, &o5, &w5, &h5, &s5, &c5) == 0) {
        save_ppm("output/test_flip_v.ppm", o5, w5, h5, s5);
        free(o5);
    }

    // 单变换：2x放大
    rkmpp_transform_t t6[] = { RKMPP_TRANSFORM_SCALE_2X };
    rkmpp_transform_combo_t c6 = {}; rkmpp_create_transform_combo(&c6, t6, 1);
    uint8_t *o6 = NULL; int w6=0,h6=0,s6=0;
    if (rkmpp_rga_transform_rgb24(rgb, w, h, stride, &o6, &w6, &h6, &s6, &c6) == 0) {
        save_ppm("output/test_scale_2x.ppm", o6, w6, h6, s6);
        free(o6);
    }

    // 单变换：1/2缩小
    rkmpp_transform_t t7[] = { RKMPP_TRANSFORM_SCALE_HALF };
    rkmpp_transform_combo_t c7 = {}; rkmpp_create_transform_combo(&c7, t7, 1);
    uint8_t *o7 = NULL; int w7=0,h7=0,s7=0;
    if (rkmpp_rga_transform_rgb24(rgb, w, h, stride, &o7, &w7, &h7, &s7, &c7) == 0) {
        save_ppm("output/test_scale_half.ppm", o7, w7, h7, s7);
        free(o7);
    }

    // 单变换：反色
    rkmpp_transform_t t8[] = { RKMPP_TRANSFORM_INVERT };
    rkmpp_transform_combo_t c8 = {}; rkmpp_create_transform_combo(&c8, t8, 1);
    uint8_t *o8 = NULL; int w8=0,h8=0,s8=0;
    if (rkmpp_rga_transform_rgb24(rgb, w, h, stride, &o8, &w8, &h8, &s8, &c8) == 0) {
        save_ppm("output/test_invert.ppm", o8, w8, h8, s8);
        free(o8);
    }

    // 多变换：旋转90 + 水平翻转 + 半缩小
    rkmpp_transform_t tm[] = { RKMPP_TRANSFORM_ROTATE_90, RKMPP_TRANSFORM_FLIP_H, RKMPP_TRANSFORM_SCALE_HALF };
    rkmpp_transform_combo_t cm = {}; rkmpp_create_transform_combo(&cm, tm, 3);
    uint8_t *om = NULL; int wm=0,hm=0,sm=0;
    if (rkmpp_rga_transform_rgb24(rgb, w, h, stride, &om, &wm, &hm, &sm, &cm) == 0) {
        save_ppm("output/test_multi_rot90_flip_h_half.ppm", om, wm, hm, sm);
        free(om);
    }

    free(rgb);
    free(jbuf);
    printf("完成，结果已保存到 output/ 目录\n");
    return 0;
} 