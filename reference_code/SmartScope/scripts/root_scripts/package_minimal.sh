#!/usr/bin/env bash
set -Eeuo pipefail

# 一键打包最小部署包：
# 1) 构建（如需）
# 2) 调用 deploy_minimal_smartscope.sh 生成最小部署目录
# 3) 打 tar.gz 归档
#
# 可配置环境变量：
#   PROJECT_ROOT  默认: /home/eddysun/projects/SmartScope
#   BUILD_DIR     默认: $PROJECT_ROOT/build
#   DEST_DIR      默认: $HOME/App/Qt/SmartScope_min
#   COPY_LDD      默认: 1 (通过 ldd 复制第三方依赖)
#   ARCHIVE_OUT   默认: $HOME/App/Qt/SmartScope_min_YYYYmmdd_HHMMSS.tar.gz

PROJECT_ROOT="${PROJECT_ROOT:-/home/eddysun/projects/SmartScope}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
DEST_DIR="${DEST_DIR:-$HOME/App/Qt/SmartScope_min}"
COPY_LDD="${COPY_LDD:-1}"
TS=$(date +%Y%m%d_%H%M%S)
ARCHIVE_OUT="${ARCHIVE_OUT:-$HOME/App/Qt/SmartScope_min_${TS}.tar.gz}"

echo "[package:min] PROJECT_ROOT = $PROJECT_ROOT"
echo "[package:min] BUILD_DIR    = $BUILD_DIR"
echo "[package:min] DEST_DIR     = $DEST_DIR"
echo "[package:min] COPY_LDD     = $COPY_LDD"
echo "[package:min] ARCHIVE_OUT  = $ARCHIVE_OUT"

# 0) 进入项目根目录并打印当前目录
cd "$PROJECT_ROOT"
pwd

# 1) 准备构建（如需）
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  echo "[package:min] 配置 CMake (Release) ..."
  cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
else
  echo "[package:min] 检测到已存在 CMake 配置，跳过配置"
fi

# 2) 编译
echo "[package:min] 开始编译 ..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

# 3) 生成最小化部署目录
echo "[package:min] 生成最小化部署目录 ..."
SOURCE_DIR="$PROJECT_ROOT" \
BUILD_DIR="$BUILD_DIR" \
DEST_DIR="$DEST_DIR" \
COPY_LDD="$COPY_LDD" \
bash "$PROJECT_ROOT/scripts/root_scripts/deploy_minimal_smartscope.sh"

# 4) 打包 tar.gz
out_dir=$(dirname "$ARCHIVE_OUT")
mkdir -p "$out_dir"

cd "$(dirname "$DEST_DIR")"
pwd

base_name="$(basename "$DEST_DIR")"
if [[ ! -d "$base_name" ]]; then
  echo "[package:min] ERROR: 部署目录不存在: $(pwd)/$base_name" >&2
  exit 1
fi

# 使用相对路径打包，避免归档中包含绝对路径
echo "[package:min] 打包 -> $ARCHIVE_OUT"
tar czf "$ARCHIVE_OUT" "$base_name"

# 5) 完成
echo "[package:min] 完成。归档文件: $ARCHIVE_OUT"
