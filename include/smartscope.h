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

#ifdef __cplusplus
}
#endif

#endif /* SMARTSCOPE_FFI_H */