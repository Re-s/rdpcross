#!/bin/bash
# RDPCross 构建脚本（支持 x86_64 / arm64）
#
# 用法：
#   ./build.sh              # x86_64，默认
#   ./build.sh --arch arm64 # 交叉编译/系统 native arm64
#
# 依赖（Debian/Ubuntu）：
#   sudo apt install -y build-essential cmake ninja-build pkg-config patchelf \
#     libqt6core6t64-dev libqt6widgets6t64-dev libfreerdp3-dev qt6-base-dev \
#     libasound2-dev libpulse-dev libx11-dev
#
# arm64 上再装交叉编译器（aarch64-linux-gnu-gcc，仅 x86_64 主机需要）

set -u

ARCH="x86_64"
while [ $# -gt 0 ]; do
    case "$1" in
        --arch) ARCH="$2"; shift 2 ;;
        --arch=*) ARCH="${1#--arch=}"; shift ;;
        -h|--help) echo "用法: $0 [--arch x86_64|arm64]"; exit 0 ;;
        *) echo "未知参数: $1"; exit 1 ;;
    esac
done

TOOLCHAIN=""
CMAKE=""
if [ "$ARCH" = "arm64" ]; then
    if [ "$(dpkg --print-architecture)" = "amd64" ]; then
        TOOLCHAIN="aarch64-linux-gnu"
        CMAKE="-DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/Toolchain-arm64.cmake"
        echo "-- arm64 交叉编译（工具链: $TOOLCHAIN）"
    else
        echo "-- arm64 本地构建"
    fi
fi

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release $CMAKE
ninja -C build -j1

echo "build OK: build/rdpcross (target $ARCH)"
