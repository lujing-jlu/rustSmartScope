#!/bin/bash

# 设置项目根目录路径（可通过环境变量覆盖）
PROJECT_ROOT="${PROJECT_ROOT:-/home/eddysun/App/Qt/SmartScope}"

echo "启动SmartScope应用程序..."
echo "项目根目录: $PROJECT_ROOT"

cd "$PROJECT_ROOT" || { echo "无法进入 $PROJECT_ROOT"; exit 1; }
echo "当前工作目录: $(pwd)"

# 适配最小化部署布局（可执行在根目录）
BIN_DIR="$PROJECT_ROOT"
LIB_DIR="$PROJECT_ROOT/lib"
PLUGINS_DIR="$PROJECT_ROOT/plugins"
MODELS_DIR_ROOT="$PROJECT_ROOT/models"

# 设置库路径
export LD_LIBRARY_PATH="$LIB_DIR:${LD_LIBRARY_PATH:-}"
echo "设置库路径: $LD_LIBRARY_PATH"

# 设置Qt插件路径（如存在）
if [ -d "$PLUGINS_DIR" ]; then
  export QT_PLUGIN_PATH="$PLUGINS_DIR"
  echo "设置Qt插件路径: $QT_PLUGIN_PATH"
fi

# OpenGL/EGL 相关环境（优先使用 EGL + ES2）
export QT_QPA_PLATFORM=xcb
export QT_XCB_GL_INTEGRATION=egl
export QT_OPENGL=es2

# 确保数据根目录存在
ROOT="$HOME/data"
mkdir -p "$ROOT/Pictures" "$ROOT/Screenshots" "$ROOT/Videos"

# 应用程序名称与路径
APP_NAME="SmartScopeQt"
APP_PATH="$BIN_DIR/$APP_NAME"

# 检查是否已经有实例在运行
PID=$(pgrep -x "$APP_NAME" || true)
if [ -n "$PID" ]; then
    echo "发现SmartScope已在运行，进程ID: $PID，正在切换到前台..."
    if command -v wmctrl >/dev/null 2>&1; then
      WINDOW_ID=$(wmctrl -lp | awk -v pid="$PID" '$3==pid{print $1; exit}')
      if [ -n "$WINDOW_ID" ]; then
          wmctrl -ia "$WINDOW_ID"
          echo "已切换到前台"
          exit 0
      fi
      echo "未找到窗口，wmctrl已安装但未匹配到窗口"
    else
      echo "提示：可安装wmctrl以实现窗口前置 (sudo apt-get install wmctrl)"
    fi
fi

# 启动可执行文件
if [ -x "$APP_PATH" ]; then
    echo "找到可执行文件，启动应用程序..."
    exec "$APP_PATH" "$@"
else
    echo "错误: 可执行文件不存在! ($APP_PATH)"
    echo "请确保程序已经部署或编译完成，并检查目录布局(lib/plugins/models)。"
    exit 1
fi 