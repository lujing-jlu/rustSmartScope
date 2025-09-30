#!/usr/bin/env bash
set -Eeuo pipefail

# 最小化部署脚本：仅拷贝运行所需内容（bin + 必要 .so + 模型 + 标定/配置）
# 可配置环境变量：
#   SOURCE_DIR  默认: /home/eddysun/App/Qt/stereo_depth_calibration/reference_code/SmartScope
#   BUILD_DIR   默认: $SOURCE_DIR/build
#   DEST_DIR    默认: $HOME/App/Qt/stereo_depth_calibration/SmartScope
#   COPY_LDD    默认: 0  (是否通过 ldd 复制第三方依赖)

SOURCE_DIR_DEFAULT="/home/eddysun/projects/SmartScope"
SOURCE_DIR="${SOURCE_DIR:-$SOURCE_DIR_DEFAULT}"
BUILD_DIR="${BUILD_DIR:-$SOURCE_DIR/build}"
DEST_DIR_DEFAULT="$HOME/App/Qt/SmartScope"
DEST_DIR="${DEST_DIR:-$DEST_DIR_DEFAULT}"
COPY_LDD="${COPY_LDD:-0}"

STEREO_DEPTH_LIB_DIR="$SOURCE_DIR/src/stereo_depth_lib"

echo "[deploy:min] SOURCE_DIR  = $SOURCE_DIR"
echo "[deploy:min] BUILD_DIR   = $BUILD_DIR"
echo "[deploy:min] DEST_DIR    = $DEST_DIR"
echo "[deploy:min] COPY_LDD    = $COPY_LDD"

# 校验
[[ -d "$SOURCE_DIR" ]] || { echo "[deploy:min] ERROR: SOURCE_DIR not found" >&2; exit 1; }
[[ -d "$BUILD_DIR" ]] || { echo "[deploy:min] ERROR: BUILD_DIR not found (请先构建)" >&2; exit 1; }
[[ -x "$BUILD_DIR/SmartScopeQt" ]] || { echo "[deploy:min] ERROR: binary not found: $BUILD_DIR/SmartScopeQt" >&2; exit 1; }

# 将可执行文件放到根目录（不使用 bin）
BIN_DIR="$DEST_DIR"
LIB_DIR="$DEST_DIR/lib"
MODELS_DIR="$DEST_DIR/models"
CALIB_DIR="$DEST_DIR/camera_parameters"
CONF_DIR="$DEST_DIR/config"
mkdir -p "$BIN_DIR" "$LIB_DIR" "$CONF_DIR"

# 1) 复制二进制（到根目录）
install -m 0755 "$BUILD_DIR/SmartScopeQt" "$BIN_DIR/"

# 2) 复制项目内必要共享库
for so in librknn_api.so librknnrt.so librga.so; do
    if [[ -f "$SOURCE_DIR/lib/$so" ]]; then
        install -m 0644 "$SOURCE_DIR/lib/$so" "$LIB_DIR/"
        echo "[deploy:min] + lib/$so"
    else
        echo "[deploy:min] WARN: missing $SOURCE_DIR/lib/$so"
    fi
done

ST_LIB_BUILD_LIB="$STEREO_DEPTH_LIB_DIR/build/lib"
if [[ -d "$ST_LIB_BUILD_LIB" ]]; then
    find "$ST_LIB_BUILD_LIB" -maxdepth 1 -type f -name "*.so*" -print0 | while IFS= read -r -d '' f; do
        install -m 0644 "$f" "$LIB_DIR/"
        echo "[deploy:min] + $(basename "$f")"
    done
else
    echo "[deploy:min] WARN: missing $ST_LIB_BUILD_LIB"
fi

# 3) 模型/权重（如存在）——只要找到第一个非空候选就复制到 models/
model_copied=0
models_dirs=( \
    "$SOURCE_DIR/models" \
    "$SOURCE_DIR/../models" \
    "$BUILD_DIR/models" \
    "$STEREO_DEPTH_LIB_DIR/build/models" \
    "$STEREO_DEPTH_LIB_DIR/build/assets" \
)
for mdir in "${models_dirs[@]}"; do
    if [[ -d "$mdir" ]] && find "$mdir" -type f -mindepth 1 -print -quit >/dev/null 2>&1; then
        mkdir -p "$MODELS_DIR"
        rsync -avh --delete "$mdir/" "$MODELS_DIR/"
        echo "[deploy:min] + models from $mdir -> $MODELS_DIR"
        model_copied=1
        break
    fi
done
if [[ "$model_copied" -eq 0 ]]; then
    echo "[deploy:min] WARN: no models found in candidates: ${models_dirs[*]}" >&2
fi

# 4) 标定文件：复制 camera_parameters（若存在）到部署包根同级目录
for cdir in "$SOURCE_DIR/camera_parameters" "$SOURCE_DIR/../camera_parameters"; do
    if [[ -d "$cdir" ]]; then
        rsync -avh --delete "$cdir/" "$CALIB_DIR/"
        echo "[deploy:min] + camera_parameters from $cdir"
        break
    fi
done

# 5) 配置文件：尝试收集应用配置（ini/toml/json）到 config/
for cfg in "$SOURCE_DIR/app.toml" "$SOURCE_DIR/config.ini" "$SOURCE_DIR/resources/app.toml" "$SOURCE_DIR/resources/config.ini"; do
    if [[ -f "$cfg" ]]; then
        install -m 0644 "$cfg" "$CONF_DIR/"
        echo "[deploy:min] + config $(basename "$cfg")"
    fi
