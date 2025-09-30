#!/bin/bash

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info()   { echo -e "${GREEN}[信息] $1${NC}"; }
log_warning(){ echo -e "${YELLOW}[警告] $1${NC}"; }
log_error()  { echo -e "${RED}[错误] $1${NC}"; }
log_debug()  { echo -e "${BLUE}[调试] $1${NC}"; }

# 解析参数：-jN/--jobs=N 并行；--reconfigure 强制重新配置；--clean 清理；--run-only 仅运行
JOBS=4
FORCE_RECONF=0
CLEAN=0
RUN_ONLY=0
for arg in "$@"; do
  case "$arg" in
    -j*) JOBS="${arg#-j}" ;;
    --jobs=*) JOBS="${arg#--jobs=}" ;;
    --reconfigure) FORCE_RECONF=1 ;;
    --clean) CLEAN=1 ;;
    --run-only) RUN_ONLY=1 ;;
  esac
done

# 项目根目录（绝对路径）
PROJECT_ROOT="/home/eddysun/projects/SmartScope"
BUILD_DIR="$PROJECT_ROOT/build"

# 切换到项目根目录并打印当前目录
cd "$PROJECT_ROOT"
pwd
log_info "当前工作目录: $(pwd)"

# 设置运行时库路径（优先包含 build 输出与本地 lib 目录）
export LD_LIBRARY_PATH="$BUILD_DIR:$BUILD_DIR/lib:$PROJECT_ROOT/lib:$LD_LIBRARY_PATH"
log_info "设置库路径: $LD_LIBRARY_PATH"

# 提示存在旧的 in-source 配置（不删除，仅提示）
if [[ -f "$PROJECT_ROOT/CMakeCache.txt" || -f "$PROJECT_ROOT/Makefile" ]]; then
  log_warning "检测到旧的 in-source CMake 缓存(可能导致链接顺序未刷新)：$PROJECT_ROOT/CMakeCache.txt 或 Makefile 存在。"
  log_warning "本脚本使用 out-of-source 构建(./build)，忽略根目录旧缓存。"
fi

# 准备构建目录
mkdir -p "$BUILD_DIR"

# 可选清理
if [[ "$CLEAN" == "1" ]]; then
  log_info "执行清理: 清空 $BUILD_DIR 并重新配置"
  (cmake --build "$BUILD_DIR" --target clean || true) >/dev/null 2>&1 || true
  rm -rf "$BUILD_DIR"
  mkdir -p "$BUILD_DIR"
  FORCE_RECONF=1
fi

# 按需配置（增量：存在 CMakeCache.txt 且未强制时跳过）
NEED_RECONF=0
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  NEED_RECONF=1
elif [[ "$FORCE_RECONF" == "1" ]]; then
  NEED_RECONF=1
else
  # 任意 CMakeLists.txt 比缓存新则需要重新配置
  if find "$PROJECT_ROOT" -type f -name CMakeLists.txt -newer "$BUILD_DIR/CMakeCache.txt" | grep -q .; then
    NEED_RECONF=1
  fi
fi

if [[ "$NEED_RECONF" == "1" ]]; then
  log_info "执行 CMake 配置 (out-of-source) ..."
  cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
else
  log_info "增量编译：CMake 配置保持不变，跳过 CMake 配置"
fi

# 编译（除非 run-only）
if [[ "$RUN_ONLY" != "1" ]]; then
  log_info "开始编译 (并行 $JOBS)..."
  cmake --build "$BUILD_DIR" -j"$JOBS"
else
  log_info "跳过编译，仅运行可执行文件"
fi

# 复制主程序到项目根目录
BUILD_APP="$BUILD_DIR/SmartScopeQt"
ROOT_APP="$PROJECT_ROOT/SmartScopeQt"

if [[ -x "$BUILD_APP" ]]; then
  log_info "编译完成，复制主程序到项目根目录..."
  cp "$BUILD_APP" "$ROOT_APP"
  chmod +x "$ROOT_APP"
  log_info "主程序已复制到: $ROOT_APP"
  
  # 运行
  log_info "运行程序..."
  pwd
  ulimit -c unlimited
  "$ROOT_APP" | cat
else
  log_error "未找到可执行文件: $BUILD_APP"
  log_info "尝试定位可执行文件..."
  find "$PROJECT_ROOT" -maxdepth 3 -type f -name SmartScopeQt -executable 2>/dev/null || true
  log_info "输出链接命令（便于排查 undefined reference）："
  if [[ -f "$BUILD_DIR/CMakeFiles/SmartScopeQt.dir/link.txt" ]]; then
    sed -n '1,200p' "$BUILD_DIR/CMakeFiles/SmartScopeQt.dir/link.txt" | cat
  else
    log_warning "未找到 link.txt"
  fi
  exit 1
fi 