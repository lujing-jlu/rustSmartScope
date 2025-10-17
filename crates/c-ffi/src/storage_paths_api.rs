use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::PathBuf;

use crate::state::get_app_state;

fn join_path_like_qt(a: &str, b: &str) -> String {
    if a.is_empty() {
        return b.to_string();
    }
    if b.is_empty() {
        return a.to_string();
    }
    if a.ends_with('/') {
        format!("{}{}", a, b)
    } else {
        format!("{}/{}", a, b)
    }
}

fn resolve_base_path(session_kind: &str) -> Option<String> {
    // 获取当前存储配置
    let app_state = get_app_state().ok()?;
    let storage = app_state.get_storage_config();

    let mut base = String::new();
    match storage.location {
        smartscope_core::config::StorageLocation::External => {
            if let Some(dev) = storage.external_device.as_ref() {
                // 枚举当前外置设备列表，匹配选中设备
                if let Ok(list) = storage_detector::list() {
                    if let Some(entry) = list.into_iter().find(|e| &e.device == dev) {
                        base = join_path_like_qt(&entry.mount_point, &storage.external_relative_path);
                    }
                }
            }
            if base.is_empty() {
                // 找不到外置或未配置，回退机内
                base = storage.internal_base_path.clone();
            }
        }
        smartscope_core::config::StorageLocation::Internal => {
            base = storage.internal_base_path.clone();
        }
    }

    if base.is_empty() {
        tracing::warn!("{}: base path is empty", session_kind);
        None
    } else {
        Some(base)
    }
}

fn build_session_dir(base: &str, category: &str, display_mode: &str) -> String {
    // 日期与时间格式与 C++ 版一致
    let now = chrono::Local::now();
    let date_str = now.format("%Y-%m-%d").to_string();
    let time_str = now.format("%H-%M-%S").to_string();
    let session_name = format!("{}_{}_{}", date_str, time_str, display_mode);

    let category_dir = join_path_like_qt(base, category);
    let date_dir = join_path_like_qt(&category_dir, &date_str);
    join_path_like_qt(&date_dir, &session_name)
}

fn resolve_and_create(category: &str, display_mode: &str) -> Option<String> {
    let base = resolve_base_path(category)?;
    let session_dir = build_session_dir(&base, category, display_mode);
    // 创建目录（等价于 Qt 的 mkpath）
    let mut pb = PathBuf::new();
    pb.push(&session_dir);
    if let Err(e) = std::fs::create_dir_all(&pb) {
        tracing::error!("create_dir_all failed: {} -> {}", session_dir, e);
        return None;
    }
    Some(session_dir)
}

#[no_mangle]
pub extern "C" fn smartscope_storage_resolve_screenshot_session_path(display_mode: *const c_char) -> *mut c_char {
    let dm = unsafe { CStr::from_ptr(display_mode).to_string_lossy().to_string() };
    let result = resolve_and_create("Screenshots", &dm).unwrap_or_default();
    CString::new(result).unwrap_or_else(|_| CString::new("").unwrap()).into_raw()
}

#[no_mangle]
pub extern "C" fn smartscope_storage_resolve_capture_session_path(display_mode: *const c_char) -> *mut c_char {
    let dm = unsafe { CStr::from_ptr(display_mode).to_string_lossy().to_string() };
    let result = resolve_and_create("Pictures", &dm).unwrap_or_default();
    CString::new(result).unwrap_or_else(|_| CString::new("").unwrap()).into_raw()
}

#[no_mangle]
pub extern "C" fn smartscope_storage_resolve_video_session_path(display_mode: *const c_char) -> *mut c_char {
    let dm = unsafe { CStr::from_ptr(display_mode).to_string_lossy().to_string() };
    let result = resolve_and_create("Videos", &dm).unwrap_or_default();
    CString::new(result).unwrap_or_else(|_| CString::new("").unwrap()).into_raw()
}

