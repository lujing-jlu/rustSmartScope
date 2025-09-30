//! SmartScope 数据流管道模块
//!
//! 基于 Rockchip RGA 硬件加速的多级图像处理管道：
//! - 原始帧缓存 -> RGA硬件处理 -> 多级结果缓存
//! - 两条独立处理链：显示链（可变）+ 视差链（固定）
//! - 高效的帧数据消费防止积压
//! - 硬件加速的图像变换

use crate::error::{SmartScopeError, SmartScopeResult};
use crate::types::*;
use log::{debug, error, info, warn};
use std::collections::HashMap;
use std::sync::{mpsc, Arc, Mutex};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};
use usb_camera::VideoFrame;

// 总是导入 RGA 和 OpenCV
use rockchip_rga::{RgaImage, RgaProcessor, RgaTransform};
use opencv::{core, imgproc, prelude::*};

/// 图像处理阶段
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ProcessingStage {
    /// 原始帧（来自相机）
    Raw,
    /// 旋转后帧
    Rotated,
    /// 畸变校正后帧
    CorrectionApplied,
    /// 立体矫正后帧
    StereoRectified,
    /// 视差图
    DisparityMap,
    /// 显示变换后帧（可变）
    DisplayTransformed,
}

/// 显示变换类型 (映射到 Rockchip RGA)
#[derive(Debug, Clone)]
pub enum DisplayTransform {
    /// 水平翻转
    FlipHorizontal,
    /// 垂直翻转
    FlipVertical,
    /// 旋转90度
    Rotate90,
    /// 旋转180度
    Rotate180,
    /// 旋转270度
    Rotate270,
    /// 缩放到指定尺寸
    Scale(u32, u32),
    /// 按比例缩放
    ScaleRatio(f32, f32),
    /// 裁剪
    Crop(u32, u32, u32, u32),
    /// 反色
    Invert,
}

/// 处理后的帧数据
#[derive(Debug, Clone)]
pub struct ProcessedFrame {
    /// 帧数据
    pub data: Vec<u8>,
    /// 宽度
    pub width: u32,
    /// 高度
    pub height: u32,
    /// 像素格式
    pub format: ImageFormat,
    /// 处理阶段
    pub stage: ProcessingStage,
    /// 时间戳
    pub timestamp: u64,
    /// 相机类型
    pub camera_type: CameraType,
    /// 处理耗时（微秒）
    pub processing_time_us: u64,
}

/// 处理链配置
#[derive(Debug, Clone)]
pub struct ProcessingPipelineConfig {
    /// 是否启用旋转
    pub rotation_enabled: bool,
    /// 旋转角度（度）
    pub rotation_angle: f32,
    /// 是否启用畸变校正
    pub correction_enabled: bool,
    /// 是否启用立体矫正
    pub stereo_rectification_enabled: bool,
    /// 是否启用视差计算
    pub disparity_enabled: bool,
    /// 显示变换链（用于UI显示）
    pub display_transforms: Vec<DisplayTransform>,
    /// 视差计算质量设置
    pub disparity_quality: DisparityQuality,
}

impl Default for ProcessingPipelineConfig {
    fn default() -> Self {
        Self {
            rotation_enabled: true,
            rotation_angle: 0.0,
            correction_enabled: true,
            stereo_rectification_enabled: true,
            disparity_enabled: true,
            display_transforms: Vec::new(),
            disparity_quality: DisparityQuality::Medium,
        }
    }
}

/// 图像处理器类型（运行时检测）
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ProcessorType {
    /// 使用 Rockchip RGA 硬件加速
    RgaHardware,
    /// 使用 OpenCV 软件处理
    OpenCVSoftware,
}

/// 视差计算质量
#[derive(Debug, Clone)]
pub enum DisparityQuality {
    Low,    // 快速计算
    Medium, // 平衡
    High,   // 高质量
}

/// 帧缓存
#[derive(Debug)]
struct FrameCache {
    /// 缓存的帧
    frames: HashMap<CameraType, ProcessedFrame>,
    /// 最大缓存时间（毫秒）
    max_age_ms: u64,
    /// 上次清理时间
    last_cleanup: Instant,
}

impl FrameCache {
    fn new(max_age_ms: u64) -> Self {
        Self {
            frames: HashMap::new(),
            max_age_ms,
            last_cleanup: Instant::now(),
        }
    }

