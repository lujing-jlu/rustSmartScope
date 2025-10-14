use crate::{ImageBuffer, Result, Yolov8Detector};
use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, AtomicU64, AtomicUsize, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::thread;
use std::time::{Duration, Instant};

/// 推理任务
pub struct InferenceTask {
    pub task_id: u64,
    pub image: ImageBuffer,
    pub callback: Box<dyn FnOnce(Result<Vec<crate::DetectionResult>>) + Send>,
}

/// 推理结果（内部使用）
#[derive(Debug)]
struct InferenceResult {
    task_id: u64,
    result: Result<Vec<crate::DetectionResult>>,
}

/// 任务队列 - 防止堆积
pub struct TaskQueue {
    queue: Arc<Mutex<Vec<InferenceTask>>>,
    max_size: usize,
    dropped_count: Arc<AtomicUsize>,
}

impl TaskQueue {
    pub fn new(max_size: usize) -> Self {
        Self {
            queue: Arc::new(Mutex::new(Vec::new())),
            max_size,
            dropped_count: Arc::new(AtomicUsize::new(0)),
        }
    }

    /// 推送任务到队列
    pub fn push(&self, task: InferenceTask) -> Result<()> {
        let mut queue = self.queue.lock().unwrap();

        // 如果队列已满，丢弃最旧的任务
        if queue.len() >= self.max_size {
            queue.remove(0);
            self.dropped_count.fetch_add(1, Ordering::Relaxed);
          }

        queue.push(task);
        Ok(())
    }

    /// 从队列中弹出任务
    pub fn pop(&self) -> Option<InferenceTask> {
        let mut queue = self.queue.lock().unwrap();
        if queue.is_empty() {
            None
        } else {
            Some(queue.remove(0))
        }
    }

    /// 获取队列大小
    pub fn len(&self) -> usize {
        self.queue.lock().unwrap().len()
    }

    /// 获取丢弃的任务数
    pub fn dropped_count(&self) -> usize {
        self.dropped_count.load(Ordering::Relaxed)
    }
}

/// 顺序跟踪器 - 确保结果按输入顺序返回
pub struct SequenceTracker {
    next_expected: Arc<AtomicU64>,
    pending_results: Arc<Mutex<HashMap<u64, InferenceResult>>>,
    condvar: Arc<Condvar>,
}

impl SequenceTracker {
    pub fn new() -> Self {
        Self {
            next_expected: Arc::new(AtomicU64::new(0)),
            pending_results: Arc::new(Mutex::new(HashMap::new())),
            condvar: Arc::new(Condvar::new()),
        }
    }

    /// 存储推理结果
    pub fn store_result(&self, task_id: u64, result: Result<Vec<crate::DetectionResult>>) {
        let mut pending = self.pending_results.lock().unwrap();
        pending.insert(task_id, InferenceResult { task_id, result });
        self.condvar.notify_all();
    }

    /// 获取下一个顺序的结果（阻塞直到可用）
    pub fn get_next_result(&self, timeout: Duration) -> Option<InferenceResult> {
        let _start = Instant::now();
        let expected = self.next_expected.load(Ordering::Relaxed);

        let pending = self.pending_results.lock().unwrap();

        // 如果结果已准备好，立即返回
        if let Some(result) = pending.get(&expected) {
            let result_clone = InferenceResult {
                task_id: result.task_id,
                result: result.result.clone(),
            };
            drop(pending);
            self.next_expected.store(expected + 1, Ordering::Relaxed);
            return Some(result_clone);
        }

        // 否则等待结果
        let wait_result = self.condvar.wait_timeout(pending, timeout).unwrap();

        // 检查结果是否可用
        if let Some(result) = wait_result.0.get(&expected) {
            let result_clone = InferenceResult {
                task_id: result.task_id,
                result: result.result.clone(),
            };
            drop(wait_result.0);
            self.next_expected.store(expected + 1, Ordering::Relaxed);
            Some(result_clone)
        } else {
            None // 超时
        }
    }

    /// 清理已完成的结果
    pub fn cleanup_completed(&self) {
        let expected = self.next_expected.load(Ordering::Relaxed);
        let mut pending = self.pending_results.lock().unwrap();
        pending.retain(|&task_id, _| task_id >= expected);
    }
}

/// 工作线程
struct Worker {
    id: usize,
    detector: Yolov8Detector,
    task_queue: Arc<TaskQueue>,
    sequence_tracker: Arc<SequenceTracker>,
    shutdown: Arc<AtomicBool>,
    processed_count: Arc<AtomicUsize>,
}

impl Worker {
    pub fn new(
        id: usize,
        detector: Yolov8Detector,
        task_queue: Arc<TaskQueue>,
        sequence_tracker: Arc<SequenceTracker>,
        shutdown: Arc<AtomicBool>,
        processed_count: Arc<AtomicUsize>,
    ) -> Self {
        Self {
            id,
            detector,
            task_queue,
            sequence_tracker,
            shutdown,
            processed_count,
        }
    }

