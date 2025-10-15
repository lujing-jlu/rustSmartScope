use std::ffi::CString;
use std::os::raw::c_char;

/// 列出外置存储设备（U盘/SD卡）为JSON数组字符串
/// 需要调用 smartscope_free_string 释放返回的字符串
#[no_mangle]
pub extern "C" fn smartscope_list_external_storages_json() -> *mut c_char {
    let json = storage_detector::list_json();
    match CString::new(json) {
        Ok(s) => s.into_raw(),
        Err(_) => CString::new("[]").unwrap().into_raw(),
    }
}

