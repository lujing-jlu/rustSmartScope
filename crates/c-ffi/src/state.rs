use std::sync::Once;

use smartscope_core::{AppState, SmartScopeError};

/// 全局应用状态实例（仅在本 crate 内部通过 get_app_state 访问）
pub(crate) static mut APP_STATE: Option<AppState> = None;
pub(crate) static INIT: Once = Once::new();

/// 取得可变全局 AppState 指针（供各 FFI 接口内部使用）
#[allow(static_mut_refs)]
pub(crate) fn get_app_state() -> Result<&'static mut AppState, SmartScopeError> {
    unsafe {
        APP_STATE
            .as_mut()
            .ok_or_else(|| SmartScopeError::Unknown("SmartScope not initialized".to_string()))
    }
}

