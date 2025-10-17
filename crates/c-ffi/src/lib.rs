//! SmartScope Minimal C FFI Interface (modularized)
//!
//! 将原有的单文件 FFI 拆分为多个模块以便维护，
//! 保持所有对外 C 接口（符号名与签名）不变。

#[macro_use]
extern crate lazy_static;

// 模块划分（仅内部组织，不影响导出的 C 符号）
mod types;          // C 侧可见的枚举/结构体与映射
mod state;          // 全局 AppState 与初始化辅助
mod init;           // smartscope_init / smartscope_shutdown
mod config_api;     // 配置加载/保存/热重载与工具函数
mod logging_api;    // 日志 FFI
mod camera_api;     // 相机启动/停止/取帧/状态
mod ai_api;         // AI 推理接口
mod video_api;      // 视频变换与畸变校正接口
mod params_api;     // 相机参数 get/set/range/reset
mod storage_api;    // 外置存储检测
mod storage_config_api; // 存储配置设置/获取
mod storage_paths_api;  // 存储会话路径解析与创建
mod storage_events_api; // 存储变更回调（推送）

// 仅为保证链接导出，将各模块中使用的符号保持为 #[no_mangle] extern "C"
// 此处不需要再做 re-export。
