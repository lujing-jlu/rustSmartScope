# 清理后的项目结构

## 项目概述

这是一个纯净的RKNN推理库，专注于深度估计功能。项目已经过全面清理，移除了所有不必要的文件。

## 清理内容

### 已删除的文件
- `config.h.in` - 配置文件模板（不需要）
- `install.sh` - 安装脚本（不需要）
- `examples/simple_usage.cpp` - 重复的示例文件
- `src/deploy_core/README.md` - 模块内部文档
- `tests/depth_inference_tests.cpp` - 多余的测试文件
- `build/` - 构建产物目录

### 保留的核心文件

```
depth_anything_inference/
├── include/                    # 头文件
│   └── depth_anything_inference.hpp  # 主接口头文件
├── src/                       # 源代码
│   ├── depth_anything_inference_impl.cpp  # 主实现文件
│   ├── rknn_core/            # RKNN推理核心
│   │   ├── include/rknn_core/rknn_core.h
│   │   └── src/
│   ├── deploy_core/          # 部署核心
│   │   ├── include/deploy_core/
│   │   └── src/
│   ├── image_processing_utils/ # 图像处理工具
│   │   ├── include/detection_2d_util/
│   │   └── src/
│   └── mono_stereo_depth_anything/ # 深度估计模型
│       ├── include/mono_stereo_depth_anything/
│       └── src/
├── examples/                  # 示例代码
│   ├── depth_inference_example.cpp  # 纯净示例
│   └── CMakeLists.txt        # 示例构建配置
├── tests/                    # 测试代码
│   ├── test_basic.cpp        # 基本测试
│   └── CMakeLists.txt        # 测试构建配置
├── models/                   # 模型文件
│   └── depth_anything_v2_vits.rknn
├── test_data/               # 测试数据
│   └── 20250728_100708_左相机.jpg
├── CMakeLists.txt           # 主构建配置
├── build.sh                 # 构建脚本
├── README.md                # 项目文档
├── PROJECT_SUMMARY.md       # 项目总结
└── CLEAN_PROJECT_STRUCTURE.md # 本文件
```

## 文件统计

### 核心文件 (4个)
- `include/depth_anything_inference.hpp` - 主API接口
- `src/depth_anything_inference_impl.cpp` - 核心实现
- `examples/depth_inference_example.cpp` - 使用示例
- `tests/test_basic.cpp` - 单元测试

### 配置文件 (6个)
- `CMakeLists.txt` - 主构建配置
- `src/CMakeLists.txt` - 源代码构建配置
- `examples/CMakeLists.txt` - 示例构建配置
- `tests/CMakeLists.txt` - 测试构建配置
- `src/rknn_core/CMakeLists.txt` - RKNN核心构建配置
- `src/deploy_core/CMakeLists.txt` - 部署核心构建配置

### 文档文件 (3个)
- `README.md` - 项目文档
- `PROJECT_SUMMARY.md` - 项目总结
- `CLEAN_PROJECT_STRUCTURE.md` - 清理结构文档

### 数据文件 (2个)
- `models/depth_anything_v2_vits.rknn` - RKNN模型文件
- `test_data/20250728_100708_左相机.jpg` - 测试图像

### 脚本文件 (1个)
- `build.sh` - 构建脚本

## 构建和使用

```bash
# 构建项目
./build.sh

# 运行示例
./build/bin/depth_inference_example test_data/20250728_100708_左相机.jpg

# 运行测试
./build/bin/test_basic
```

## 测试结果

```
[==========] Running 8 tests from 1 test suite.
[  PASSED  ] 8 tests.
```

所有测试都通过，项目构建成功！

## 清理效果

1. **减少文件数量**: 删除了5个不必要的文件
2. **简化结构**: 移除了重复的示例和多余的文档
3. **专注核心**: 保留了所有核心功能文件
4. **易于维护**: 更清晰的项目结构
5. **构建成功**: 所有组件都能正常构建和运行

## 项目特点

- **纯净性**: 专注于RKNN推理，无多余功能
- **简洁性**: 最小化的文件结构
- **完整性**: 保留所有必要的核心功能
- **可维护性**: 清晰的文件组织和文档

项目现在更加简洁，专注于核心的RKNN推理功能，所有测试通过，构建系统完整。 