done

# 6) 可选：通过 ldd 复制第三方依赖到 lib/
copy_ldd_for() {
    local target="$1"
    echo "[deploy:min] ldd scan: $target"
    ldd "$target" | awk '/=>/ {print $(NF-1)}' | while read -r dep; do
        [[ -z "$dep" ]] && continue
        # 仅排除核心系统运行时，其他一律打包
        case "$dep" in
            *linux-vdso.so*|/lib*/ld-linux*.so*|/lib*/ld-musl*.so*|/usr/lib*/ld-linux*.so*|/usr/lib*/ld-musl*.so*) continue;;
            */libc.so.*|*/libm.so.*|*/librt.so.*|*/libdl.so.*|*/libpthread.so.*) continue;;
        esac
        if [[ -f "$dep" ]]; then
            local base; base=$(basename "$dep")
            if [[ ! -f "$LIB_DIR/$base" ]]; then
                install -m 0644 "$dep" "$LIB_DIR/$base"
                echo "[deploy:min]   + dep $base"
            fi
        fi
    done
}

if [[ "$COPY_LDD" == "1" ]]; then
    copy_ldd_for "$BIN_DIR/SmartScopeQt"
    find "$LIB_DIR" -maxdepth 1 -type f -name "*.so*" -print0 | while IFS= read -r -d '' sofile; do
        copy_ldd_for "$sofile"
    done
fi

# 6.1 强化：直接抓取 glog/gflags 库（保留符号链接），避免遗漏
copy_lib_glob() {
    local pattern="$1"
    local copied=0
    for d in /usr/lib /usr/lib64 /usr/local/lib /lib /usr/lib/aarch64-linux-gnu /lib/aarch64-linux-gnu; do
        if compgen -G "$d/$pattern" > /dev/null; then
            for f in $d/$pattern; do
                if [[ -e "$f" ]]; then
                    local base=$(basename "$f")
                    if [[ ! -e "$LIB_DIR/$base" ]]; then
                        # 保留符号链接与目标
                        cp -a "$f" "$LIB_DIR/" || true
                        echo "[deploy:min]   + force copy: $base"
                        copied=1
                    fi
                fi
            done
        fi
    done
    return $copied
}
# 尝试拷贝 glog/gflags 全套符号链
copy_lib_glob "libglog.so*" || true
copy_lib_glob "libgflags.so*" || true
# 强制拷贝 OpenGL/EGL 相关库，避免 Qt XCB 无法创建上下文
copy_lib_glob "libEGL.so*" || true
copy_lib_glob "libGL.so*" || true
copy_lib_glob "libGLX*.so*" || true
copy_lib_glob "libglapi.so*" || true
copy_lib_glob "libGLESv2.so*" || true
copy_lib_glob "libGLESv1*_CM.so*" || true
copy_lib_glob "libgbm.so*" || true

# 7) 启动脚本与Qt插件
# 7.1 复制Qt插件（platforms, imageformats, xcbglintegrations），优先从常见路径获取
PLUGINS_DIR="$DEST_DIR/plugins"
mkdir -p "$PLUGINS_DIR"
qt_plugin_candidates=(
    "/opt/qt5.15.15/plugins"
    "/opt/qt5*/plugins"
    "/usr/lib/qt5/plugins"
    "/usr/lib/aarch64-linux-gnu/qt5/plugins"
)
for qpd in "${qt_plugin_candidates[@]}"; do
    if compgen -G "$qpd" > /dev/null; then
        for sub in platforms imageformats xcbglintegrations; do
            if [[ -d "$qpd/$sub" ]]; then
                mkdir -p "$PLUGINS_DIR/$sub"
                rsync -a "$qpd/$sub/" "$PLUGINS_DIR/$sub/"
                echo "[deploy:min] + qt plugin dir: $qpd/$sub -> plugins/$sub"
            fi
        done
        break
    fi
done

# 7.2 生成运行脚本：设置库路径、Qt插件路径，并在首次运行时创建数据目录
cat > "$DEST_DIR/run_smartscope.sh" << 'EOF'
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# 切换到程序目录，确保相对路径（camera_parameters/config/models）按应用目录解析
cd "$SCRIPT_DIR"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:${LD_LIBRARY_PATH:-}"
# Qt 插件路径（如存在）
if [ -d "$SCRIPT_DIR/plugins" ]; then
  export QT_PLUGIN_PATH="$SCRIPT_DIR/plugins"
fi
# 强制使用 XCB + EGL + OpenGL ES2，避免 GLX 依赖
export QT_QPA_PLATFORM=xcb
export QT_XCB_GL_INTEGRATION=egl
export QT_OPENGL=es2
# 数据根目录：~/data，确保子目录存在
ROOT="$HOME/data"
mkdir -p "$ROOT/Pictures" "$ROOT/Screenshots" "$ROOT/Videos"
# 运行（从根目录）
exec "$SCRIPT_DIR/SmartScopeQt" "$@"
EOF
chmod +x "$DEST_DIR/run_smartscope.sh"

echo "[deploy:min] Minimal deploy done -> $DEST_DIR"
if command -v tree >/dev/null 2>&1; then
    tree -L 2 "$DEST_DIR" | sed 's/^/[deploy:min] /'
fi 