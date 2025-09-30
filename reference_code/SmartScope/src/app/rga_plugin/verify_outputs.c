#include "rkmpp_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned char *data; int w, h, stride; } image_t;

static int read_ppm(const char *path, image_t *img) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    char magic[3] = {0};
    if (fread(magic, 1, 2, fp) != 2 || magic[0] != 'P' || magic[1] != '6') { fclose(fp); return -1; }
    int w = 0, h = 0, maxval = 0;
    if (fscanf(fp, "%d %d", &w, &h) != 2) { fclose(fp); return -1; }
    if (fscanf(fp, "%d", &maxval) != 1) { fclose(fp); return -1; }
    fgetc(fp); // consume single whitespace after header
    size_t stride = (size_t)w * 3;
    size_t size = stride * h;
    unsigned char *buf = (unsigned char*)malloc(size);
    if (!buf) { fclose(fp); return -1; }
    if (fread(buf, 1, size, fp) != size) { free(buf); fclose(fp); return -1; }
    fclose(fp);
    img->data = buf; img->w = w; img->h = h; img->stride = (int)stride;
    return 0;
}

static void free_image(image_t *img) { if (img && img->data) { free(img->data); img->data = NULL; } }

static void make_rot90(const image_t *src, image_t *dst) {
    dst->w = src->h; dst->h = src->w; dst->stride = dst->w * 3;
    dst->data = (unsigned char*)malloc((size_t)dst->stride * dst->h);
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            int si = y * src->stride + x * 3;
            int nx = src->h - 1 - y;
            int ny = x;
            int di = ny * dst->stride + nx * 3;
            dst->data[di+0] = src->data[si+0];
            dst->data[di+1] = src->data[si+1];
            dst->data[di+2] = src->data[si+2];
        }
    }
}

static void make_rot180(const image_t *src, image_t *dst) {
    dst->w = src->w; dst->h = src->h; dst->stride = dst->w * 3;
    dst->data = (unsigned char*)malloc((size_t)dst->stride * dst->h);
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            int si = y * src->stride + x * 3;
            int nx = src->w - 1 - x;
            int ny = src->h - 1 - y;
            int di = ny * dst->stride + nx * 3;
            dst->data[di+0] = src->data[si+0];
            dst->data[di+1] = src->data[si+1];
            dst->data[di+2] = src->data[si+2];
        }
    }
}

static void make_rot270(const image_t *src, image_t *dst) {
    dst->w = src->h; dst->h = src->w; dst->stride = dst->w * 3;
    dst->data = (unsigned char*)malloc((size_t)dst->stride * dst->h);
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            int si = y * src->stride + x * 3;
            int nx = y;
            int ny = src->w - 1 - x;
            int di = ny * dst->stride + nx * 3;
            dst->data[di+0] = src->data[si+0];
            dst->data[di+1] = src->data[si+1];
            dst->data[di+2] = src->data[si+2];
        }
    }
}

static void make_flip_h(const image_t *src, image_t *dst) {
    dst->w = src->w; dst->h = src->h; dst->stride = dst->w * 3;
    dst->data = (unsigned char*)malloc((size_t)dst->stride * dst->h);
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            int si = y * src->stride + x * 3;
            int nx = src->w - 1 - x;
            int di = y * dst->stride + nx * 3;
            dst->data[di+0] = src->data[si+0];
            dst->data[di+1] = src->data[si+1];
            dst->data[di+2] = src->data[si+2];
        }
    }
}

static void make_flip_v(const image_t *src, image_t *dst) {
    dst->w = src->w; dst->h = src->h; dst->stride = dst->w * 3;
    dst->data = (unsigned char*)malloc((size_t)dst->stride * dst->h);
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            int si = y * src->stride + x * 3;
            int ny = src->h - 1 - y;
            int di = ny * dst->stride + x * 3;
            dst->data[di+0] = src->data[si+0];
            dst->data[di+1] = src->data[si+1];
            dst->data[di+2] = src->data[si+2];
        }
    }
}

static inline unsigned char clamp_u8(int v){ return (v<0)?0:((v>255)?255:(unsigned char)v); }

static void make_scale_bilinear(const image_t *src, image_t *dst, int out_w, int out_h) {
    dst->w = out_w; dst->h = out_h; dst->stride = out_w * 3;
    dst->data = (unsigned char*)malloc((size_t)dst->stride * out_h);
    double sx_ratio = (double)(src->w - 1) / (double)(out_w - 1 > 0 ? out_w - 1 : 1);
    double sy_ratio = (double)(src->h - 1) / (double)(out_h - 1 > 0 ? out_h - 1 : 1);
    for (int y = 0; y < out_h; y++) {
        double sy = y * sy_ratio;
        int y0 = (int)sy; int y1 = (y0 + 1 < src->h) ? y0 + 1 : y0; double fy = sy - y0;
        for (int x = 0; x < out_w; x++) {
            double sx = x * sx_ratio;
            int x0 = (int)sx; int x1 = (x0 + 1 < src->w) ? x0 + 1 : x0; double fx = sx - x0;
            int i00 = y0 * src->stride + x0 * 3;
            int i01 = y0 * src->stride + x1 * 3;
            int i10 = y1 * src->stride + x0 * 3;
            int i11 = y1 * src->stride + x1 * 3;
            int di = y * dst->stride + x * 3;
            for (int c = 0; c < 3; c++) {
                int v = (int)(
                    (1 - fy) * ((1 - fx) * src->data[i00+c] + fx * src->data[i01+c]) +
                    fy       * ((1 - fx) * src->data[i10+c] + fx * src->data[i11+c])
                );
                dst->data[di+c] = clamp_u8(v);
            }
        }
    }
}

