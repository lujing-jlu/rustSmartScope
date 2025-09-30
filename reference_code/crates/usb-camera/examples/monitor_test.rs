//! Camera Monitor Test
//! 
//! 这个示例测试usb-camera库的设备监控功能

use std::time::Duration;
use usb_camera::monitor::{CameraStatusMonitor, CameraMode, StatusMessage};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 初始化日志
    env_logger::init();
    
    println!("🔍 Camera Monitor Test");
    println!("{}", "=".repeat(40));

    // 创建监控器，每秒检查一次 (1000ms)
    let mut monitor = CameraStatusMonitor::new(1000);

    println!("🚀 启动设备监控...");
    monitor.start()?;
    
    println!("✅ 监控已启动");
    println!("📡 等待设备状态变化...");
    println!("💡 提示: 可以尝试插拔USB相机来观察状态变化");
    println!("🛑 10秒后自动停止\n");

    let mut message_count = 0;

    // 获取初始状态
    let initial_status = CameraStatusMonitor::get_current_status()?;
    let mut last_mode = initial_status.mode;

    println!("📋 初始状态: {:?}", last_mode);
    match last_mode {
        CameraMode::NoCamera => {
            println!("   📵 当前无相机连接");
        }
        CameraMode::SingleCamera => {
            println!("   📷 当前单相机模式");
            for camera in &initial_status.cameras {
                println!("     - {}: {}", camera.name, camera.path.display());
            }
        }
        CameraMode::StereoCamera => {
            println!("   📷📷 当前立体相机模式");
            for camera in &initial_status.cameras {
                println!("     - {}: {}", camera.name, camera.path.display());
            }
        }
    }

    // 监听状态消息，最多10秒
    let start_time = std::time::Instant::now();
    while start_time.elapsed() < Duration::from_secs(10) {
        match monitor.try_get_status() {
            Some(status) => {
                message_count += 1;

                if status.mode != last_mode {
                    println!("🔄 [{}] 相机模式变化: {:?} -> {:?}",
                        message_count,
                        last_mode,
                        status.mode
                    );

                    match status.mode {
                        CameraMode::NoCamera => {
                            println!("   📵 无相机连接");
                        }
                        CameraMode::SingleCamera => {
                            println!("   📷 单相机模式");
                            for camera in &status.cameras {
                                println!("     - {}: {}", camera.name, camera.path.display());
                            }
                        }
                        CameraMode::StereoCamera => {
                            println!("   📷📷 立体相机模式");
                            for camera in &status.cameras {
                                println!("     - {}: {}", camera.name, camera.path.display());
                            }
                        }
                    }

                    last_mode = status.mode;
                } else if message_count % 5 == 0 {
                    // 每5次显示一次心跳
                    println!("💓 [{}] 监控正常 - 模式: {:?}, 设备数: {}",
                        message_count,
                        status.mode,
                        status.cameras.len()
                    );
                }
            }
            None => {
                // 没有新状态，等待一下
                std::thread::sleep(Duration::from_millis(100));
            }
        }
    }

    // 停止监控
    monitor.stop()?;
    println!("\n🛑 监控已停止");
    println!("📊 总共收到 {} 条状态消息", message_count);
    println!("🎉 测试完成！");
    
    Ok(())
}
