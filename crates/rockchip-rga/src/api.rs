use crate::{buffer::RgaFormat, context::RgaContext, error::RgaError};
use image::ImageReader;
use std::path::Path;

/// RGA图像变换类型
#[derive(Debug, Clone)]
pub enum RgaTransform {
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
    /// 自定义角度旋转 (度数)
    Rotate(f32),
    /// 缩放 (宽, 高)
    Scale(u32, u32),
    /// 缩放比例 (宽比例, 高比例)
    ScaleRatio(f32, f32),
    /// 裁剪 (x, y, 宽, 高)
    Crop(u32, u32, u32, u32),
    /// 反色
    Invert,
    /// 组合变换
    Composite(Vec<RgaTransform>),
}

/// RGA图像处理器
pub struct RgaProcessor {
    context: RgaContext,
}

impl RgaProcessor {
    /// 创建新的RGA处理器
    pub fn new() -> Result<Self, RgaError> {
        let context = RgaContext::new()?;
        Ok(Self { context })
    }

    /// 从文件加载图像
    pub fn load_image<P: AsRef<Path>>(&self, path: P) -> Result<RgaImage, RgaError> {
        RgaImage::from_file(path)
    }

    /// 从内存数据创建图像
    pub fn create_image(
        &self,
        data: Vec<u8>,
        width: u32,
        height: u32,
        format: RgaFormat,
    ) -> Result<RgaImage, RgaError> {
        RgaImage::from_data(data, width, height, format)
    }

    /// 应用变换到图像
    pub fn transform(
        &self,
        image: &RgaImage,
        transform: &RgaTransform,
    ) -> Result<RgaImage, RgaError> {
        match transform {
            RgaTransform::Rotate90 => self.rotate_90(image),
            RgaTransform::Rotate180 => self.rotate_180(image),
            RgaTransform::Rotate270 => self.rotate_270(image),
            RgaTransform::FlipHorizontal => self.flip_horizontal(image),
            RgaTransform::FlipVertical => self.flip_vertical(image),
            RgaTransform::Rotate(angle) => self.rotate_angle(image, *angle),
            RgaTransform::Scale(width, height) => self.scale(image, *width, *height),
            RgaTransform::ScaleRatio(ratio_x, ratio_y) => {
                let new_width = (image.width() as f32 * ratio_x) as u32;
                let new_height = (image.height() as f32 * ratio_y) as u32;
                self.scale(image, new_width, new_height)
            }
            RgaTransform::Crop(x, y, width, height) => self.crop(image, *x, *y, *width, *height),
            RgaTransform::Invert => self.invert(image),
            RgaTransform::Composite(transforms) => self.apply_composite(image, transforms),
        }
    }

    /// 批量应用变换
    pub fn batch_transform(
        &self,
        image: &RgaImage,
        transforms: &[RgaTransform],
    ) -> Result<RgaImage, RgaError> {
        let mut result = image.clone();
        for transform in transforms {
            result = self.transform(&result, transform)?;
        }
        Ok(result)
    }

    /// 旋转90度
    pub fn rotate_90(&self, image: &RgaImage) -> Result<RgaImage, RgaError> {
        let (new_width, new_height) = (image.height(), image.width());
        let result = RgaImage::new(new_width, new_height, image.format());

        unsafe {
            let src = image.to_rga_buffer()?;
            let dst = result.to_rga_buffer()?;

            let status =
                crate::imrotate_t(src, dst, crate::IM_USAGE_IM_HAL_TRANSFORM_ROT_90 as i32, 1);

            if status != crate::IM_STATUS_IM_STATUS_SUCCESS {
                return Err(RgaError::RgaError(status));
            }
        }

        Ok(result)
    }