    /// 存储帧
    fn store_frame(&mut self, frame: ProcessedFrame) {
        self.frames.insert(frame.camera_type, frame);

        // 定期清理过期帧
        if self.last_cleanup.elapsed().as_millis() > 1000 {
            self.cleanup_expired_frames();
            self.last_cleanup = Instant::now();
        }
    }

    /// 获取最新帧
    fn get_latest_frame(&self, camera_type: CameraType) -> Option<&ProcessedFrame> {
        self.frames.get(&camera_type)
    }

    /// 清理过期帧
    fn cleanup_expired_frames(&mut self) {
        let now = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis() as u64;

        self.frames
            .retain(|_, frame| now - frame.timestamp < self.max_age_ms);
    }
}

/// 帧处理管道管理器 (基于 Rockchip RGA)
pub struct FrameProcessingPipeline {
    /// 各阶段缓存
    stage_caches: HashMap<ProcessingStage, Arc<Mutex<FrameCache>>>,
    /// 处理线程句柄
    processing_threads: Vec<JoinHandle<()>>,
    /// 管道配置
    config: Arc<Mutex<ProcessingPipelineConfig>>,
    /// 原始帧输入通道
    raw_frame_sender: mpsc::Sender<VideoFrame>,
    /// 停止标志
    stop_flag: Arc<Mutex<bool>>,
    /// 统计信息
    stats: Arc<Mutex<PipelineStats>>,
    /// 图像处理器类型（自动检测）
    processor_type: ProcessorType,
    /// Rockchip RGA 处理器（如果可用）
    rga_processor: Option<Arc<Mutex<RgaProcessor>>>,
}

/// 管道统计信息
#[derive(Debug, Clone)]
pub struct PipelineStats {
    /// 处理的总帧数
    pub total_frames_processed: u64,
    /// 丢弃的帧数
    pub frames_dropped: u64,
    /// 各阶段平均处理时间（微秒）
    pub avg_processing_times: HashMap<ProcessingStage, u64>,
    /// 当前FPS
    pub current_fps: f64,
    /// 最后更新时间
    pub last_update: Instant,
}

impl Default for PipelineStats {
    fn default() -> Self {
        Self {
            total_frames_processed: 0,
            frames_dropped: 0,
            avg_processing_times: HashMap::new(),
            current_fps: 0.0,
            last_update: Instant::now(),
        }
    }
}

impl FrameProcessingPipeline {
    /// 创建新的处理管道
    pub fn new(config: ProcessingPipelineConfig) -> SmartScopeResult<Self> {
        info!("Creating frame processing pipeline");

        // 创建各阶段缓存
        let mut stage_caches = HashMap::new();
        let cache_stages = [
            ProcessingStage::Raw,
            ProcessingStage::Rotated,
            ProcessingStage::CorrectionApplied,
            ProcessingStage::StereoRectified,
            ProcessingStage::DisparityMap,
            ProcessingStage::DisplayTransformed,
        ];

        for stage in &cache_stages {
            stage_caches.insert(*stage, Arc::new(Mutex::new(FrameCache::new(1000))));
            // 1秒缓存
        }

        let (raw_frame_sender, raw_frame_receiver) = mpsc::channel();
        let config = Arc::new(Mutex::new(config));
        let stop_flag = Arc::new(Mutex::new(false));
        let stats = Arc::new(Mutex::new(PipelineStats::default()));

        // 自动检测处理器类型和创建 RGA 处理器
        let (processor_type, rga_processor) = Self::detect_processor_capabilities();

        let mut pipeline = Self {
            stage_caches,
            processing_threads: Vec::new(),
            config,
            raw_frame_sender,
            stop_flag,
            stats,
            processor_type,
            rga_processor,
        };

        // 启动处理线程
        pipeline.start_processing_threads(raw_frame_receiver)?;

        info!("Frame processing pipeline created successfully");
        Ok(pipeline)
    }

    /// 自动检测处理器能力并选择最佳处理方式
    fn detect_processor_capabilities() -> (ProcessorType, Option<Arc<Mutex<RgaProcessor>>>) {
        // 尝试初始化 Rockchip RGA
        match RgaProcessor::new() {
            Ok(processor) => {
                info!("Rockchip RGA hardware acceleration available");
                (
                    ProcessorType::RgaHardware,
                    Some(Arc::new(Mutex::new(processor))),
                )
            }
            Err(e) => {
                info!(
                    "Rockchip RGA not available, falling back to OpenCV: {}",
                    e
                );
                (ProcessorType::OpenCVSoftware, None)
            }
        }
    }

