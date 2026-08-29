#!/usr/bin/env bash
# ============================================================
#  TrialSpawnerFinder C++ 核心模块构建脚本（Linux / GCC）
#
#  用法:
#    ./build.sh              仅核心库（Release）
#    ./build.sh --test       构建并运行 GoogleTest 单测
#    ./build.sh --bench      构建并运行性能基准（trialfinder_bench）
#    ./build.sh --jni        构建 JNI 桥接动态库（需 JDK 25）
#    ./build.sh --debug      Debug 构建
# ============================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/build"
CONFIG=Release

for arg in "$@"; do
  case "$arg" in
    --debug) CONFIG=Debug ;;
    --test)  CMAKE_OPTS="$CMAKE_OPTS -DBUILD_TESTING=ON" ;;
    --bench) CMAKE_OPTS="$CMAKE_OPTS -DBUILD_BENCH=ON" ;;
    --jni)   CMAKE_OPTS="$CMAKE_OPTS -DBUILD_JNI=ON" ;;
    *) echo "未知参数: $arg" >&2; exit 1 ;;
  esac
done

if ! command -v cmake >/dev/null 2>&1; then
  echo "[错误] 未找到 cmake，请安装 CMake 3.20+." >&2
  exit 1
fi

echo "[1/3] 配置 CMake ... $CMAKE_OPTS"
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG" $CMAKE_OPTS

echo "[2/3] 构建 ..."
cmake --build "$BUILD_DIR" --config "$CONFIG" -j

echo "[3/3] 构建完成。产物目录: $BUILD_DIR/bin"
if [[ "$*" == *--test* ]]; then
  echo
  echo "====== 运行单元测试 ======"
  "$BUILD_DIR/bin/trialfinder_tests"
fi
if [[ "$*" == *--bench* ]]; then
  echo
  echo "====== 运行性能基准 ======"
  "$BUILD_DIR/bin/trialfinder_bench"
fi
