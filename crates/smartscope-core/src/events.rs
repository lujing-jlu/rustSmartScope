//! SmartScope事件系统

use serde::{Deserialize, Serialize};
use crate::types::{CameraType, CameraStatus};

/// 应用程序事件
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum AppEvent {
    /// 应用启动
    AppStarted,
    /// 应用关闭
    AppShutdown,
    /// 配置更新
    ConfigUpdated,
    /// 界面事件
    UiEvent(UiEvent),
    /// 相机事件
    CameraEvent(CameraEvent),
    /// 处理事件
    ProcessingEvent(ProcessingEvent),
}

/// 系统事件
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum SystemEvent {
    /// 应用事件
    App(AppEvent),
    /// 错误事件
    Error(ErrorEvent),
    /// 状态更新
    StateUpdate(StateUpdateEvent),
}

/// UI事件
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum UiEvent {
    /// 窗口大小改变
    WindowResized { width: u32, height: u32 },
    /// 主题改变
    ThemeChanged(String),
    /// 语言改变
    LanguageChanged(String),
    /// 页面切换
    PageChanged(String),
}

/// 相机事件
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum CameraEvent {
    /// 相机连接
    CameraConnected { camera_type: CameraType, device_path: String },
    /// 相机断开
    CameraDisconnected { camera_type: CameraType },
    /// 开始采集
    StartCapture,
    /// 停止采集
    StopCapture,
    /// 帧采集完成
    FrameCaptured { camera_type: CameraType, timestamp: u64 },
    /// 参数更新
    ParametersUpdated { camera_type: CameraType },
}

/// 处理事件
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ProcessingEvent {
    /// 开始处理
    ProcessingStarted,
    /// 处理完成
    ProcessingCompleted { processing_time_ms: u64 },
    /// 立体匹配完成
    StereoMatchingCompleted { disparity_computed: bool },
    /// 深度图生成完成
    DepthMapGenerated,
    /// 点云生成完成
    PointCloudGenerated { point_count: u32 },
}

/// 错误事件
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ErrorEvent {
    /// 相机错误
    CameraError { camera_type: CameraType, message: String },
    /// 处理错误
    ProcessingError { message: String },
    /// 配置错误
    ConfigError { message: String },
    /// 系统错误
    SystemError { message: String },
}

/// 状态更新事件
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum StateUpdateEvent {
    /// 相机状态更新
    CameraStatusUpdated(CameraStatus),
    /// 处理状态更新
    ProcessingStatusUpdated { processing: bool },
    /// FPS更新
    FpsUpdated { fps: f64 },
}