    /// 尝试创建 Rockchip RGA 处理器（废弃函数，保留兼容）
    fn try_create_rga_processor() -> SmartScopeResult<RgaProcessor> {
        match RgaProcessor::new() {
            Ok(processor) => Ok(processor),
            Err(e) => Err(SmartScopeError::CameraError(format!(
                "Failed to create RGA processor: {:?}",
                e
            ))),
        }
    }

    /// 启动处理线程
    fn start_processing_threads(
        &mut self,
        raw_receiver: mpsc::Receiver<VideoFrame>,
    ) -> SmartScopeResult<()> {
        let stage_caches = self.stage_caches.clone();
        let config = Arc::clone(&self.config);
        let stop_flag = Arc::clone(&self.stop_flag);
        let stats = Arc::clone(&self.stats);
        let processor_type = self.processor_type;
        let rga_processor = self.rga_processor.clone();

        // 主处理线程
        let processing_thread = thread::spawn(move || {
            Self::main_processing_loop(
                raw_receiver,
                stage_caches,
                config,
                stop_flag,
                stats,
                processor_type,
                rga_processor,
            );
        });

        self.processing_threads.push(processing_thread);
        info!("Frame processing threads started");
        Ok(())
    }

    /// 主处理循环 (自动检测处理器)
    fn main_processing_loop(
        raw_receiver: mpsc::Receiver<VideoFrame>,
        stage_caches: HashMap<ProcessingStage, Arc<Mutex<FrameCache>>>,
        config: Arc<Mutex<ProcessingPipelineConfig>>,
        stop_flag: Arc<Mutex<bool>>,
        stats: Arc<Mutex<PipelineStats>>,
        processor_type: ProcessorType,
        rga_processor: Option<Arc<Mutex<RgaProcessor>>>,
    ) {
        info!("Frame processing loop started");

        while !*stop_flag.lock().unwrap() {
            // 接收原始帧
            match raw_receiver.recv_timeout(Duration::from_millis(10)) {
                Ok(raw_frame) => {
                    debug!("Processing frame from {} camera", raw_frame.camera_name);

                    // 将VideoFrame转换为ProcessedFrame
                    let processed_frame = ProcessedFrame {
                        data: raw_frame.data.clone(),
                        width: raw_frame.width,
                        height: raw_frame.height,
                        format: ImageFormat::MJPEG,
                        stage: ProcessingStage::Raw,
                        timestamp: raw_frame
                            .timestamp
                            .duration_since(std::time::UNIX_EPOCH)
                            .unwrap_or_default()
                            .as_millis() as u64,
                        camera_type: if raw_frame.camera_name.contains("L")
                            || raw_frame.camera_name.contains("left")
                        {
                            CameraType::Left
                        } else {
                            CameraType::Right
                        },
                        processing_time_us: 0,
                    };

                    // 存储原始帧
                    Self::cache_frame(&stage_caches, ProcessingStage::Raw, &processed_frame);

                    // 使用自动检测的处理器处理管道
                    Self::process_frame_auto(
                        processed_frame,
                        &stage_caches,
                        &config,
                        processor_type,
                        &rga_processor,
                        &stats,
                    );

                    // 更新统计信息
                    Self::update_stats(&stats);
                }
                Err(mpsc::RecvTimeoutError::Timeout) => {
                    // 超时，继续循环检查停止标志
                    continue;
                }
                Err(mpsc::RecvTimeoutError::Disconnected) => {
                    warn!("Raw frame channel disconnected");
                    break;
                }
            }
        }

        info!("Frame processing loop stopped");
    }

