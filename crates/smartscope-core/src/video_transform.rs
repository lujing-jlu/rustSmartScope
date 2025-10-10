//! 视频变换模块
//!
//! 提供基于RockchipRGA硬件加速的视频变换功能，包括旋转、翻转、反色等

use crate::error::{SmartScopeError, Result};
use rockchip_rga::{RgaProcessor, RgaImage, RgaFormat};
use std::sync::{Arc, Mutex};
use tracing::{debug, info, warn};

/// 视频变换类型
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum VideoTransform {
    /// 旋转90度
    Rotate90,
    /// 旋转180度
    Rotate180,
    /// 旋转270度
    Rotate270,
    /// 水平翻转
    FlipHorizontal,
    /// 垂直翻转
    FlipVertical,
    /// 反色
    Invert,
}

/// 视频变换配置
#[derive(Debug, Clone)]
pub struct VideoTransformConfig {
    /// 当前旋转角度 (0, 90, 180, 270)
    pub rotation_degrees: u32,
    /// 是否水平翻转
    pub flip_horizontal: bool,
    /// 是否垂直翻转
    pub flip_vertical: bool,
    /// 是否反色
    pub invert_colors: bool,
}

impl Default for VideoTransformConfig {
    fn default() -> Self {
        Self {
            rotation_degrees: 0,
            flip_horizontal: false,
            flip_vertical: false,
            invert_colors: false,
        }
    }
}

impl VideoTransformConfig {
    /// 创建新配置
    pub fn new() -> Self {
        Self::default()
    }

    /// 应用旋转（累加90度）
    pub fn apply_rotation(&mut self) {
        self.rotation_degrees = (self.rotation_degrees + 90) % 360;
        debug!("Rotation updated to: {}°", self.rotation_degrees);
    }

    /// 设置旋转角度
    pub fn set_rotation(&mut self, degrees: u32) {
        self.rotation_degrees = degrees % 360;
        debug!("Rotation set to: {}°", self.rotation_degrees);
    }

    /// 切换水平翻转
    pub fn toggle_flip_horizontal(&mut self) {
        self.flip_horizontal = !self.flip_horizontal;
        debug!("Horizontal flip: {}", self.flip_horizontal);
    }

    /// 切换垂直翻转
    pub fn toggle_flip_vertical(&mut self) {
        self.flip_vertical = !self.flip_vertical;
        debug!("Vertical flip: {}", self.flip_vertical);
    }

    /// 切换反色
    pub fn toggle_invert(&mut self) {
        self.invert_colors = !self.invert_colors;
        debug!("Color invert: {}", self.invert_colors);
    }

    /// 重置所有变换
    pub fn reset(&mut self) {
        self.rotation_degrees = 0;
        self.flip_horizontal = false;
        self.flip_vertical = false;
        self.invert_colors = false;
        info!("Video transform config reset");
    }

    /// 获取需要应用的变换列表
    pub fn get_transforms(&self) -> Vec<VideoTransform> {
        let mut transforms = Vec::new();

        // 添加旋转
        match self.rotation_degrees {
            90 => transforms.push(VideoTransform::Rotate90),
            180 => transforms.push(VideoTransform::Rotate180),
            270 => transforms.push(VideoTransform::Rotate270),
            _ => {}
        }

        // 添加翻转
        if self.flip_horizontal {
            transforms.push(VideoTransform::FlipHorizontal);
        }
        if self.flip_vertical {
            transforms.push(VideoTransform::FlipVertical);
        }

        // 添加反色
        if self.invert_colors {
            transforms.push(VideoTransform::Invert);
        }

        transforms
    }
}

/// 视频变换处理器
pub struct VideoTransformProcessor {
    rga_processor: Option<Arc<Mutex<RgaProcessor>>>,
    config: VideoTransformConfig,
}

impl VideoTransformProcessor {
    /// 创建新的处理器
    pub fn new() -> Self {
        // 尝试初始化RGA处理器
        let rga_processor = match RgaProcessor::new() {
            Ok(processor) => {
                info!("RGA hardware acceleration available for video transforms");
                Some(Arc::new(Mutex::new(processor)))
            }
            Err(e) => {
                warn!("RGA hardware not available, video transforms disabled: {:?}", e);
                None
            }
        };

        Self {
            rga_processor,
            config: VideoTransformConfig::default(),
        }
    }

    /// 获取当前配置
    pub fn get_config(&self) -> &VideoTransformConfig {
        &self.config
    }

    /// 获取可变配置
    pub fn get_config_mut(&mut self) -> &mut VideoTransformConfig {
        &mut self.config
    }

    /// 更新配置
    pub fn set_config(&mut self, config: VideoTransformConfig) {
        self.config = config;
    }

    /// 检查RGA是否可用
    pub fn is_rga_available(&self) -> bool {
        self.rga_processor.is_some()
    }

