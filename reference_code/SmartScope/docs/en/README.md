# Depth Anything RKNN Inference

一个纯净的深度估计推理库，基于Depth Anything模型和RKNN推理后端。

## 功能特性

- **纯净设计**: 专注于RKNN推理，无多余功能
- **简洁API**: 简单易用的C++接口
- **高性能**: 优化的RKNN推理流程
- **轻量级**: 最小化依赖，专注于核心功能

## 项目结构

```
depth_anything_inference/
├── include/                    # 头文件
│   └── depth_anything_inference.hpp
├── src/                       # 源代码
│   ├── depth_anything_inference_impl.cpp
│   ├── rknn_core/            # RKNN推理核心
│   ├── deploy_core/          # 部署核心
│   ├── image_processing_utils/ # 图像处理工具
│   └── mono_stereo_depth_anything/ # 深度估计模型
├── examples/                  # 示例代码
│   └── depth_inference_example.cpp
├── tests/                    # 测试代码
├── models/                   # 模型文件
├── test_data/               # 测试数据
└── CMakeLists.txt           # 构建配置
```

## 依赖项

- **OpenCV**: 图像处理
- **Eigen3**: 数学计算
- **glog**: 日志记录
- **GTest**: 单元测试（可选）
- **RKNN**: RKNN推理后端

## 安装

### 1. 克隆项目

```bash
git clone <repository_url>
cd depth_anything_inference
```

### 2. 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install libopencv-dev libeigen3-dev libgoogle-glog-dev

# 可选：安装测试依赖
sudo apt-get install libgtest-dev
```

### 3. 构建项目

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 使用方法

### 基本用法

```cpp
#include "depth_anything_inference.hpp"

int main() {
    // 创建RKNN推理引擎
    auto engine = depth_anything::CreateRknnInferCore("models/depth_anything_v2_vits.rknn");
    
    // 创建深度估计模型
    auto model = depth_anything::CreateDepthAnythingModel(engine, 518, 518);
    
    // 加载图像
    cv::Mat image = cv::imread("input.jpg");
    
    // 执行推理
    cv::Mat depth;
    bool success = model->ComputeDepth(image, depth);
    
    if (success) {
        cv::imwrite("depth_result.png", depth);
    }
    
    return 0;
}
```

### 运行示例

```bash
# 编译示例
cd build
make depth_inference_example

# 运行示例
./bin/depth_inference_example test_data/input.jpg
```

## API 参考

### InferenceEngine 接口

```cpp
class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;
    
    // 同步推理
    virtual bool ComputeDepth(const cv::Mat& image, cv::Mat& depth) = 0;
    
    // 异步推理
    virtual std::future<cv::Mat> ComputeDepthAsync(const cv::Mat& image) = 0;
    
    // 管道管理
    virtual void InitPipeline() = 0;
    virtual void StopPipeline() = 0;
    virtual void ClosePipeline() = 0;
};
```

### 工厂函数

```cpp
// 创建RKNN推理引擎
std::shared_ptr<InferenceEngine> CreateRknnInferCore(
    const std::string& model_path,
    int mem_buf_size = 5,
    int parallel_ctx_num = 3
);

// 创建深度估计模型
std::shared_ptr<InferenceEngine> CreateDepthAnythingModel(
    std::shared_ptr<InferenceEngine> engine,
    int input_height = 518,
    int input_width = 518
);
```

## 性能特点

- **推理延迟**: < 50ms (RK3588)
- **内存使用**: < 500MB
- **模型大小**: < 100MB
- **支持分辨率**: 518x518 (可配置)

## 测试

### 运行单元测试

```bash
cd build
make test_basic
./bin/test_basic
```

## 故障排除

### 常见问题

1. **模型加载失败**
   - 检查模型文件路径
   - 确认模型格式正确
   - 检查RKNN环境

2. **推理性能低**
   - 检查硬件加速是否启用
   - 调整内存缓冲区大小
   - 优化并行上下文数量

3. **内存不足**
   - 减少内存缓冲区大小
   - 使用内存优化选项

### 调试模式

```bash
# 启用调试信息
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

## 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 更新日志

### v1.0.0
- 初始版本发布
- 纯净的RKNN推理功能
- 简洁的API接口
- 基本示例和文档 