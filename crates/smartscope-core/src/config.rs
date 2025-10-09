//! 完整的SmartScope配置管理系统
//!
//! 支持相机、算法、硬件、UI等全方位配置

use serde::{Deserialize, Serialize};
use crate::error::{SmartScopeError, Result};
use std::path::Path;

/// SmartScope主配置结构
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SmartScopeConfig {
    pub app: AppConfig,
    pub camera: CameraConfig,
    pub stereo: StereoConfig,
    pub hardware: HardwareConfig,
    pub ui: UiConfig,
    pub measurement: MeasurementConfig,
    pub logging: LoggingConfig,
    pub performance: PerformanceConfig,
}

impl Default for SmartScopeConfig {
    fn default() -> Self {
        Self {
            app: AppConfig::default(),
            camera: CameraConfig::default(),
            stereo: StereoConfig::default(),
            hardware: HardwareConfig::default(),
            ui: UiConfig::default(),
            measurement: MeasurementConfig::default(),
            logging: LoggingConfig::default(),
            performance: PerformanceConfig::default(),
        }
    }
}

/// 应用程序基础配置
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppConfig {
    pub name: String,
    pub version: String,
    pub window_width: u32,
    pub window_height: u32,
}

impl Default for AppConfig {
    fn default() -> Self {
        Self {
            name: "RustSmartScope".to_string(),
            version: "0.1.0".to_string(),
            window_width: 1920,
            window_height: 1080,
        }
    }
}

