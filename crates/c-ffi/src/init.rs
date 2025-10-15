use std::os::raw::c_int;

use smartscope_core::{
    init_global_logger, AppState, LogLevel, LogRotation, LoggerConfig,
};

use crate::state::{APP_STATE, INIT};
use crate::types::ErrorCode;

/// 初始化SmartScope系统和日志
#[no_mangle]
pub extern "C" fn smartscope_init() -> c_int {
    INIT.call_once(|| {
        // 初始化统一日志系统
        let log_config = LoggerConfig {
            level: LogLevel::Info,
            log_dir: "logs".to_string(),
            console_output: true,
            file_output: true,
            json_format: false,
            rotation: LogRotation::Daily,
        };

        if let Err(e) = init_global_logger(log_config) {
            std::println!("Failed to initialize logger: {}", e);
        }

        unsafe {
            let mut state = AppState::new();
            match state.initialize() {
                Ok(_) => {
                    APP_STATE = Some(state);
                }
                Err(e) => {
                    std::println!("Failed to initialize: {}", e);
                }
            }
        }
    });

    ErrorCode::Success as c_int
}

/// 关闭SmartScope系统
#[no_mangle]
#[allow(static_mut_refs)]
pub extern "C" fn smartscope_shutdown() -> c_int {
    unsafe {
        if let Some(mut state) = APP_STATE.take() {
            state.shutdown();
        }
    }

    ErrorCode::Success as c_int
}

