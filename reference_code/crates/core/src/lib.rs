// Core 3D mathematics and point cloud algorithms
// This crate contains pure Rust logic without Qt dependencies

use chrono::{DateTime, Local};
use glam::{Mat4, Vec3};
use log::{debug, error, info, warn};
use rand::Rng;

/// Logger utility for core operations
pub mod logger {
    use super::*;
    use std::fmt;

    #[derive(Debug, Clone)]
    pub enum LogLevel {
        Debug,
        Info,
        Warning,
        Error,
    }

    #[derive(Debug, Clone)]
    pub struct LogMessage {
        pub timestamp: DateTime<Local>,
        pub level: LogLevel,
        pub category: String,
        pub message_en: String,
        pub message_zh: String,
    }

    impl LogMessage {
        pub fn new(level: LogLevel, category: &str, message_en: &str, message_zh: &str) -> Self {
            Self {
                timestamp: Local::now(),
                level,
                category: category.to_string(),
                message_en: message_en.to_string(),
                message_zh: message_zh.to_string(),
            }
        }

        pub fn get_message(&self, language: &str) -> &str {
            match language {
                "zh" => &self.message_zh,
                _ => &self.message_en,
            }
        }
    }

    impl fmt::Display for LogMessage {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            write!(
                f,
                "[{}] {}: {} | {}",
                self.timestamp.format("%Y-%m-%d %H:%M:%S"),
                match self.level {
                    LogLevel::Debug => "DEBUG",
                    LogLevel::Info => "INFO",
                    LogLevel::Warning => "WARN",
                    LogLevel::Error => "ERROR",
                },
                self.message_en,
                self.message_zh
            )
        }
    }

    /// Log a bilingual message
    pub fn log_bilingual(level: LogLevel, category: &str, message_en: &str, message_zh: &str) {
        let log_msg = LogMessage::new(level.clone(), category, message_en, message_zh);

        match level {
            LogLevel::Debug => debug!("{}", log_msg),
            LogLevel::Info => info!("{}", log_msg),
            LogLevel::Warning => warn!("{}", log_msg),
            LogLevel::Error => error!("{}", log_msg),
        }
    }

    /// Convenience functions for common log operations
    pub fn log_info(category: &str, message_en: &str, message_zh: &str) {
        log_bilingual(LogLevel::Info, category, message_en, message_zh);
    }

    pub fn log_warn(category: &str, message_en: &str, message_zh: &str) {
        log_bilingual(LogLevel::Warning, category, message_en, message_zh);
    }

    pub fn log_error(category: &str, message_en: &str, message_zh: &str) {
        log_bilingual(LogLevel::Error, category, message_en, message_zh);
    }

    pub fn log_debug(category: &str, message_en: &str, message_zh: &str) {
        log_bilingual(LogLevel::Debug, category, message_en, message_zh);
    }
}

/// Generate a random point cloud in spherical distribution
pub fn generate_sphere_points(count: usize) -> Vec<Vec3> {
    logger::log_info(
        "PointCloud",
        &format!("Generating {} points in spherical distribution", count),
        &format!("正在生成 {} 个球形分布点", count),
    );

    let mut rng = rand::thread_rng();
    let mut points = Vec::with_capacity(count);

    for _ in 0..count {
        let theta = rng.gen::<f32>() * 2.0 * std::f32::consts::PI;
        let phi = rng.gen::<f32>() * std::f32::consts::PI;
        let r = rng.gen::<f32>().powf(1.0 / 3.0) * 2.0; // Uniform distribution in sphere

        let x = r * phi.sin() * theta.cos();
        let y = r * phi.sin() * theta.sin();
        let z = r * phi.cos();

        points.push(Vec3::new(x, y, z));
    }

    logger::log_info(
        "PointCloud",
        &format!("Successfully generated {} points", points.len()),
        &format!("成功生成 {} 个点", points.len()),
    );

    points
}

/// Create rotation matrix from Euler angles
pub fn create_rotation_matrix(rotation_x: f32, rotation_y: f32, rotation_z: f32) -> Mat4 {
    logger::log_debug(
        "Math",
        &format!(
            "Creating rotation matrix: X={:.2}°, Y={:.2}°, Z={:.2}°",
            rotation_x, rotation_y, rotation_z
        ),
        &format!(
            "创建旋转矩阵: X={:.2}°, Y={:.2}°, Z={:.2}°",
            rotation_x, rotation_y, rotation_z
        ),
    );

    let rot_x = Mat4::from_rotation_x(rotation_x.to_radians());
    let rot_y = Mat4::from_rotation_y(rotation_y.to_radians());
    let rot_z = Mat4::from_rotation_z(rotation_z.to_radians());
    rot_z * rot_y * rot_x
}