    /// 旋转180度
    pub fn rotate_180(&self, image: &RgaImage) -> Result<RgaImage, RgaError> {
        let result = RgaImage::new(image.width(), image.height(), image.format());

        unsafe {
            let src = image.to_rga_buffer()?;
            let dst = result.to_rga_buffer()?;

            let status =
                crate::imrotate_t(src, dst, crate::IM_USAGE_IM_HAL_TRANSFORM_ROT_180 as i32, 1);

            if status != crate::IM_STATUS_IM_STATUS_SUCCESS {
                return Err(RgaError::RgaError(status));
            }
        }

        Ok(result)
    }

    /// 旋转270度
    pub fn rotate_270(&self, image: &RgaImage) -> Result<RgaImage, RgaError> {
        let (new_width, new_height) = (image.height(), image.width());
        let result = RgaImage::new(new_width, new_height, image.format());

        unsafe {
            let src = image.to_rga_buffer()?;
            let dst = result.to_rga_buffer()?;

            let status =
                crate::imrotate_t(src, dst, crate::IM_USAGE_IM_HAL_TRANSFORM_ROT_270 as i32, 1);

            if status != crate::IM_STATUS_IM_STATUS_SUCCESS {
                return Err(RgaError::RgaError(status));
            }
        }

        Ok(result)
    }

    /// 水平翻转
    pub fn flip_horizontal(&self, image: &RgaImage) -> Result<RgaImage, RgaError> {
        let result = RgaImage::new(image.width(), image.height(), image.format());

        unsafe {
            let src = image.to_rga_buffer()?;
            let dst = result.to_rga_buffer()?;

            let status =
                crate::imflip_t(src, dst, crate::IM_USAGE_IM_HAL_TRANSFORM_FLIP_H as i32, 1);

            if status != crate::IM_STATUS_IM_STATUS_SUCCESS {
                return Err(RgaError::RgaError(status));
            }
        }

        Ok(result)
    }

    /// 垂直翻转
    pub fn flip_vertical(&self, image: &RgaImage) -> Result<RgaImage, RgaError> {
        let result = RgaImage::new(image.width(), image.height(), image.format());

        unsafe {
            let src = image.to_rga_buffer()?;
            let dst = result.to_rga_buffer()?;

            let status =
                crate::imflip_t(src, dst, crate::IM_USAGE_IM_HAL_TRANSFORM_FLIP_V as i32, 1);

            if status != crate::IM_STATUS_IM_STATUS_SUCCESS {
                return Err(RgaError::RgaError(status));
            }
        }

        Ok(result)
    }

    /// 自定义角度旋转
    pub fn rotate_angle(&self, image: &RgaImage, angle: f32) -> Result<RgaImage, RgaError> {
        // 标准化角度到0-360度
        let normalized_angle = ((angle % 360.0) + 360.0) % 360.0;

        match normalized_angle {
            0.0 => Ok(image.clone()),
            90.0 => self.rotate_90(image),
            180.0 => self.rotate_180(image),
            270.0 => self.rotate_270(image),
            _ => Err(RgaError::InvalidParameter(format!(
                "不支持的角度: {}",
                angle
            ))),
        }
    }

    /// 缩放图像
    pub fn scale(
        &self,
        image: &RgaImage,
        new_width: u32,
        new_height: u32,
    ) -> Result<RgaImage, RgaError> {
        let result = RgaImage::new(new_width, new_height, image.format());

        unsafe {
            let src = image.to_rga_buffer()?;
            let dst = result.to_rga_buffer()?;

            // 先进行参数校验
            let empty_buffer = std::mem::zeroed::<crate::rga_buffer_t>();
            let empty_rect = std::mem::zeroed::<crate::im_rect>();

            let check_status = crate::imcheck_t(
                src,
                dst,
                empty_buffer,
                empty_rect,
                empty_rect,
                empty_rect,
                0,
            );

            if check_status != crate::IM_STATUS_IM_STATUS_NOERROR {
                return Err(RgaError::RgaError(check_status));
            }

            // 执行缩放
            let status = crate::imresize_t(
                src,
                dst,
                1.0, // fx
                1.0, // fy
                crate::IM_INTER_MODE_IM_INTERP_LINEAR as i32,
                1,
            );

            if status != crate::IM_STATUS_IM_STATUS_SUCCESS {
                return Err(RgaError::RgaError(status));
            }
        }

        Ok(result)
    }

