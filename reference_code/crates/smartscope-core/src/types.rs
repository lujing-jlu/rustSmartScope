//! SmartScope核心数据类型定义

use serde::{Deserialize, Serialize};

/// 相机模式
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum CameraMode {
    /// 无相机连接
    NoCamera = 0,
    /// 单相机模式
    SingleCamera = 1,
    /// 立体相机模式（左右相机）
    StereoCamera = 2,
}

/// 相机类型
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum CameraType {
    /// 左相机
    Left = 0,
    /// 右相机
    Right = 1,
}

/// 校正类型
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum CorrectionType {
    /// 无校正
    None = 0,
    /// 仅畸变校正
    Distortion = 1,
    /// 仅立体校正
    Stereo = 2,
    /// 畸变和立体校正
    Both = 3,
}

/// 图像格式
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ImageFormat {
    /// MJPEG格式
    MJPEG = 0,
    /// RGB888格式
    RGB888 = 1,
    /// BGR888格式
    BGR888 = 2,
    /// YUYV格式
    YUYV = 3,
}

/// 流配置结构体
#[repr(C)]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SmartScopeStreamConfig {
    /// 目标宽度
    pub width: u32,
    /// 目标高度
    pub height: u32,
    /// 目标格式
    pub format: ImageFormat,
    /// 帧率 (FPS)
    pub fps: u32,
    /// 读取间隔（毫秒）
    pub read_interval_ms: u32,
}

/// 校正配置结构体
#[repr(C)]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SmartScopeCorrectionConfig {
    /// 校正类型
    pub correction_type: CorrectionType,
    /// 相机参数目录路径
    pub params_dir: String,
    /// 启用缓存
    pub enable_caching: bool,
}

/// 相机状态结构体（用于C FFI）
#[repr(C)]
#[derive(Debug, Clone)]
pub struct SmartScopeCameraStatus {
    /// 当前相机模式
    pub mode: CameraMode,
    /// 连接的相机数量
    pub camera_count: u32,
    /// 左相机连接状态
    pub left_connected: bool,
    /// 右相机连接状态
    pub right_connected: bool,
    /// 最后更新时间戳
    pub timestamp: u64,
}

/// 帧数据结构体（用于C FFI）
#[repr(C)]
#[derive(Debug, Clone)]
pub struct SmartScopeFrameData {
    /// 帧宽度
    pub width: u32,
    /// 帧高度
    pub height: u32,
    /// 图像格式
    pub format: ImageFormat,
    /// 数据指针（用于C FFI）
    pub data: *mut u8,
    /// 数据大小（字节）
    pub size: usize,
    /// 相机类型（左/右）
    pub camera_type: CameraType,
    /// 时间戳（自纪元以来的毫秒数）
    pub timestamp: u64,
}


impl Default for SmartScopeStreamConfig {
    fn default() -> Self {
        Self {
            width: 640,   // 任意值，会被自动检测覆盖
            height: 480,  // 任意值，会被自动检测覆盖
            format: ImageFormat::MJPEG, // 自动检测强制使用MJPEG
            fps: 30,
            read_interval_ms: 33, // ~30fps
        }
    }
}

impl Default for SmartScopeCorrectionConfig {
    fn default() -> Self {
        Self {
            correction_type: CorrectionType::None,
            params_dir: String::new(),
            enable_caching: true,
        }
    }
}