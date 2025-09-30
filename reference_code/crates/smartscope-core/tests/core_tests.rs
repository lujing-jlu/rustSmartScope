//! SmartScope Core 集成测试
//!
//! 测试核心功能包括初始化、状态管理、配置和帧数据获取

use smartscope_core::{
    SmartScopeCore, SmartScopeStreamConfig, SmartScopeCorrectionConfig,
    CameraMode, CorrectionType, ImageFormat, SmartScopeError
};

/// 测试基础的生命周期管理
#[test]
fn test_lifecycle_management() {
    let mut core = SmartScopeCore::new().expect("Failed to create SmartScopeCore");

    // 初始状态检查
    assert!(!core.is_initialized());
    assert!(!core.is_started());

    // 配置参数
    let stream_config = SmartScopeStreamConfig {
        width: 640,
        height: 480,
        format: ImageFormat::RGB888,
        fps: 30,
        read_interval_ms: 33,
    };

    let correction_config = SmartScopeCorrectionConfig {
        correction_type: CorrectionType::None,
        params_dir: "/tmp/test_params".to_string(),
        enable_caching: false,
    };

    // 测试初始化
    core.initialize(stream_config, correction_config)
        .expect("Failed to initialize core");

    assert!(core.is_initialized());
    assert!(!core.is_started());

    // 测试启动
    core.start().expect("Failed to start core");
    assert!(core.is_started());

    // 测试停止
    core.stop().expect("Failed to stop core");
    assert!(!core.is_started());
    assert!(core.is_initialized()); // 停止后仍保持初始化状态
}

/// 测试重复操作的错误处理
#[test]
fn test_duplicate_operations_error_handling() {
    let mut core = SmartScopeCore::new().expect("Failed to create SmartScopeCore");

    let stream_config = SmartScopeStreamConfig::default();
    let correction_config = SmartScopeCorrectionConfig::default();

    // 第一次初始化应该成功
    core.initialize(stream_config.clone(), correction_config.clone())
        .expect("First initialization should succeed");

    // 第二次初始化应该失败
    match core.initialize(stream_config, correction_config) {
        Err(SmartScopeError::AlreadyInitialized(_)) => {
            // 期望的错误类型
        }
        _ => panic!("Expected AlreadyInitialized error"),
    }

    // 启动
    core.start().expect("Failed to start core");

    // 重复启动应该失败
    match core.start() {
        Err(SmartScopeError::AlreadyStarted(_)) => {
            // 期望的错误类型
        }
        _ => panic!("Expected AlreadyStarted error"),
    }
}

/// 测试未初始化状态下的操作
#[test]
fn test_operations_without_initialization() {
    let mut core = SmartScopeCore::new().expect("Failed to create SmartScopeCore");

    // 未初始化时启动应该失败
    match core.start() {
        Err(SmartScopeError::NotInitialized(_)) => {
            // 期望的错误类型
        }
        _ => panic!("Expected NotInitialized error"),
    }

    // 未初始化时获取状态应该失败
    match core.get_camera_status() {
        Err(SmartScopeError::NotInitialized(_)) => {
            // 期望的错误类型
        }
        _ => panic!("Expected NotInitialized error"),
    }
}

/// 测试未启动状态下的帧获取
#[test]
fn test_frame_operations_without_start() {
    let mut core = SmartScopeCore::new().expect("Failed to create SmartScopeCore");

    // 初始化但不启动
    let stream_config = SmartScopeStreamConfig::default();
    let correction_config = SmartScopeCorrectionConfig::default();
    core.initialize(stream_config, correction_config)
        .expect("Failed to initialize core");

    // 未启动时获取帧应该失败
    match core.get_left_frame() {
        Err(SmartScopeError::NotStarted(_)) => {
            // 期望的错误类型
        }
        _ => panic!("Expected NotStarted error"),
    }

    match core.get_right_frame() {
        Err(SmartScopeError::NotStarted(_)) => {
            // 期望的错误类型
        }
        _ => panic!("Expected NotStarted error"),
    }
}

/// 测试相机状态获取
#[test]
fn test_camera_status_retrieval() {
    let mut core = SmartScopeCore::new().expect("Failed to create SmartScopeCore");

    let stream_config = SmartScopeStreamConfig::default();
    let correction_config = SmartScopeCorrectionConfig::default();

    core.initialize(stream_config, correction_config)
        .expect("Failed to initialize core");

    // 获取相机状态
    let status = core.get_camera_status().expect("Failed to get camera status");

    // 验证状态字段
    assert_eq!(status.mode, CameraMode::StereoCamera);
    assert_eq!(status.camera_count, 2);
    assert!(status.left_connected);
    assert!(status.right_connected);
    assert!(status.timestamp > 0);
}

