# 深度流程对比分析与Depth Anything集成方案

## 当前流程对比

### 1. stereo_rectify_stream 流程

**完整流程：**
```
1. 相机采集 → 2. 图像旋转 → 3. 立体校正 → 4. 预处理 → 5. 双目视差计算 → 6. 深度计算 → 7. 单目深度推理 → 8. 深度校准 → 9. 点云生成
```

**关键步骤：**
- **立体校正**: 使用相机参数进行立体校正
- **双目深度**: SGBM算法计算视差，转换为深度
- **单目深度**: Depth Anything模型推理
- **深度校准**: 使用双目深度校准单目深度（线性拟合）
- **点云生成**: 使用校准后的单目深度生成点云

**核心代码片段：**
```cpp
// 双目深度计算
Mat disp16S; sgbm->compute(grayL, grayR, disp16S);
Mat disp32F; disp16S.convertTo(disp32F, CV_32F, 1.0/16.0);
Mat points3D; reprojectImageTo3D(disp32F, points3D, Q, true, CV_32F);
Mat depthZ = xyz[2]; // Z轴深度

// 单目深度推理
Mat monoDepth;
if (monoModel) {
    monoModel->ComputeDepth(procL, monoDepth);
}

// 深度校准
double s_est = 1.0, b_est = 0.0;
if (fitScaleAndBias(monoDepth, depthMm, disp32F, valid, leftBoundX, s_est, b_est)) {
    // 应用校准参数
    monoMm = monoMm * (float)s_est + (float)b_est;
}

// 点云生成（使用校准后的单目深度）
saveRGBPointCloud(leftRect, monoMmCalibrated, Q, "mono_pointcloud.ply");
```

### 2. SmartScope 当前流程

**当前流程：**
```
1. 相机采集 → 2. 立体校正 → 3. 双目视差计算 → 4. 深度计算 → 5. 点云生成
```

**关键步骤：**
- **立体校正**: 使用StereoCalibrationHelper
- **双目深度**: StereoDepthInference进行推理
- **点云生成**: PointCloudGenerator使用双目深度

**核心代码片段：**
```cpp
// 推理服务
result.depth_map = m_inference->inference(cropped_left, cropped_right);

// 点云生成
bool success = m_pointCloudGenerator->generate(
    depthMap,           // 双目深度图
    colorImageForPCL,   // 颜色图像
    m_calibrationHelper, // 校准参数
    points,             // 输出点云
    colors,             // 输出颜色
    m_pointCloudPixelCoords
);
```

## 主要差异分析

### 1. 深度来源差异
- **stereo_rectify_stream**: 使用校准后的单目深度（Depth Anything）
- **SmartScope**: 使用双目立体深度

### 2. 深度质量差异
- **单目深度**: 全局一致性好，细节丰富，但需要校准
- **双目深度**: 几何精度高，但可能有空洞和噪声

### 3. 处理复杂度
- **stereo_rectify_stream**: 复杂，包含深度校准
- **SmartScope**: 简单，直接使用双目结果

## 集成方案

### 方案1：完全集成Depth Anything流程

**目标：** 在SmartScope中实现完整的stereo_rectify_stream流程

**实现步骤：**

#### 1. 添加Depth Anything推理引擎
```cpp
// 在InferenceService中添加
#include "depth_anything_inference.hpp"

class InferenceService {
private:
    std::shared_ptr<depth_anything::InferenceEngine> m_monoEngine;
    std::shared_ptr<depth_anything::InferenceEngine> m_monoModel;
    
public:
    bool initializeMonoDepthModel(const std::string& modelPath);
    cv::Mat computeMonoDepth(const cv::Mat& leftImage);
    bool calibrateMonoDepth(const cv::Mat& monoDepth, const cv::Mat& stereoDepth);
};
```

#### 2. 实现深度校准算法
```cpp
// 从stereo_rectify_stream移植校准函数
bool InferenceService::fitScaleAndBias(
    const cv::Mat &monoDepth,
    const cv::Mat &metricDepth,
    const cv::Mat &disparity,
    const cv::Mat &validMask,
    int leftBoundX,
    double &s_out,
    double &b_out,
    int minSamples = 1000,
    double percentile = 0.5)
{
    // 实现线性拟合算法
    // 使用RANSAC或加权线性拟合
}
```

#### 3. 修改点云生成流程
```cpp
// 在MeasurementPage中修改
void MeasurementPage::generatePointCloudFromCalibratedMono() {
    // 1. 获取双目深度
    cv::Mat stereoDepth = m_depthMap;
    
    // 2. 计算单目深度
    cv::Mat monoDepth = m_inferenceService->computeMonoDepth(m_leftRectifiedImage);
    
    // 3. 校准单目深度
    double scale, bias;
    if (m_inferenceService->calibrateMonoDepth(monoDepth, stereoDepth, scale, bias)) {
        cv::Mat calibratedMonoDepth = monoDepth * scale + bias;
        
        // 4. 使用校准后的单目深度生成点云
        m_pointCloudGenerator->generate(
            calibratedMonoDepth,  // 使用校准后的单目深度
            m_leftRectifiedImage, // 颜色图像
            m_calibrationHelper,
            points, colors, m_pointCloudPixelCoords
        );
    }
}
```

