//! SmartScope Core Library
//!
//! 提供统一的相机管理、流处理和校正功能的核心引擎

pub mod core;
pub mod types;
pub mod error;
pub mod pipeline;

// 重新导出核心类型
pub use core::SmartScopeCore;
pub use types::*;
pub use error::{SmartScopeError, SmartScopeResult};
pub use pipeline::{
    FrameProcessingPipeline, ProcessingStage, DisplayTransform,
    ProcessingPipelineConfig, DisparityQuality, ProcessedFrame, PipelineStats
};