    /// 裁剪图像
    pub fn crop(
        &self,
        image: &RgaImage,
        x: u32,
        y: u32,
        width: u32,
        height: u32,
    ) -> Result<RgaImage, RgaError> {
        if x + width > image.width() || y + height > image.height() {
            return Err(RgaError::InvalidParameter(
                "裁剪区域超出图像边界".to_string(),
            ));
        }

        let result = RgaImage::new(width, height, image.format());

        unsafe {
            let src = image.to_rga_buffer()?;
            let dst = result.to_rga_buffer()?;

            let status = crate::imcrop_t(
                src,
                dst,
                crate::im_rect {
                    x: x as i32,
                    y: y as i32,
                    width: width as i32,
                    height: height as i32,
                },
                1,
            );

            if status != crate::IM_STATUS_IM_STATUS_SUCCESS {
                return Err(RgaError::RgaError(status));
            }
        }

        Ok(result)
    }

    /// 反色 (软件实现)
    pub fn invert(&self, image: &RgaImage) -> Result<RgaImage, RgaError> {
        let mut result = image.clone();

        // 软件反色实现
        for pixel in result.data_mut().chunks_mut(3) {
            pixel[0] = 255 - pixel[0]; // R
            pixel[1] = 255 - pixel[1]; // G
            pixel[2] = 255 - pixel[2]; // B
        }

        Ok(result)
    }

    /// 应用组合变换
    fn apply_composite(
        &self,
        image: &RgaImage,
        transforms: &[RgaTransform],
    ) -> Result<RgaImage, RgaError> {
        let mut result = image.clone();
        for transform in transforms {
            result = self.transform(&result, transform)?;
        }
        Ok(result)
    }
}

/// RGA图像
#[derive(Clone)]
pub struct RgaImage {
    data: Vec<u8>,
    width: u32,
    height: u32,
    format: RgaFormat,
}

impl RgaImage {
    /// 创建新图像
    pub fn new(width: u32, height: u32, format: RgaFormat) -> Self {
        let size = match format {
            RgaFormat::Rgb888 => width * height * 3,
            RgaFormat::Rgba8888 => width * height * 4,
            _ => width * height * 3, // 默认RGB
        };

        Self {
            data: vec![0; size as usize],
            width,
            height,
            format,
        }
    }

    /// 从文件加载图像
    pub fn from_file<P: AsRef<Path>>(path: P) -> Result<Self, RgaError> {
        let img = ImageReader::open(path)
            .map_err(|e| RgaError::IoError(e.into()))?
            .decode()
            .map_err(|e| RgaError::Unknown(format!("图像解码失败: {}", e)))?;

        let rgb_img = img.to_rgb8();
        let (width, height) = rgb_img.dimensions();

        Ok(Self {
            data: rgb_img.into_raw(),
            width,
            height,
            format: RgaFormat::Rgb888,
        })
    }

    /// 从内存数据创建图像
    pub fn from_data(
        data: Vec<u8>,
        width: u32,
        height: u32,
        format: RgaFormat,
    ) -> Result<Self, RgaError> {
        let expected_size = match format {
            RgaFormat::Rgb888 => width * height * 3,
            RgaFormat::Rgba8888 => width * height * 4,
            _ => width * height * 3,
        };

        if data.len() != expected_size as usize {
            return Err(RgaError::InvalidParameter(format!(
                "数据大小不匹配: 期望 {}, 实际 {}",
                expected_size,
                data.len()
            )));
        }

        Ok(Self {
            data,
            width,
            height,
            format,
        })
    }