static void make_scale_half(const image_t *src, image_t *dst) {
    int out_w = src->w / 2; if (out_w < 1) out_w = 1;
    int out_h = src->h / 2; if (out_h < 1) out_h = 1;
    make_scale_bilinear(src, dst, out_w, out_h);
}

static void make_scale_2x(const image_t *src, image_t *dst) {
    make_scale_bilinear(src, dst, src->w * 2, src->h * 2);
}

static void make_invert(const image_t *src, image_t *dst) {
    dst->w = src->w; dst->h = src->h; dst->stride = dst->w * 3;
    dst->data = (unsigned char*)malloc((size_t)dst->stride * dst->h);
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            int si = y * src->stride + x * 3;
            int di = y * dst->stride + x * 3;
            dst->data[di+0] = 255 - src->data[si+0];
            dst->data[di+1] = 255 - src->data[si+1];
            dst->data[di+2] = 255 - src->data[si+2];
        }
    }
}

static double diff_ratio(const image_t *a, const image_t *b) {
    if (a->w != b->w || a->h != b->h) return 1.0;
    long long diff = 0, total = (long long)a->w * a->h * 3;
    for (int y = 0; y < a->h; y++) {
        const unsigned char *pa = a->data + y * a->stride;
        const unsigned char *pb = b->data + y * b->stride;
        for (int i = 0; i < a->w * 3; i++) {
            diff += (pa[i] != pb[i]);
        }
    }
    return (double)diff / (double)total;
}

static int load_jpeg_as_image(const char *jpegPath, image_t *img) {
    FILE *jf = fopen(jpegPath, "rb"); if (!jf) return -1;
    fseek(jf, 0, SEEK_END); long jsize = ftell(jf); fseek(jf, 0, SEEK_SET);
    unsigned char *jbuf = (unsigned char*)malloc(jsize); if (!jbuf) { fclose(jf); return -1; }
    if (fread(jbuf, 1, jsize, jf) != (size_t)jsize) { free(jbuf); fclose(jf); return -1; }
    fclose(jf);
    uint8_t *rgb = NULL; int w=0,h=0,stride=0;
    if (rkmpp_decode_jpeg_to_rgb(jbuf, (int)jsize, &rgb, &w, &h, &stride) != 0) { free(jbuf); return -1; }
    free(jbuf);
    img->data = rgb; img->w = w; img->h = h; img->stride = stride;
    return 0;
}

static void report_case(const char *name, const image_t *expected, const char *out_path) {
    image_t got = {0};
    if (read_ppm(out_path, &got) != 0) {
        printf("%-32s: 文件不存在或读取失败: %s\n", name, out_path);
        return;
    }
    double r = diff_ratio(expected, &got);
    printf("%-32s: 差异比例=%.6f (%s)\n", name, r, (r == 0.0 ? "OK" : (r < 0.001 ? "近似" : "不匹配")));
    free_image(&got);
}

int main() {
    image_t base = {0};
    if (load_jpeg_as_image("test.jpg", &base) != 0) {
        fprintf(stderr, "加载 test.jpg 失败\n");
        return -1;
    }

    // 期望图
    image_t e_rgb = base;
    image_t e_rot90={0}, e_rot180={0}, e_rot270={0};
    image_t e_flip_h={0}, e_flip_v={0};
    image_t e_half={0}, e_2x={0};
    image_t e_multi={0};
    image_t e_invert={0};

    make_rot90(&base, &e_rot90);
    make_rot180(&base, &e_rot180);
    make_rot270(&base, &e_rot270);
    make_flip_h(&base, &e_flip_h);
    make_flip_v(&base, &e_flip_v);
    make_scale_half(&base, &e_half);
    make_scale_2x(&base, &e_2x);
    // multi: rot90 + flip_h + half
    image_t tmp1={0}; make_rot90(&base, &tmp1);
    image_t tmp2={0}; make_flip_h(&tmp1, &tmp2); free_image(&tmp1);
    make_scale_half(&tmp2, &e_multi); free_image(&tmp2);
    make_invert(&base, &e_invert);

    // 对比
    report_case("rgb", &e_rgb, "output/test_rgb.ppm");
    report_case("rot90", &e_rot90, "output/test_rot90.ppm");
    report_case("rot180", &e_rot180, "output/test_rot180.ppm");
    report_case("rot270", &e_rot270, "output/test_rot270.ppm");
    report_case("flip_h", &e_flip_h, "output/test_flip_h.ppm");
    report_case("flip_v", &e_flip_v, "output/test_flip_v.ppm");
    report_case("scale_half", &e_half, "output/test_scale_half.ppm");
    report_case("scale_2x", &e_2x, "output/test_scale_2x.ppm");
    report_case("multi_rot90_flip_h_half", &e_multi, "output/test_multi_rot90_flip_h_half.ppm");
    report_case("invert", &e_invert, "output/test_invert.ppm");

    // 释放
    free_image(&base);
    free_image(&e_rot90);
    free_image(&e_rot180);
    free_image(&e_rot270);
    free_image(&e_flip_h);
    free_image(&e_flip_v);
    free_image(&e_half);
    free_image(&e_2x);
    free_image(&e_multi);
    free_image(&e_invert);

    return 0;
} 