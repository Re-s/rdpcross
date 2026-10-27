#!/bin/bash
# 把 RDPCross（C++/Qt6 版）安装到系统
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${1:-/usr/local}"

if [ "$(id -u)" != "0" ]; then
    echo "需要 root 权限，请使用 sudo 运行。" >&2
    exec sudo "$0" "$@"
fi

install -d "$PREFIX/bin"
install -d "$PREFIX/share/applications"
install -d "$PREFIX/share/icons/hicolor/scalable/apps"

install -m 755 "$HERE/build/rdpcross" "$PREFIX/bin/rdpcross"
install -m 644 "$HERE/resources/rdpcross.svg" \
    "$PREFIX/share/icons/hicolor/scalable/apps/rdpcross.svg"
install -m 644 "$HERE/resources/rdpcross.desktop" \
    "$PREFIX/share/applications/rdpcross.desktop"

gtk-update-icon-cache -q "$PREFIX/share/icons/hicolor" 2>/dev/null || true
update-desktop-database -q "$PREFIX/share/applications" 2>/dev/null || true

echo "安装完成："
echo "  命令: $PREFIX/bin/rdpcross"
echo "  菜单: 应用菜单 -> RDPCross 远程桌面"
echo ""
echo "运行依赖：qt6-base-dev（运行时 libqt6-widgets6）、libx11-6、freerdp3-x11"
