#ifndef SILENT_PRINTF_H
#define SILENT_PRINTF_H

// 简单的方法：通过条件编译完全替换printf和fprintf
// 在build.rs中通过DISABLE_PRINTF宏来控制是否显示日志

#ifdef DISABLE_PRINTF
    #include <cstdio>
    // 完全禁用printf和fprintf
    static inline int silent_printf(const char *fmt, ...) { return 0; }
    static inline int silent_fprintf(FILE *fp, const char *fmt, ...) { return 0; }
    #define printf silent_printf
    #define fprintf silent_fprintf
#endif

#endif // SILENT_PRINTF_H