    /// 使用自动检测的处理器处理帧数据
    fn process_frame_auto(
        frame: ProcessedFrame,
        stage_caches: &HashMap<ProcessingStage, Arc<Mutex<FrameCache>>>,
        config: &Arc<Mutex<ProcessingPipelineConfig>>,
        processor_type: ProcessorType,
        rga_processor: &Option<Arc<Mutex<RgaProcessor>>>,
        _stats: &Arc<Mutex<PipelineStats>>,
    ) {
        let config_guard = config.lock().unwrap();
        let processing_config = config_guard.clone();
        drop(config_guard);

        // 分为两条处理链：
        // 1. 视差计算链 (固定): 原始 -> 旋转 -> 畸变校正 -> 立体矫正 -> 视差
        // 2. 显示链 (可变): 原始 -> 用户自定义变换

        // === 视差计算链 (固定处理链) ===
        let mut disparity_frame = frame.clone();

        // 1. 旋转处理 (用于视差计算)
        if processing_config.rotation_enabled {
            disparity_frame = Self::apply_rga_rotation(
                &disparity_frame,
                processing_config.rotation_angle,
                rga_processor,
            );
            Self::cache_frame(stage_caches, ProcessingStage::Rotated, &disparity_frame);
        }

        // 2. 畸变校正 (TODO: 集成 camera-correction crate)
        if processing_config.correction_enabled {
            disparity_frame = Self::apply_camera_correction(disparity_frame);
            Self::cache_frame(
                stage_caches,
                ProcessingStage::CorrectionApplied,
                &disparity_frame,
            );
        }

        // 3. 立体矫正 (TODO: 集成 stereo-rectifier crate)
        if processing_config.stereo_rectification_enabled {
            disparity_frame = Self::apply_stereo_rectification(disparity_frame);
            Self::cache_frame(
                stage_caches,
                ProcessingStage::StereoRectified,
                &disparity_frame,
            );
        }

        // 4. 视差计算 (TODO: 实现立体匹配算法)
        if processing_config.disparity_enabled {
            if let Some(disparity_result) =
                Self::calculate_disparity_map(disparity_frame, &processing_config)
            {
                Self::cache_frame(
                    stage_caches,
                    ProcessingStage::DisparityMap,
                    &disparity_result,
                );
            }
        }

        // === 显示链 (可变处理链) ===
        if !processing_config.display_transforms.is_empty() {
            let display_frame = Self::apply_display_transforms_auto(
                frame,
                &processing_config.display_transforms,
                processor_type,
                rga_processor,
            );
            Self::cache_frame(
                stage_caches,
                ProcessingStage::DisplayTransformed,
                &display_frame,
            );
        }
    }

    /// 统计信息更新
    fn update_stats(stats: &Arc<Mutex<PipelineStats>>) {
        if let Ok(mut stats_guard) = stats.lock() {
            stats_guard.total_frames_processed += 1;

            // 计算FPS
            let elapsed = stats_guard.last_update.elapsed();
            if elapsed.as_millis() > 1000 {
                stats_guard.current_fps =
                    stats_guard.total_frames_processed as f64 / elapsed.as_secs_f64();
                stats_guard.last_update = Instant::now();
            }
        }
    }

    /// 缓存帧
    fn cache_frame(
        stage_caches: &HashMap<ProcessingStage, Arc<Mutex<FrameCache>>>,
        stage: ProcessingStage,
        frame: &ProcessedFrame,
    ) {
        if let Some(cache) = stage_caches.get(&stage) {
            if let Ok(mut cache_guard) = cache.lock() {
                cache_guard.store_frame(frame.clone());
            }
        }
    }

    /// 使用 RGA 应用旋转 (用于视差计算链)
    fn apply_rga_rotation(
        frame: &ProcessedFrame,
        angle: f32,
        rga_processor: &Option<Arc<Mutex<RgaProcessor>>>,
    ) -> ProcessedFrame {
        let start_time = Instant::now();

        match rga_processor {
            Some(rga) => {
                if let Ok(_processor) = rga.lock() {
                    // 暂时跳过实际RGA处理，因为需要格式转换
                    debug!(
                        "RGA rotation: {} degrees for {:?} frame",
                        angle, frame.camera_type
                    );
                    // TODO: 实现MJPEG -> RGB -> RGA处理 -> 结果
                }
            }
            None => {
                debug!(
                    "Software rotation: {} degrees for {:?} frame (RGA not available)",
                    angle, frame.camera_type
                );
                // TODO: 软件旋转fallback
            }
        }

        let mut result = frame.clone();
        result.stage = ProcessingStage::Rotated;
        result.processing_time_us = start_time.elapsed().as_micros() as u64;
        result
    }

