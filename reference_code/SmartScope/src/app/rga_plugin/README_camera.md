# USB摄像头查看器

这是一个用于查看USB摄像头并在窗口中显示的Python脚本。

## 功能特性

- 支持指定摄像头设备ID (video0, video1, video2...)
- 实时显示摄像头画面
- 可调节分辨率
- 支持保存图片
- 显示摄像头信息
- 自动检测可用设备

## 安装依赖

```bash
pip install opencv-python
```

## 使用方法

### 1. 查看可用摄像头设备

```bash
python3 camera_viewer.py -l
```

### 2. 查看video1摄像头 (默认)

```bash
python3 camera_viewer.py
```

### 3. 指定摄像头设备

```bash
# 查看video0
python3 camera_viewer.py -d 0

# 查看video2
python3 camera_viewer.py -d 2
```

### 4. 指定分辨率

```bash
# 设置分辨率为1280x720
python3 camera_viewer.py -d 1 -w 1280 -h 720
```

## 操作说明

在摄像头窗口显示时：

- 按 `q` 键退出程序
- 按 `s` 键保存当前帧为图片
- 按 `i` 键显示摄像头详细信息

## 命令行参数

- `-d, --device`: 摄像头设备ID (默认: 1)
- `-w, --width`: 视频宽度 (默认: 640)
- `-h, --height`: 视频高度 (默认: 480)
- `-l, --list`: 列出所有可用的摄像头设备

## 示例

```bash
# 列出所有摄像头设备
python3 camera_viewer.py -l

# 查看video1，分辨率1280x720
python3 camera_viewer.py -d 1 -w 1280 -h 720

# 查看video0，默认分辨率
python3 camera_viewer.py -d 0
```

## 故障排除

1. **设备不存在**: 使用 `-l` 参数查看可用设备
2. **无法打开摄像头**: 检查设备权限，确保用户属于video组
3. **画面不显示**: 检查摄像头是否被其他程序占用
4. **OpenCV未安装**: 运行 `pip install opencv-python`

## 权限设置

如果遇到权限问题，可以将用户添加到video组：

```bash
sudo usermod -a -G video $USER
```

然后重新登录或重启系统。 