/// 测试帧数据获取
#[test]
fn test_frame_data_retrieval() {
    let mut core = SmartScopeCore::new().expect("Failed to create SmartScopeCore");

    let stream_config = SmartScopeStreamConfig {
        width: 640,  // 任意值，会被自动检测覆盖
        height: 480, // 任意值，会被自动检测覆盖
        format: ImageFormat::MJPEG, // 自动检测强制使用MJPEG
        fps: 30,
        read_interval_ms: 33,
    };
    let correction_config = SmartScopeCorrectionConfig::default();

    core.initialize(stream_config, correction_config)
        .expect("Failed to initialize core");
    core.start().expect("Failed to start core");

    // 测试左相机帧 - 使用自动检测的配置
    let left_frame = core.get_left_frame().expect("Failed to get left frame");
    assert_eq!(left_frame.width, 1280);  // 自动检测选择的最高分辨率
    assert_eq!(left_frame.height, 720);
    assert_eq!(left_frame.format, ImageFormat::MJPEG); // 自动检测强制使用MJPEG
    assert!(left_frame.size > 0); // MJPEG是压缩格式，大小可变
    assert!(!left_frame.data.is_null());
    assert!(left_frame.timestamp > 0);

    // 测试右相机帧 - 使用自动检测的配置
    let right_frame = core.get_right_frame().expect("Failed to get right frame");
    assert_eq!(right_frame.width, 1280);  // 自动检测选择的最高分辨率
    assert_eq!(right_frame.height, 720);
    assert_eq!(right_frame.format, ImageFormat::MJPEG); // 自动检测强制使用MJPEG
    assert!(right_frame.size > 0); // MJPEG是压缩格式，大小可变
    assert!(!right_frame.data.is_null());
    assert!(right_frame.timestamp > 0);

    // 验证左右帧的时间戳不同
    assert_ne!(left_frame.timestamp, right_frame.timestamp);
}

/// 测试校正类型设置
#[test]
fn test_correction_type_setting() {
    let mut core = SmartScopeCore::new().expect("Failed to create SmartScopeCore");

    let stream_config = SmartScopeStreamConfig::default();
    let correction_config = SmartScopeCorrectionConfig {
        correction_type: CorrectionType::None,
        params_dir: "/tmp/params".to_string(),
        enable_caching: true,
    };

    core.initialize(stream_config, correction_config)
        .expect("Failed to initialize core");

    // 测试各种校正类型设置
    for correction_type in [
        CorrectionType::None,
        CorrectionType::Distortion,
        CorrectionType::Stereo,
        CorrectionType::Both,
    ] {
        core.set_correction(correction_type)
            .expect("Failed to set correction type");
    }
}

/// 测试默认配置
#[test]
fn test_default_configurations() {
    let stream_config = SmartScopeStreamConfig::default();
    assert_eq!(stream_config.width, 640);  // 自动检测前的任意默认值
    assert_eq!(stream_config.height, 480); // 自动检测前的任意默认值
    assert_eq!(stream_config.format, ImageFormat::MJPEG); // 自动检测强制使用MJPEG
    assert_eq!(stream_config.fps, 30);
    assert_eq!(stream_config.read_interval_ms, 33);

    let correction_config = SmartScopeCorrectionConfig::default();
    assert_eq!(correction_config.correction_type, CorrectionType::None);
    assert!(correction_config.params_dir.is_empty());
    assert!(correction_config.enable_caching);
}

/// 性能基准测试
#[test]
fn test_frame_generation_performance() {
    let mut core = SmartScopeCore::new().expect("Failed to create SmartScopeCore");

    let stream_config = SmartScopeStreamConfig::default();
    let correction_config = SmartScopeCorrectionConfig::default();

    core.initialize(stream_config, correction_config)
        .expect("Failed to initialize core");
    core.start().expect("Failed to start core");

    let start_time = std::time::Instant::now();
    let iterations = 100;

    // 测试连续获取帧的性能
    for _ in 0..iterations {
        let _left = core.get_left_frame().expect("Failed to get left frame");
        let _right = core.get_right_frame().expect("Failed to get right frame");
    }

    let elapsed = start_time.elapsed();
    let fps = (iterations as f64 * 2.0) / elapsed.as_secs_f64(); // 2 frames per iteration

    println!("Frame generation rate: {:.2} FPS", fps);

    // 基本性能要求：至少能达到30fps
    assert!(fps >= 30.0, "Frame generation too slow: {} FPS", fps);
}