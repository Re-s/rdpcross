#!/bin/bash
# 一键构建 Anylinux AppImage（官方流程：quick-sharun + uruntime）
#
# 关键点（踩坑经验）：
# 1. 必须用 quick-sharun 把 glibc/ld-linux 一起打进 AppDir，否则旧系统报
#    GLIBC_2.39 not found（传统 AppImage 只拷应用层 .so 的错误做法）
# 2. mkdwarfs 默认 zstd:level=22 压缩在小内存机器上会被 OOM 杀死，
#    用 DWARFS_COMP="zstd:level=3" 控制内存（28MB vs 674MB）
# 3. quick-sharun 需要 patchelf
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
VERSION="${1:-2.0.0}"
APPDIR="$HERE/AppDir"
OUT="$ROOT/dist"

command -v patchelf >/dev/null || { echo "需要 patchelf：apt install patchelf" >&2; exit 1; }
[ -x "$ROOT/build/rdpcross" ] || { echo "请先编译：cmake -B build -G Ninja && cmake --build build -j1" >&2; exit 1; }

# 0) appimagetool（full 版内嵌 mkdwarfs/uruntime）
if [ ! -x "$HERE/appimagetool" ]; then
    echo "-- 下载 appimagetool 0.5.3（full）..."
    curl -sL --retry 5 -o "$HERE/appimagetool" \
        "https://github.com/pkgforge-dev/appimagetool/releases/download/0.5.3/appimagetool-full-x86_64-linux"
    chmod +x "$HERE/appimagetool"
fi

# 1) 干净的最小 AppDir：只放二进制 + desktop + 图标，依赖交给 quick-sharun
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
install -m 755 "$ROOT/build/rdpcross" "$APPDIR/usr/bin/rdpcross"
install -m 644 "$ROOT/resources/rdpcross.desktop" "$APPDIR/"
install -m 644 "$ROOT/resources/rdpcross.svg" "$APPDIR/.DirIcon"

# 2) quick-sharun：部署 sharun + glibc + Qt6 全部依赖，生成 AppRun
echo "-- quick-sharun 部署依赖（含 glibc）..."
export APPDIR="$APPDIR"
export APPIMAGETOOL="$HERE/appimagetool"
export OUTPATH="$OUT"
"$HERE/quick-sharun.sh" "$APPDIR" || true   # 末尾的警告不影响产物

# 3) 打包（低压缩级别防 OOM）
mkdir -p "$OUT"
echo "-- 生成 AppImage..."
cd "$HERE"
VERSION="$VERSION" DWARFS_COMP="zstd:level=3" ./appimagetool "$APPDIR" -o "$OUT"

echo "-- 完成："
ls -lh "$OUT"/*.AppImage