    /// 应用相机校正 (集成 camera-correction crate)
    fn apply_camera_correction(mut frame: ProcessedFrame) -> ProcessedFrame {
        let start_time = Instant::now();

        debug!(
            "Applying camera correction for {:?} frame",
            frame.camera_type
        );
        // TODO: 集成 crates/camera-correction

        frame.stage = ProcessingStage::CorrectionApplied;
        frame.processing_time_us = start_time.elapsed().as_micros() as u64;
        frame
    }

    /// 应用立体矫正 (集成 stereo-rectifier crate)
    fn apply_stereo_rectification(mut frame: ProcessedFrame) -> ProcessedFrame {
        let start_time = Instant::now();

        debug!(
            "Applying stereo rectification for {:?} frame",
            frame.camera_type
        );
        // TODO: 集成 crates/stereo-rectifier

        frame.stage = ProcessingStage::StereoRectified;
        frame.processing_time_us = start_time.elapsed().as_micros() as u64;
        frame
    }

    /// 计算视差图 (实现立体匹配算法)
    fn calculate_disparity_map(
        _frame: ProcessedFrame,
        _config: &ProcessingPipelineConfig,
    ) -> Option<ProcessedFrame> {
        debug!("Calculating disparity map");
        // TODO: 实现双目立体匹配算法
        // 1. 等待左右相机帧对齐
        // 2. 使用OpenCV或自定义算法计算视差
        // 3. 返回视差图
        None
    }

    /// 自动选择处理器应用显示变换 (用于UI显示链)
    fn apply_display_transforms_auto(
        frame: ProcessedFrame,
        transforms: &[DisplayTransform],
        processor_type: ProcessorType,
        rga_processor: &Option<Arc<Mutex<RgaProcessor>>>,
    ) -> ProcessedFrame {
        match processor_type {
            ProcessorType::RgaHardware => {
                Self::apply_rga_display_transforms(frame, transforms, rga_processor)
            }
            ProcessorType::OpenCVSoftware => {
                Self::apply_opencv_display_transforms(frame, transforms)
            }
        }
    }

    /// 使用 RGA 应用显示变换 (用于UI显示链)
    fn apply_rga_display_transforms(
        frame: ProcessedFrame,
        transforms: &[DisplayTransform],
        rga_processor: &Option<Arc<Mutex<RgaProcessor>>>,
    ) -> ProcessedFrame {
        let start_time = Instant::now();

        match rga_processor {
            Some(rga) => {
                if let Ok(_processor) = rga.lock() {
                    debug!(
                        "Applying RGA display transforms: {} operations",
                        transforms.len()
                    );
                    // TODO: 实现RGA显示变换
                    for transform in transforms {
                        match transform {
                            DisplayTransform::FlipHorizontal => {
                                debug!("RGA horizontal flip");
                                // processor.flip_horizontal(&rga_image)?
                            }
                            DisplayTransform::FlipVertical => {
                                debug!("RGA vertical flip");
                                // processor.flip_vertical(&rga_image)?
                            }
                            DisplayTransform::Rotate90 => {
                                debug!("RGA rotate 90°");
                                // processor.rotate_90(&rga_image)?
                            }
                            DisplayTransform::Rotate180 => {
                                debug!("RGA rotate 180°");
                                // processor.rotate_180(&rga_image)?
                            }
                            DisplayTransform::Rotate270 => {
                                debug!("RGA rotate 270°");
                                // processor.rotate_270(&rga_image)?
                            }
                            DisplayTransform::Scale(w, h) => {
                                debug!("RGA scale to {}x{}", w, h);
                                // processor.scale(&rga_image, *w, *h)?
                            }
                            DisplayTransform::ScaleRatio(rx, ry) => {
                                debug!("RGA scale ratio {}x{}", rx, ry);
                                // processor.transform(&rga_image, &RgaTransform::ScaleRatio(*rx, *ry))?
                            }
                            DisplayTransform::Crop(x, y, w, h) => {
                                debug!("RGA crop {}x{} at ({}, {})", w, h, x, y);
                                // processor.crop(&rga_image, *x, *y, *w, *h)?
                            }
                            DisplayTransform::Invert => {
                                debug!("RGA color invert");
                                // processor.invert(&rga_image)?
                            }
                        }
                    }
                }
            }
            None => {
                debug!(
                    "Software display transforms: {} operations (RGA not available)",
                    transforms.len()
                );
                // TODO: 软件变换fallback
            }
        }

        let mut result = frame.clone();
        result.stage = ProcessingStage::DisplayTransformed;
        result.processing_time_us = start_time.elapsed().as_micros() as u64;
        result
    }

