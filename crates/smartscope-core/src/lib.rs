//! SmartScope Core - 最小核心引擎
//!
//! 提供基本的应用状态管理和配置系统

pub mod config;
pub mod state;
pub mod error;

// Re-export core types
pub use config::AppConfig;
pub use state::AppState;
pub use error::{SmartScopeError, Result};