    /// 获取图像宽度
    pub fn width(&self) -> u32 {
        self.width
    }

    /// 获取图像高度
    pub fn height(&self) -> u32 {
        self.height
    }

    /// 获取图像格式
    pub fn format(&self) -> RgaFormat {
        self.format
    }

    /// 获取图像数据
    pub fn data(&self) -> &[u8] {
        &self.data
    }

    /// 获取可变图像数据
    pub fn data_mut(&mut self) -> &mut [u8] {
        &mut self.data
    }

    /// 保存图像到文件
    pub fn save<P: AsRef<Path>>(&self, path: P) -> Result<(), RgaError> {
        use image::{ImageBuffer, Rgb};

        let img: ImageBuffer<Rgb<u8>, Vec<u8>> =
            ImageBuffer::from_raw(self.width, self.height, self.data.clone())
                .ok_or_else(|| RgaError::InvalidParameter("无法创建图像缓冲区".to_string()))?;

        img.save(path)
            .map_err(|e| RgaError::Unknown(format!("保存图像失败: {}", e)))?;

        Ok(())
    }

    /// 转换为RGA缓冲区
    fn to_rga_buffer(&self) -> Result<crate::rga_buffer_t, RgaError> {
        let stride = self.align_stride(self.width);
        let height_stride = self.align_stride(self.height);

        unsafe {
            Ok(crate::wrapbuffer_virtualaddr_t(
                self.data.as_ptr() as *mut std::ffi::c_void,
                self.width as i32,
                self.height as i32,
                stride as i32,
                height_stride as i32,
                self.format.to_rga_format() as i32,
            ))
        }
    }

    /// 16字节对齐的步长
    fn align_stride(&self, val: u32) -> u32 {
        (val + 15) & !15
    }
}

impl std::fmt::Debug for RgaImage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("RgaImage")
            .field("width", &self.width)
            .field("height", &self.height)
            .field("format", &self.format)
            .field("data_len", &self.data.len())
            .finish()
    }
}

/// 便捷的变换函数
impl RgaImage {
    /// 旋转90度
    pub fn rotate_90(&self) -> Result<Self, RgaError> {
        let processor = RgaProcessor::new()?;
        processor.rotate_90(self)
    }

    /// 旋转180度
    pub fn rotate_180(&self) -> Result<Self, RgaError> {
        let processor = RgaProcessor::new()?;
        processor.rotate_180(self)
    }

    /// 旋转270度
    pub fn rotate_270(&self) -> Result<Self, RgaError> {
        let processor = RgaProcessor::new()?;
        processor.rotate_270(self)
    }

    /// 水平翻转
    pub fn flip_horizontal(&self) -> Result<Self, RgaError> {
        let processor = RgaProcessor::new()?;
        processor.flip_horizontal(self)
    }

    /// 垂直翻转
    pub fn flip_vertical(&self) -> Result<Self, RgaError> {
        let processor = RgaProcessor::new()?;
        processor.flip_vertical(self)
    }

    /// 缩放
    pub fn scale(&self, new_width: u32, new_height: u32) -> Result<Self, RgaError> {
        let processor = RgaProcessor::new()?;
        processor.scale(self, new_width, new_height)
    }

    /// 按比例缩放
    pub fn scale_ratio(&self, ratio_x: f32, ratio_y: f32) -> Result<Self, RgaError> {
        let new_width = (self.width() as f32 * ratio_x) as u32;
        let new_height = (self.height() as f32 * ratio_y) as u32;
        self.scale(new_width, new_height)
    }

    /// 裁剪
    pub fn crop(&self, x: u32, y: u32, width: u32, height: u32) -> Result<Self, RgaError> {
        let processor = RgaProcessor::new()?;
        processor.crop(self, x, y, width, height)
    }

    /// 反色
    pub fn invert(&self) -> Result<Self, RgaError> {
        let processor = RgaProcessor::new()?;
        processor.invert(self)
    }
}