#### 4. 更新调试页面
```cpp
// 在DebugPage中显示四种图像
void DebugPage::setDebugImages(
    const cv::Mat &rectifiedLeftImage,
    const cv::Mat &stereoDepthMap,
    const cv::Mat &monoDepthMap,
    const cv::Mat &calibratedMonoDepthMap)
{
    // 显示：校正左图、双目深度、单目深度、校准后单目深度
}
```

### 方案2：混合模式

**目标：** 提供两种深度模式选择

**实现：**
```cpp
enum class DepthMode {
    STEREO_ONLY,      // 仅双目深度
    MONO_CALIBRATED,  // 校准后的单目深度
    HYBRID           // 混合模式（根据区域选择）
};

class MeasurementPage {
private:
    DepthMode m_depthMode = DepthMode::STEREO_ONLY;
    
public:
    void setDepthMode(DepthMode mode);
    cv::Mat getBestDepthMap(); // 根据模式返回最佳深度图
};
```

### 方案3：渐进式集成

**阶段1：** 添加单目深度推理
**阶段2：** 实现深度校准
**阶段3：** 优化点云生成
**阶段4：** 添加模式选择

## 技术实现细节

### 1. 深度校准算法

**线性拟合：**
```cpp
// 从stereo_rectify_stream移植
static bool weightedLinearFit(const std::vector<std::tuple<float, float, float>>& points,
                             double &s_out, double &b_out) {
    // 实现加权线性拟合
    // 使用置信度权重
}
```

**RANSAC鲁棒拟合：**
```cpp
static bool ransacLinearFit(const std::vector<std::tuple<float, float, float>>& points,
                           double &s_out, double &b_out,
                           int minSamples = 2,
                           int maxIterations = 100,
                           float threshold = 30.0f,
                           int minInliers = 10) {
    // 实现RANSAC算法
}
```

### 2. 点云生成优化

**使用校准后的单目深度：**
```cpp
// 修改PointCloudGenerator
bool PointCloudGenerator::generateFromCalibratedMono(
    const cv::Mat& calibratedMonoDepth,
    const cv::Mat& colorImage,
    const StereoCalibrationHelper* calibrationHelper,
    std::vector<QVector3D>& outPoints,
    std::vector<QVector3D>& outColors,
    std::vector<cv::Point2i>& outPixelCoords)
{
    // 使用校准后的单目深度生成点云
    // 保持与现有接口兼容
}
```

### 3. 性能优化

**异步处理：**
```cpp
// 并行计算双目和单目深度
std::future<cv::Mat> stereoFuture = std::async(
    std::launch::async, 
    [this, left, right]() { return m_inference->inference(left, right); }
);

std::future<cv::Mat> monoFuture = std::async(
    std::launch::async,
    [this, left]() { return m_monoModel->ComputeDepth(left); }
);

cv::Mat stereoDepth = stereoFuture.get();
cv::Mat monoDepth = monoFuture.get();
```

## 配置文件更新

### 1. 模型路径配置
```yaml
# calibration_settings.yaml
depth_anything:
  model_path: "models/depth_anything_v2_vits.rknn"
  input_size: [518, 518]
  calibration_mode: "linear"
  mask_margin_left: 0
```

### 2. 校准参数配置
```yaml
depth_calibration:
  min_samples: 1000
  percentile: 0.5
  ransac_threshold: 30.0
  max_iterations: 100
  min_inliers: 10
```

## 测试验证

### 1. 深度质量对比
- 双目深度 vs 校准后单目深度
- 点云密度和质量对比
- 处理速度对比

### 2. 校准精度验证
- 校准参数稳定性
- 不同场景下的校准效果
- 校准失败的处理机制

### 3. 集成测试
- 端到端流程测试
- 内存使用监控
- 性能基准测试

## 实施建议

### 优先级排序
1. **高优先级**: 集成Depth Anything推理引擎
2. **中优先级**: 实现深度校准算法
3. **低优先级**: 优化点云生成和UI改进

### 风险控制
1. **兼容性**: 保持现有功能不变
2. **性能**: 确保实时性要求
3. **稳定性**: 充分的测试验证

### 时间规划
- **第1周**: 集成Depth Anything推理
- **第2周**: 实现深度校准
- **第3周**: 优化点云生成
- **第4周**: 测试和调试

## 总结

通过集成stereo_rectify_stream的深度校准流程，SmartScope可以获得更高质量的深度信息，从而生成更精确的点云。建议采用渐进式集成方案，确保系统的稳定性和兼容性。 