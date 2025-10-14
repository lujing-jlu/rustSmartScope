//! 统一日志系统
//!
//! 为Rust、C++和QML提供统一的日志功能

use std::fs;
use std::sync::Once;
use tracing::{info, Level};
use tracing_appender::{non_blocking, rolling};
use tracing_subscriber::{
    fmt::{format::FmtSpan, time::LocalTime},
    layer::SubscriberExt,
    util::SubscriberInitExt,
    EnvFilter, Registry,
};

static LOGGER_INIT: Once = Once::new();

/// 日志级别枚举（C FFI兼容）
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
}

impl From<LogLevel> for Level {
    fn from(level: LogLevel) -> Self {
        match level {
            LogLevel::Trace => Level::TRACE,
            LogLevel::Debug => Level::DEBUG,
            LogLevel::Info => Level::INFO,
            LogLevel::Warn => Level::WARN,
            LogLevel::Error => Level::ERROR,
        }
    }
}

/// 日志配置
#[derive(Debug, Clone)]
pub struct LoggerConfig {
    /// 日志级别
    pub level: LogLevel,
    /// 日志目录
    pub log_dir: String,
    /// 是否输出到控制台
    pub console_output: bool,
    /// 是否输出到文件
    pub file_output: bool,
    /// 是否使用JSON格式
    pub json_format: bool,
    /// 文件滚动策略
    pub rotation: LogRotation,
}

#[derive(Debug, Clone)]
pub enum LogRotation {
    Daily,
    Hourly,
    Never,
}

impl Default for LoggerConfig {
    fn default() -> Self {
        Self {
            level: LogLevel::Info,
            log_dir: "logs".to_string(),
            console_output: true,
            file_output: true,
            json_format: false,
            rotation: LogRotation::Daily,
        }
    }
}

/// 统一日志系统
pub struct UnifiedLogger {
    config: LoggerConfig,
}

impl UnifiedLogger {
    /// 创建新的日志系统
    pub fn new(config: LoggerConfig) -> Self {
        Self { config }
    }

    /// 初始化日志系统
    pub fn initialize(&self) -> crate::Result<()> {
        LOGGER_INIT.call_once(|| {
            if let Err(e) = self.setup_logging() {
                std::println!("Failed to initialize logger: {}", e);
            }
        });
        Ok(())
    }

    /// 设置日志系统
    fn setup_logging(&self) -> crate::Result<()> {
        // 确保日志目录存在
        if self.config.file_output {
            fs::create_dir_all(&self.config.log_dir)?;
        }

        // 创建环境过滤器
        let level_str = match self.config.level {
            LogLevel::Trace => "trace",
            LogLevel::Debug => "debug",
            LogLevel::Info => "info",
            LogLevel::Warn => "warn",
            LogLevel::Error => "error",
        };

        let env_filter = EnvFilter::try_from_default_env().unwrap_or_else(|_| {
            EnvFilter::new(format!(
                "smartscope={},rustsmartscope={}",
                level_str, level_str
            ))
        });

        let registry = Registry::default().with(env_filter);

        // 控制台输出层
        if self.config.console_output {
            let console_layer = tracing_subscriber::fmt::layer()
                .with_target(true)
                .with_thread_ids(true)
                .with_span_events(FmtSpan::CLOSE)
                .with_timer(LocalTime::rfc_3339());

            if self.config.file_output {
                // 同时输出到控制台和文件
                let file_appender = match self.config.rotation {
                    LogRotation::Daily => rolling::daily(&self.config.log_dir, "smartscope.log"),
                    LogRotation::Hourly => rolling::hourly(&self.config.log_dir, "smartscope.log"),
                    LogRotation::Never => rolling::never(&self.config.log_dir, "smartscope.log"),
                };

                let (file_writer, _guard) = non_blocking(file_appender);

                let file_layer = tracing_subscriber::fmt::layer()
                    .with_writer(file_writer)
                    .with_target(true)
                    .with_thread_ids(true)
                    .with_ansi(false)
                    .with_timer(LocalTime::rfc_3339());

                if self.config.json_format {
                    registry.with(console_layer).with(file_layer.json()).init();
                } else {
                    registry.with(console_layer).with(file_layer).init();
                }
            } else {
                // 仅控制台输出
                registry.with(console_layer).init();
            }
        } else if self.config.file_output {
            // 仅文件输出
            let file_appender = match self.config.rotation {
                LogRotation::Daily => rolling::daily(&self.config.log_dir, "smartscope.log"),
                LogRotation::Hourly => rolling::hourly(&self.config.log_dir, "smartscope.log"),
                LogRotation::Never => rolling::never(&self.config.log_dir, "smartscope.log"),
            };

            let (file_writer, _guard) = non_blocking(file_appender);

            let file_layer = tracing_subscriber::fmt::layer()
                .with_writer(file_writer)
                .with_target(true)
                .with_thread_ids(true)
                .with_ansi(false)
                .with_timer(LocalTime::rfc_3339());

            if self.config.json_format {
                registry.with(file_layer.json()).init();
            } else {
                registry.with(file_layer).init();
            }
        }

        info!("UnifiedLogger initialized");
        info!("Log level: {:?}", self.config.level);
        info!("Console output: {}", self.config.console_output);
        info!("File output: {}", self.config.file_output);
        if self.config.file_output {
            info!("Log directory: {}", self.config.log_dir);
        }

        Ok(())
    }

