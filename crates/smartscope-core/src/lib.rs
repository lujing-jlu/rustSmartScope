//! SmartScope Core - 最小核心引擎
//!
//! 提供基本的应用状态管理、配置系统和统一日志

pub mod config;
pub mod state;
pub mod error;
pub mod logger;
pub mod camera;
pub mod video_transform;

// Re-export core types
pub use config::AppConfig;
pub use state::AppState;
pub use error::{SmartScopeError, Result};
pub use logger::{
    UnifiedLogger, LoggerConfig, LogLevel, LogRotation,
    init_global_logger, get_global_logger, log_from_cpp, log_from_qml
};
pub use camera::{CameraManager, CameraStatus, CameraMode, VideoFrame};
pub use video_transform::{VideoTransform, VideoTransformConfig, apply_transform, apply_transforms};