    /// 使用 OpenCV 应用显示变换 (软件备选方案)
    fn apply_opencv_display_transforms(
        frame: ProcessedFrame,
        transforms: &[DisplayTransform],
    ) -> ProcessedFrame {
        let start_time = Instant::now();
        debug!(
            "Applying OpenCV display transforms: {} operations",
            transforms.len()
        );

        // TODO: 实现OpenCV显示变换
        for transform in transforms {
            match transform {
                DisplayTransform::FlipHorizontal => {
                    debug!("OpenCV horizontal flip");
                    // 使用 OpenCV flip 函数
                    // cv::flip(src, dst, 1);
                }
                DisplayTransform::FlipVertical => {
                    debug!("OpenCV vertical flip");
                    // cv::flip(src, dst, 0);
                }
                DisplayTransform::Rotate90 => {
                    debug!("OpenCV rotate 90°");
                    // cv::rotate(src, dst, cv::ROTATE_90_CLOCKWISE);
                }
                DisplayTransform::Rotate180 => {
                    debug!("OpenCV rotate 180°");
                    // cv::rotate(src, dst, cv::ROTATE_180);
                }
                DisplayTransform::Rotate270 => {
                    debug!("OpenCV rotate 270°");
                    // cv::rotate(src, dst, cv::ROTATE_90_COUNTERCLOCKWISE);
                }
                DisplayTransform::Scale(width, height) => {
                    debug!("OpenCV scale to {}x{}", width, height);
                    // cv::resize(src, dst, cv::Size(width, height));
                }
                DisplayTransform::ScaleRatio(x_ratio, y_ratio) => {
                    debug!("OpenCV scale by ratio {}x{}", x_ratio, y_ratio);
                    // let new_width = (image.width() as f32 * x_ratio) as i32;
                    // let new_height = (image.height() as f32 * y_ratio) as i32;
                    // cv::resize(src, dst, cv::Size(new_width, new_height));
                }
                DisplayTransform::Crop(x, y, width, height) => {
                    debug!("OpenCV crop {}x{} at ({}, {})", width, height, x, y);
                    // cv::Rect roi(x, y, width, height);
                    // dst = src(roi);
                }
                DisplayTransform::Invert => {
                    debug!("OpenCV color invert");
                    // cv::bitwise_not(src, dst);
                }
            }
        }

        let mut result = frame.clone();
        result.stage = ProcessingStage::DisplayTransformed;
        result.processing_time_us = start_time.elapsed().as_micros() as u64;
        result
    }

    /// 输入原始帧
    pub fn input_raw_frame(&self, frame: VideoFrame) -> SmartScopeResult<()> {
        self.raw_frame_sender
            .send(frame)
            .map_err(|e| SmartScopeError::CameraError(format!("Failed to input raw frame: {}", e)))
    }

    /// 获取指定阶段的帧
    pub fn get_frame(
        &self,
        stage: ProcessingStage,
        camera_type: CameraType,
    ) -> Option<ProcessedFrame> {
        if let Some(cache) = self.stage_caches.get(&stage) {
            if let Ok(cache_guard) = cache.lock() {
                return cache_guard.get_latest_frame(camera_type).cloned();
            }
        }
        None
    }

    /// 更新配置
    pub fn update_config(&self, new_config: ProcessingPipelineConfig) {
        if let Ok(mut config_guard) = self.config.lock() {
            *config_guard = new_config;
            info!("Pipeline configuration updated");
        }
    }

    /// 获取统计信息
    pub fn get_stats(&self) -> PipelineStats {
        self.stats.lock().unwrap().clone()
    }

    /// 停止管道
    pub fn stop(&mut self) {
        info!("Stopping frame processing pipeline");

        // 设置停止标志
        if let Ok(mut stop_flag) = self.stop_flag.lock() {
            *stop_flag = true;
        }

        // 等待所有处理线程结束
        while let Some(handle) = self.processing_threads.pop() {
            if let Err(e) = handle.join() {
                error!("Failed to join processing thread: {:?}", e);
            }
        }

        info!("Frame processing pipeline stopped");
    }
}

impl Drop for FrameProcessingPipeline {
    fn drop(&mut self) {
        self.stop();
    }
}
