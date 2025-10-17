use std::ffi::CString;
use std::os::raw::{c_char, c_void};
use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread::{self, JoinHandle};
use std::time::Duration;

use crate::state::get_app_state;

type ListCb = extern "C" fn(*mut c_void, *const c_char);
type CfgCb = extern "C" fn(*mut c_void, *const c_char);

#[derive(Clone, Copy)]
struct CbPair {
    // 存储为 usize，避免原生裸指针的 Send 限制
    ctx: usize,
    list_cb: Option<ListCb>,
    cfg_cb: Option<CfgCb>,
}

// 将不透明的 C 指针跨线程传递，仅在回调中使用，不在 Rust 侧解引用。
unsafe impl Send for CbPair {}

struct Monitor {
    stop: Arc<AtomicBool>,
    handle: Option<JoinHandle<()>>,
    cbs: CbPair,
}

impl Monitor {
    fn start(cbs: CbPair) -> Self {
        let stop = Arc::new(AtomicBool::new(false));
        let stop_clone = stop.clone();
        let handle = thread::spawn(move || {
            let mut last_list = String::new();
            let mut last_cfg = String::new();
            loop {
                if stop_clone.load(Ordering::Relaxed) { break; }

                // Storage list JSON
                let list_json = storage_detector::list_json();
                if list_json != last_list {
                    if let Some(cb) = cbs.list_cb {
                        if let Ok(cstr) = CString::new(list_json.clone()) {
                            cb(cbs.ctx as *mut c_void, cstr.as_ptr());
                            // drop cstr here (after callback returns)
                        }
                    }
                    last_list = list_json;
                }

                // Storage config JSON
                if let Ok(app_state) = get_app_state() {
                    let storage = app_state.get_storage_config();
                    if let Ok(cfg_json) = serde_json::to_string(&storage) {
                        if cfg_json != last_cfg {
                            if let Some(cb) = cbs.cfg_cb {
                                if let Ok(cstr) = CString::new(cfg_json.clone()) {
                                    cb(cbs.ctx as *mut c_void, cstr.as_ptr());
                                }
                            }
                            last_cfg = cfg_json;
                        }
                    }
                }

                thread::sleep(Duration::from_millis(1000));
            }
        });

        Self { stop, handle: Some(handle), cbs }
    }

    fn stop(mut self) {
        self.stop.store(true, Ordering::Relaxed);
        if let Some(h) = self.handle.take() {
            let _ = h.join();
        }
    }
}

lazy_static::lazy_static! {
    static ref MONITOR: Mutex<Option<Monitor>> = Mutex::new(None);
}

#[no_mangle]
pub extern "C" fn smartscope_storage_register_callbacks(
    ctx: *mut c_void,
    list_cb: Option<ListCb>,
    cfg_cb: Option<CfgCb>,
) {
    let mut guard = MONITOR.lock().unwrap();
    // Stop existing monitor if any
    if let Some(m) = guard.take() {
        m.stop();
    }
    // Start new monitor only if at least one callback provided
    if list_cb.is_some() || cfg_cb.is_some() {
        let cbs = CbPair { ctx: ctx as usize, list_cb, cfg_cb };
        *guard = Some(Monitor::start(cbs));
    }
}

#[no_mangle]
pub extern "C" fn smartscope_storage_unregister_callbacks(_ctx: *mut c_void) {
    let mut guard = MONITOR.lock().unwrap();
    if let Some(m) = guard.take() {
        m.stop();
    }
}
