use crate::{ImageBuffer, Result, Yolov8Detector};
use std::sync::atomic::{AtomicBool, AtomicU64, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

/// 推理任务
struct InferenceTask {
    task_id: u64,
    image: ImageBuffer,
    sender: std::sync::mpsc::Sender<Result<Vec<crate::DetectionResult>>>,
}

/// 简化的任务队列
struct TaskQueue {
    queue: Arc<Mutex<Vec<InferenceTask>>>,
    max_size: usize,
    dropped_count: Arc<AtomicUsize>,
}

impl TaskQueue {
    fn new(max_size: usize) -> Self {
        Self {
            queue: Arc::new(Mutex::new(Vec::new())),
            max_size,
            dropped_count: Arc::new(AtomicUsize::new(0)),
        }
    }

    fn push(&self, task: InferenceTask) -> Result<()> {
        let mut queue = self.queue.lock().unwrap();

        // 如果队列已满，丢弃最旧的任务
        if queue.len() >= self.max_size {
            queue.remove(0);
            self.dropped_count.fetch_add(1, Ordering::Relaxed);
        }

        queue.push(task);
        Ok(())
    }

    fn pop(&self) -> Option<InferenceTask> {
        let mut queue = self.queue.lock().unwrap();
        if queue.is_empty() {
            None
        } else {
            Some(queue.remove(0))
        }
    }

    fn len(&self) -> usize {
        self.queue.lock().unwrap().len()
    }

    fn dropped_count(&self) -> usize {
        self.dropped_count.load(Ordering::Relaxed)
    }
}

/// 工作线程
struct Worker {
    id: usize,
    detector: Yolov8Detector,
    task_queue: Arc<TaskQueue>,
    shutdown: Arc<AtomicBool>,
    processed_count: Arc<AtomicUsize>,
}

impl Worker {
    fn new(
        id: usize,
        detector: Yolov8Detector,
        task_queue: Arc<TaskQueue>,
        shutdown: Arc<AtomicBool>,
        processed_count: Arc<AtomicUsize>,
    ) -> Self {
        Self {
            id,
            detector,
            task_queue,
            shutdown,
            processed_count,
        }
    }

    fn run(mut self) {

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

                // 发送结果
                let _ = task.sender.send(result);

                // 更新计数
                self.processed_count.fetch_add(1, Ordering::Relaxed);
            } else {
                // 队列为空，短暂休眠
                thread::sleep(Duration::from_millis(1));
            }
        }
    }
}

/// 简化的推理服务
pub struct SimpleInferenceService {
    task_queue: Arc<TaskQueue>,
    shutdown: Arc<AtomicBool>,
    task_counter: Arc<AtomicU64>,
    processed_count: Arc<AtomicUsize>,
    _workers: Vec<thread::JoinHandle<()>>,
}

// 使 SimpleInferenceService 可以安全地在线程间传递
unsafe impl Send for SimpleInferenceService {}
unsafe impl Sync for SimpleInferenceService {}

impl SimpleInferenceService {
    /// 创建新的推理服务
    pub fn new(model_path: &str, num_workers: usize, max_queue_size: usize) -> Result<Self> {
        if num_workers == 0 {
            return Err(crate::RknnError::InvalidInput("Number of workers must be > 0".to_string()));
        }

        let task_queue = Arc::new(TaskQueue::new(max_queue_size));
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
                shutdown.clone(),
                processed_count.clone(),
            );

            let handle = thread::spawn(|| worker.run());
            workers.push(handle);
        }

        Ok(Self {
            task_queue,
            shutdown,
            task_counter,
            processed_count,
            _workers: workers,
        })
    }

    /// 推理任务并等待结果
    pub fn inference(&self, image: ImageBuffer) -> Result<Vec<crate::DetectionResult>> {
        let task_id = self.task_counter.fetch_add(1, Ordering::Relaxed);

        // 创建通道接收结果
        let (sender, receiver) = std::sync::mpsc::channel();

        // 创建任务
        let task = InferenceTask {
            task_id,
            image,
            sender,
        };

        // 提交到队列
        self.task_queue.push(task)?;

        // 等待结果
        match receiver.recv_timeout(Duration::from_secs(10)) {
            Ok(result) => result,
            Err(_) => Err(crate::RknnError::InvalidInput("Timeout".to_string())),
        }
    }

    /// 获取服务状态
    pub fn get_stats(&self) -> ServiceStats {
        ServiceStats {
            queue_size: self.task_queue.len(),
            dropped_tasks: self.task_queue.dropped_count(),
            processed_tasks: self.processed_count.load(Ordering::Relaxed),
        }
    }
}

impl Drop for SimpleInferenceService {
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
}