    /// 记录日志（供C FFI使用）
    pub fn log(&self, level: LogLevel, module: &str, message: &str) {
        match level {
            LogLevel::Trace => tracing::trace!("{}: {}", module, message),
            LogLevel::Debug => tracing::debug!("{}: {}", module, message),
            LogLevel::Info => tracing::info!("{}: {}", module, message),
            LogLevel::Warn => tracing::warn!("{}: {}", module, message),
            LogLevel::Error => tracing::error!("{}: {}", module, message),
        }
    }
}

/// 全局日志实例
static mut GLOBAL_LOGGER: Option<UnifiedLogger> = None;

/// 初始化全局日志系统
pub fn init_global_logger(config: LoggerConfig) -> crate::Result<()> {
    unsafe {
        let logger = UnifiedLogger::new(config);
        logger.initialize()?;
        GLOBAL_LOGGER = Some(logger);
    }
    Ok(())
}

/// 获取全局日志实例
#[allow(static_mut_refs)]
pub fn get_global_logger() -> Option<&'static UnifiedLogger> {
    unsafe { GLOBAL_LOGGER.as_ref() }
}

/// 便捷宏：记录来自C++的日志
pub fn log_from_cpp(level: LogLevel, module: &str, message: &str) {
    if let Some(logger) = get_global_logger() {
        logger.log(level, module, message);
    } else {
        // 如果日志系统未初始化，直接输出到stderr
        std::eprintln!(
            "[{}][{}] {}",
            match level {
                LogLevel::Trace => "TRACE",
                LogLevel::Debug => "DEBUG",
                LogLevel::Info => "INFO",
                LogLevel::Warn => "WARN",
                LogLevel::Error => "ERROR",
            },
            module,
            message
        );
    }
}

/// 便捷宏：记录来自QML的日志
pub fn log_from_qml(level: LogLevel, message: &str) {
    log_from_cpp(level, "QML", message);
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::tempdir;

    #[test]
    fn test_logger_initialization() {
        let temp_dir = tempdir().unwrap();
        let config = LoggerConfig {
            log_dir: temp_dir.path().to_string_lossy().to_string(),
            ..Default::default()
        };

        let logger = UnifiedLogger::new(config);
        assert!(logger.initialize().is_ok());
    }

    #[test]
    fn test_log_levels() {
        assert_eq!(Level::from(LogLevel::Info), Level::INFO);
        assert_eq!(Level::from(LogLevel::Error), Level::ERROR);
    }
}