    pub fn run(mut self) {

        loop {
            // 检查是否应该关闭
            if self.shutdown.load(Ordering::Relaxed) {
                break;
            }

            // 获取任务
            if let Some(task) = self.task_queue.pop() {
                let task_id = task.task_id;

                // 执行推理
                let result = self.detector.detect(&task.image);

                // 存储结果
                self.sequence_tracker.store_result(task_id, result);

                // 更新计数
                self.processed_count.fetch_add(1, Ordering::Relaxed);
            } else {
                // 队列为空，短暂休眠
                thread::sleep(Duration::from_millis(1));
            }
        }
    }
}

/// 推理服务
pub struct InferenceService {
    task_queue: Arc<TaskQueue>,
    sequence_tracker: Arc<SequenceTracker>,
    shutdown: Arc<AtomicBool>,
    task_counter: Arc<AtomicU64>,
    processed_count: Arc<AtomicUsize>,
    _workers: Vec<thread::JoinHandle<()>>,
}

// 使 InferenceService 可以安全地在线程间传递
unsafe impl Send for InferenceService {}
unsafe impl Sync for InferenceService {}

impl InferenceService {
    /// 创建新的推理服务
    pub fn new(model_path: &str, num_workers: usize, max_queue_size: usize) -> Result<Self> {
        if num_workers == 0 {
            return Err(crate::RknnError::InvalidInput("Number of workers must be > 0".to_string()));
        }

        let task_queue = Arc::new(TaskQueue::new(max_queue_size));
        let sequence_tracker = Arc::new(SequenceTracker::new());
        let shutdown = Arc::new(AtomicBool::new(false));
        let task_counter = Arc::new(AtomicU64::new(0));
        let processed_count = Arc::new(AtomicUsize::new(0));

        let mut workers = Vec::new();

        // 创建工作线程
        for i in 0..num_workers {
            let detector = Yolov8Detector::new(model_path)
                .map_err(|e| crate::RknnError::InvalidInput(format!("Failed to create detector {}: {:?}", i, e)))?;

            let worker = Worker::new(
                i,
                detector,
                task_queue.clone(),
                sequence_tracker.clone(),
                shutdown.clone(),
                processed_count.clone(),
            );

            let handle = thread::spawn(|| worker.run());
            workers.push(handle);
        }

        Ok(Self {
            task_queue,
            sequence_tracker,
            shutdown,
            task_counter,
            processed_count,
            _workers: workers,
        })
    }

    /// 提交推理任务
    pub async fn submit_inference<F>(
        &self,
        image: ImageBuffer,
        callback: F,
    ) -> Result<u64>
    where
        F: FnOnce(Result<Vec<crate::DetectionResult>>) + Send + 'static,
    {
        // 分配任务ID
        let task_id = self.task_counter.fetch_add(1, Ordering::Relaxed);

        // 创建任务
        let task = InferenceTask {
            task_id,
            image,
            callback: Box::new(callback),
        };

        // 提交到队列
        self.task_queue.push(task)?;
        Ok(task_id)
    }

    /// 提交推理任务并等待结果（阻塞）
    pub fn inference_sync(&self, image: ImageBuffer, timeout: Duration) -> Result<Vec<crate::DetectionResult>> {
        let task_id = self.task_counter.fetch_add(1, Ordering::Relaxed);

        // 提交任务
        let task = InferenceTask {
            task_id,
            image: image.clone(),
            callback: Box::new(|_| {}), // 忽略回调，我们将直接获取结果
        };

        self.task_queue.push(task)?;

        // 等待结果 - 使用简单的轮询方式
        let start = Instant::now();
        loop {
            // 检查超时
            if start.elapsed() > timeout {
                return Err(crate::RknnError::InvalidInput("Timeout".to_string()));
            }

            // 简单的轮询等待
            let result = self.sequence_tracker.get_next_result(Duration::from_millis(10));
            if let Some(inference_result) = result {
                if inference_result.task_id == task_id {
                    return inference_result.result;
                }
                // 如果不是我们要的结果，继续等待
            }

            thread::sleep(Duration::from_millis(1));
        }
    }

    /// 获取队列状态
    pub fn get_stats(&self) -> ServiceStats {
        ServiceStats {
            queue_size: self.task_queue.len(),
            dropped_tasks: self.task_queue.dropped_count(),
            processed_tasks: self.processed_count.load(Ordering::Relaxed),
            next_expected_task_id: self.sequence_tracker.next_expected.load(Ordering::Relaxed),
        }
    }

    /// 清理已完成的任务
    pub fn cleanup(&self) {
        self.sequence_tracker.cleanup_completed();
    }
}

impl Drop for InferenceService {
    fn drop(&mut self) {
        // 通知所有工作线程关闭
        self.shutdown.store(true, Ordering::Relaxed);

        // 等待所有工作线程完成
        let workers = std::mem::take(&mut self._workers);
        for worker in workers {
            let _ = worker.join();
        }
    }
}

/// 服务状态统计
#[derive(Debug, Clone)]
pub struct ServiceStats {
    pub queue_size: usize,
    pub dropped_tasks: usize,
    pub processed_tasks: usize,
    pub next_expected_task_id: u64,
}