    /// 应用单个变换到图像数据
    pub fn apply_transform(
        &self,
        data: &[u8],
        width: u32,
        height: u32,
        format: RgaFormat,
        transform: VideoTransform,
    ) -> Result<Vec<u8>> {
        if let Some(ref rga) = self.rga_processor {
            let processor = rga.lock().map_err(|e| {
                SmartScopeError::CameraError(format!("Failed to lock RGA processor: {}", e))
            })?;

            // 创建RGA图像
            let image = RgaImage::from_data(data.to_vec(), width, height, format)
                .map_err(|e| SmartScopeError::CameraError(format!("Failed to create RGA image: {:?}", e)))?;

            // 应用变换
            let transformed = match transform {
                VideoTransform::Rotate90 => processor.rotate_90(&image),
                VideoTransform::Rotate180 => processor.rotate_180(&image),
                VideoTransform::Rotate270 => processor.rotate_270(&image),
                VideoTransform::FlipHorizontal => processor.flip_horizontal(&image),
                VideoTransform::FlipVertical => processor.flip_vertical(&image),
                VideoTransform::Invert => processor.invert(&image),
            }
            .map_err(|e| SmartScopeError::CameraError(format!("RGA transform failed: {:?}", e)))?;

            Ok(transformed.data().to_vec())
        } else {
            Err(SmartScopeError::CameraError(
                "RGA hardware not available".to_string(),
            ))
        }
    }

    /// 应用多个变换到图像数据
    pub fn apply_transforms(
        &self,
        data: &[u8],
        width: u32,
        height: u32,
        format: RgaFormat,
        transforms: &[VideoTransform],
    ) -> Result<Vec<u8>> {
        if transforms.is_empty() {
            return Ok(data.to_vec());
        }

        if let Some(ref rga) = self.rga_processor {
            let processor = rga.lock().map_err(|e| {
                SmartScopeError::CameraError(format!("Failed to lock RGA processor: {}", e))
            })?;

            // 创建初始RGA图像
            let mut image = RgaImage::from_data(data.to_vec(), width, height, format)
                .map_err(|e| SmartScopeError::CameraError(format!("Failed to create RGA image: {:?}", e)))?;

            // 依次应用所有变换
            for transform in transforms {
                image = match transform {
                    VideoTransform::Rotate90 => processor.rotate_90(&image),
                    VideoTransform::Rotate180 => processor.rotate_180(&image),
                    VideoTransform::Rotate270 => processor.rotate_270(&image),
                    VideoTransform::FlipHorizontal => processor.flip_horizontal(&image),
                    VideoTransform::FlipVertical => processor.flip_vertical(&image),
                    VideoTransform::Invert => processor.invert(&image),
                }
                .map_err(|e| SmartScopeError::CameraError(format!("RGA transform failed: {:?}", e)))?;
            }

            Ok(image.data().to_vec())
        } else {
            Err(SmartScopeError::CameraError(
                "RGA hardware not available".to_string(),
            ))
        }
    }

    /// 使用当前配置应用变换
    pub fn apply_config_transforms(
        &self,
        data: &[u8],
        width: u32,
        height: u32,
        format: RgaFormat,
    ) -> Result<Vec<u8>> {
        let transforms = self.config.get_transforms();
        self.apply_transforms(data, width, height, format, &transforms)
    }
}

impl Default for VideoTransformProcessor {
    fn default() -> Self {
        Self::new()
    }
}

/// 便捷函数：应用单个变换
pub fn apply_transform(
    data: &[u8],
    width: u32,
    height: u32,
    format: RgaFormat,
    transform: VideoTransform,
) -> Result<Vec<u8>> {
    let processor = VideoTransformProcessor::new();
    processor.apply_transform(data, width, height, format, transform)
}

/// 便捷函数：应用多个变换
pub fn apply_transforms(
    data: &[u8],
    width: u32,
    height: u32,
    format: RgaFormat,
    transforms: &[VideoTransform],
) -> Result<Vec<u8>> {
    let processor = VideoTransformProcessor::new();
    processor.apply_transforms(data, width, height, format, transforms)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_config_rotation() {
        let mut config = VideoTransformConfig::new();
        assert_eq!(config.rotation_degrees, 0);

        config.apply_rotation();
        assert_eq!(config.rotation_degrees, 90);

        config.apply_rotation();
        assert_eq!(config.rotation_degrees, 180);

        config.apply_rotation();
        assert_eq!(config.rotation_degrees, 270);

        config.apply_rotation();
        assert_eq!(config.rotation_degrees, 0);
    }

    #[test]
    fn test_config_toggles() {
        let mut config = VideoTransformConfig::new();

        config.toggle_flip_horizontal();
        assert_eq!(config.flip_horizontal, true);

        config.toggle_flip_vertical();
        assert_eq!(config.flip_vertical, true);

        config.toggle_invert();
        assert_eq!(config.invert_colors, true);

        config.reset();
        assert_eq!(config.rotation_degrees, 0);
        assert_eq!(config.flip_horizontal, false);
        assert_eq!(config.flip_vertical, false);
        assert_eq!(config.invert_colors, false);
    }

    #[test]
    fn test_get_transforms() {
        let mut config = VideoTransformConfig::new();

        // 无变换
        assert_eq!(config.get_transforms().len(), 0);

        // 单个旋转
        config.set_rotation(90);
        let transforms = config.get_transforms();
        assert_eq!(transforms.len(), 1);
        assert_eq!(transforms[0], VideoTransform::Rotate90);

        // 多个变换
        config.toggle_flip_horizontal();
        config.toggle_invert();
        let transforms = config.get_transforms();
        assert_eq!(transforms.len(), 3); // 旋转 + 翻转 + 反色
    }
}
