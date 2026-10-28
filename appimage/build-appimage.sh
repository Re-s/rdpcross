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
# 关键：必须传「二进制」而不是 AppDir 目录。
# 只有传二进制，quick-sharun 才会对它跑 ldd、在其 NEEDED_LIBS 里
# 检测到 libQt*Gui.so，进而触发 DEPLOY_QT 分支 —— 该分支会把
# qt6/plugins/platforms/* 的全部平台插件（libqxcb.so/libqoffscreen.so/
# libqwayland-*.so 等）部署进 AppDir。若传目录，平台插件只能靠
# STRACE（LD_DEBUG=libs 运行时收集），在某些 CI 环境（xvfb/Qt 行为
# 差异）下收集不到，AppImage 缺 xcb 插件 → 用户机器崩溃。
echo "-- quick-sharun 部署依赖（含 glibc）..."
export APPDIR="$APPDIR"
export APPIMAGETOOL="$HERE/appimagetool"
export OUTPATH="$OUT"
"$HERE/quick-sharun.sh" "$APPDIR/usr/bin/rdpcross" || true   # 末尾的警告不影响产物

# 3) 剔除本程序不需要的平台插件减体积
# rdpcross 靠 X11 嵌入 freerdp 子窗口（XEmbed），wayland 原生平台不可用；
# eglfs/linuxfb/vnc/kvkhrdisplay 均非桌面场景所需，保留 xcb + offscreen + minimal
echo "-- 精简平台插件..."
rm -rf "$APPDIR"/lib/qt6/plugins/platforms/libqwayland-*.so \
        "$APPDIR"/lib/qt6/plugins/wayland-* \
        "$APPDIR"/lib/qt6/plugins/platforms/libqeglfs.so \
        "$APPDIR"/lib/qt6/plugins/platforms/libqlinuxfb.so \
        "$APPDIR"/lib/qt6/plugins/platforms/libqvnc.so \
        "$APPDIR"/lib/qt6/plugins/platforms/libqvkkhrdisplay.so 2>/dev/null || true
ls "$APPDIR"/lib/qt6/plugins/platforms/ 2>/dev/null | sed 's/^/  platform: /'

# 3) 打包（低压缩级别防 OOM）
mkdir -p "$OUT"
echo "-- 生成 AppImage..."
cd "$HERE"
VERSION="$VERSION" DWARFS_COMP="zstd:level=3" ./appimagetool "$APPDIR" -o "$OUT"

echo "-- 完成："
ls -lh "$OUT"/*.AppImage