/// 相机配置
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CameraConfig {
    pub width: u32,
    pub height: u32,
    pub fps: u32,
    pub left: CameraDevice,
    pub right: CameraDevice,
    pub control: CameraControlConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CameraDevice {
    pub name_keywords: Vec<String>,
    pub parameters_path: String,
    pub rot_trans_path: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CameraControlConfig {
    pub auto_exposure: bool,
    pub exposure_time: u32,
    pub gain: f32,
    pub auto_white_balance: bool,
    pub brightness: i32,
    pub contrast: i32,
    pub saturation: i32,
}

impl Default for CameraConfig {
    fn default() -> Self {
        Self {
            width: 1280,
            height: 720,
            fps: 30,
            left: CameraDevice {
                name_keywords: vec!["cameral".to_string(), "left".to_string(), "video3".to_string()],
                parameters_path: "camera_parameters/camera0_intrinsics.dat".to_string(),
                rot_trans_path: "camera_parameters/camera0_rot_trans.dat".to_string(),
            },
            right: CameraDevice {
                name_keywords: vec!["camerar".to_string(), "right".to_string(), "video1".to_string()],
                parameters_path: "camera_parameters/camera1_intrinsics.dat".to_string(),
                rot_trans_path: "camera_parameters/camera1_rot_trans.dat".to_string(),
            },
            control: CameraControlConfig {
                auto_exposure: true,
                exposure_time: 10000,
                gain: 1.0,
                auto_white_balance: true,
                brightness: 0,
                contrast: 0,
                saturation: 0,
            },
        }
    }
}

/// 立体视觉算法配置
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StereoConfig {
    pub matcher: String,
    pub min_disparity: i32,
    pub num_disparities: i32,
    pub block_size: i32,
    pub p1: i32,
    pub p2: i32,
    pub disp12_max_diff: i32,
    pub pre_filter_cap: i32,
    pub uniqueness_ratio: i32,
    pub speckle_window_size: i32,
    pub speckle_range: i32,
    pub neural: NeuralConfig,
    pub point_cloud: PointCloudConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NeuralConfig {
    pub model_path: String,
    pub provider: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PointCloudConfig {
    pub max_points: u32,
    pub filter_outliers: bool,
    pub voxel_size: f32,
}

impl Default for StereoConfig {
    fn default() -> Self {
        Self {
            matcher: "sgbm".to_string(),
            min_disparity: 0,
            num_disparities: 128,
            block_size: 9,
            p1: 200,
            p2: 800,
            disp12_max_diff: 1,
            pre_filter_cap: 31,
            uniqueness_ratio: 15,
            speckle_window_size: 100,
            speckle_range: 32,
            neural: NeuralConfig {
                model_path: "models/stereo_model.onnx".to_string(),
                provider: "cuda".to_string(),
            },
            point_cloud: PointCloudConfig {
                max_points: 1000000,
                filter_outliers: true,
                voxel_size: 0.01,
            },
        }
    }
}

/// 硬件设备配置
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HardwareConfig {
    pub hid: HidConfig,
    pub device: DeviceConfig,
    pub processing: ProcessingConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HidConfig {
    pub vendor_id: u16,
    pub product_id: u16,
    pub usage_page: u16,
    pub usage: u16,
    pub report_size: usize,
    pub timeout_ms: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeviceConfig {
    pub light_level: u8,
    pub lens_locked: bool,
    pub lens_speed: u8,
    pub servo_x_angle: f32,
    pub servo_y_angle: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProcessingConfig {
    pub rotation_enabled: bool,
    pub rotation_angle: f32,
    pub correction_enabled: bool,
    pub stereo_rectification_enabled: bool,
    pub disparity_enabled: bool,
    pub disparity_quality: String,
}

impl Default for HardwareConfig {
    fn default() -> Self {
        Self {
            hid: HidConfig {
                vendor_id: 0x0001,
                product_id: 0xEDD1,
                usage_page: 0xFF60,
                usage: 0x61,
                report_size: 33,
                timeout_ms: 1000,
            },
            device: DeviceConfig {
                light_level: 2,
                lens_locked: false,
                lens_speed: 50,
                servo_x_angle: 90.0,
                servo_y_angle: 90.0,
            },
            processing: ProcessingConfig {
                rotation_enabled: false,
                rotation_angle: 0.0,
                correction_enabled: true,
                stereo_rectification_enabled: true,
                disparity_enabled: true,
                disparity_quality: "Medium".to_string(),
            },
        }
    }
}

/// UI界面配置
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UiConfig {
    pub language: String,
    pub theme: String,
    pub show_fps: bool,
    pub auto_save: bool,
}

impl Default for UiConfig {
    fn default() -> Self {
        Self {
            language: "zh_CN".to_string(),
            theme: "dark".to_string(),
            show_fps: true,
            auto_save: true,
        }
    }
}

/// 测量系统配置
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MeasurementConfig {
    pub unit: String,
    pub precision: u8,
    pub save_path: String,
    pub max_reasonable_depth_mm: f32,
    pub max_reasonable_length_mm: f32,
    pub max_reasonable_area_mm2: f32,
}

impl Default for MeasurementConfig {
    fn default() -> Self {
        Self {
            unit: "mm".to_string(),
            precision: 2,
            save_path: "measurements/".to_string(),
            max_reasonable_depth_mm: 10000.0,
            max_reasonable_length_mm: 10000.0,
            max_reasonable_area_mm2: 1000000.0,
        }
    }
}

/// 日志系统配置
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LoggingConfig {
    pub level: String,
    pub file_path: String,
    pub max_file_size: u64,
    pub backup_count: u32,
    pub enable_detailed_logging: bool,
}

impl Default for LoggingConfig {
    fn default() -> Self {
        Self {
            level: "info".to_string(),
            file_path: "logs/smartscope.log".to_string(),
            max_file_size: 10485760,  // 10MB
            backup_count: 5,
            enable_detailed_logging: true,
        }
    }
}

/// 性能优化配置
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PerformanceConfig {
    pub enable_parallel_processing: bool,
    pub max_threads: u32,
    pub enable_gpu_acceleration: bool,
}

impl Default for PerformanceConfig {
    fn default() -> Self {
        Self {
            enable_parallel_processing: true,
            max_threads: 4,
            enable_gpu_acceleration: true,
        }
    }
}

impl SmartScopeConfig {
    /// 从TOML文件加载配置
    pub fn load_from_file<P: AsRef<Path>>(path: P) -> Result<Self> {
        let content = std::fs::read_to_string(path)
            .map_err(|e| SmartScopeError::Config(format!("读取配置文件失败: {}", e)))?;

        let config: SmartScopeConfig = toml::from_str(&content)
            .map_err(|e| SmartScopeError::Config(format!("解析配置文件失败: {}", e)))?;

        Ok(config)
    }

    /// 保存配置到TOML文件
    pub fn save_to_file<P: AsRef<Path>>(&self, path: P) -> Result<()> {
        let content = toml::to_string_pretty(self)
            .map_err(|e| SmartScopeError::Config(format!("序列化配置失败: {}", e)))?;

        std::fs::write(path, content)
            .map_err(|e| SmartScopeError::Config(format!("写入配置文件失败: {}", e)))?;

        Ok(())
    }

    /// 验证配置有效性
    pub fn validate(&self) -> Result<()> {
        // 验证相机配置
        if self.camera.width == 0 || self.camera.height == 0 {
            return Err(SmartScopeError::Config("相机分辨率不能为0".to_string()));
        }

        if self.camera.fps == 0 || self.camera.fps > 120 {
            return Err(SmartScopeError::Config("相机帧率应在1-120之间".to_string()));
        }

        // 验证立体视觉配置
        if self.stereo.num_disparities <= 0 || self.stereo.num_disparities % 16 != 0 {
            return Err(SmartScopeError::Config("视差数量必须是16的倍数且大于0".to_string()));
        }

        if self.stereo.block_size < 5 || self.stereo.block_size % 2 == 0 {
            return Err(SmartScopeError::Config("块大小必须是奇数且>=5".to_string()));
        }

        // 验证硬件配置
        if self.hardware.device.light_level > 4 {
            return Err(SmartScopeError::Config("LED亮度级别应在0-4之间".to_string()));
        }

        if self.hardware.device.servo_x_angle < 0.0 || self.hardware.device.servo_x_angle > 180.0 {
            return Err(SmartScopeError::Config("舵机X角度应在0-180度之间".to_string()));
        }

        if self.hardware.device.servo_y_angle < 0.0 || self.hardware.device.servo_y_angle > 180.0 {
            return Err(SmartScopeError::Config("舵机Y角度应在0-180度之间".to_string()));
        }

        // 验证测量配置
        if self.measurement.precision > 6 {
            return Err(SmartScopeError::Config("测量精度不应超过6位小数".to_string()));
        }

        Ok(())
    }

    /// 从环境变量或默认路径加载配置
    pub fn load_with_fallback() -> Self {
        let config_paths = vec![
            "smartscope.toml",
            "config/smartscope.toml",
            "smartscope_config.toml",
            "test_config.toml",  // 向后兼容
        ];

        for path in config_paths {
            if let Ok(config) = Self::load_from_file(path) {
                tracing::debug!("配置加载成功: {}", path);
                return config;
            }
        }

        tracing::debug!("未找到配置文件，使用默认配置");
        Self::default()
    }

    /// 合并配置（部分更新）
    pub fn merge_from(&mut self, partial: PartialConfig) -> Result<()> {
        if let Some(camera) = partial.camera {
            self.camera = camera;
        }
        if let Some(stereo) = partial.stereo {
            self.stereo = stereo;
        }
        if let Some(hardware) = partial.hardware {
            self.hardware = hardware;
        }
        if let Some(ui) = partial.ui {
            self.ui = ui;
        }

        self.validate()?;
        Ok(())
    }
}

/// 部分配置结构，用于运行时更新
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PartialConfig {
    pub camera: Option<CameraConfig>,
    pub stereo: Option<StereoConfig>,
    pub hardware: Option<HardwareConfig>,
    pub ui: Option<UiConfig>,
}

impl AppConfig {
    /// 保持向后兼容性的简单配置加载
    pub fn load_from_file<P: AsRef<Path>>(path: P) -> Result<Self> {
        let full_config = SmartScopeConfig::load_from_file(path)?;
        Ok(full_config.app)
    }

    /// 保持向后兼容性的简单配置保存
    pub fn save_to_file<P: AsRef<Path>>(&self, path: P) -> Result<()> {
        let mut full_config = SmartScopeConfig::default();
        full_config.app = self.clone();
        full_config.save_to_file(path)
    }
}