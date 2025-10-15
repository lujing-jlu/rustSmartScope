/* RustSmartScope Minimal C FFI Header */
/* Manually generated - basic bridge interface */

#ifndef SMARTSCOPE_FFI_H
#define SMARTSCOPE_FFI_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * C FFI错误码
 */
typedef enum {
    SMARTSCOPE_ERROR_SUCCESS = 0,
    SMARTSCOPE_ERROR_GENERAL = -1,
    SMARTSCOPE_ERROR_CONFIG = -3,
    SMARTSCOPE_ERROR_IO = -5,
} smartscope_ErrorCode;

/**
 * 初始化SmartScope系统
 */
int smartscope_init(void);

/**
 * 关闭SmartScope系统
 */
int smartscope_shutdown(void);

/**
 * 检查系统是否已初始化
 */
bool smartscope_is_initialized(void);

/**
 * 加载配置文件
 */
int smartscope_load_config(const char *config_path);

/**
 * 保存配置文件
 */
int smartscope_save_config(const char *config_path);

/**
 * 启用配置文件热重载
 */
int smartscope_enable_config_hot_reload(const char *config_path);

/**
 * 获取版本字符串
 */
const char *smartscope_get_version(void);

/**
 * 获取错误信息字符串
 */
const char *smartscope_get_error_string(int error_code);

/**
 * 释放由Rust分配的内存
 */
void smartscope_free_string(char *s);

// =========================
// External Storage Detection
// =========================

/**
 * 列出外置存储（U盘/SD卡）为JSON数组字符串。
 * 需要调用 smartscope_free_string 释放返回的字符串。
 * JSON 格式: [{"device":"/dev/sda1","label":"UDISK","mount_point":"/media/...","fs_type":"vfat"}, ...]
 */
char* smartscope_list_external_storages_json(void);

// =========================
// AI Inference (RKNN YOLOv8)
// =========================

typedef struct {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
    float confidence;
    int32_t class_id;
} smartscope_CDetection;

/** 初始化AI推理服务（在程序启动时调用） */
int smartscope_ai_init(const char* model_path, int num_workers);

/** 关闭AI推理服务（程序退出时调用） */
void smartscope_ai_shutdown(void);

/** 启用/禁用AI检测 */
void smartscope_ai_set_enabled(bool enabled);

/** 查询AI检测是否启用 */
bool smartscope_ai_is_enabled(void);

/** 提交RGB888图像给推理服务（非阻塞） */
int smartscope_ai_submit_rgb888(int width, int height, const uint8_t* data, size_t len);

/** 尝试获取最新推理结果；返回检测数量（>=0），错误返回-1 */
int smartscope_ai_try_get_latest_result(smartscope_CDetection* results_out, int max_results);

#ifdef __cplusplus
}
#endif

#endif /* SMARTSCOPE_FFI_H */
