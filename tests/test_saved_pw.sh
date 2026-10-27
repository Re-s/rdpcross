#!/bin/bash
# 回归测试：保存密码的连接必须直接使用已存密码（不弹凭据框、不传空密码）
#
# 复现的 bug：保存密码后双击连接 -> launch() 的 password 参数为空 ->
# !(savePassword && !password.isEmpty()) 不成立 -> 不弹凭据框 ->
# pw 始终为空 -> xfreerdp stdin 收到空行 -> 认证失败
set -u
SHADOW_PORT="${E2E_PORT:-13400}"
SHADOW_LOG="$(mktemp)"
APP_LOG="$(mktemp)"
CFGDIR="$(mktemp -d)"
BIN_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$BIN_DIR/build/rdpcross"
PROFILE_NAME="savedpw-test"
DISPLAY_TEST="${E2E_DISPLAY:-:99}"

fail() { echo "FAIL: $*"; cleanup; exit 1; }
cleanup() {
    [ -n "${SHADOW_PID:-}" ] && kill "$SHADOW_PID" 2>/dev/null
    rm -rf "$CFGDIR" "$SHADOW_LOG" "$APP_LOG"
}
trap cleanup EXIT

# 1) 影子服务器（TLS + PAM 用户）
DISPLAY="$DISPLAY_TEST" HOME=/root setsid freerdp-shadow-cli3 \
    "/port:$SHADOW_PORT" /sec:tls /bind-address:127.0.0.1 \
    >"$SHADOW_LOG" 2>&1 </dev/null &
SHADOW_PID=$!
sleep 3
ss -tln 2>/dev/null | grep -q ":$SHADOW_PORT" || fail "影子服务器未监听 $SHADOW_PORT"

# 2) 注入一个【保存了密码】的连接配置
mkdir -p "$CFGDIR/rdpcross"
cat > "$CFGDIR/rdpcross/profiles.json" <<EOF
[{
  "id": "test-savedpw-001",
  "name": "$PROFILE_NAME",
  "group": "",
  "host": "127.0.0.1:$SHADOW_PORT",
  "username": "e2euser",
  "domain": "",
  "password": "e2epass",
  "save_password": true,
  "width": 1280,
  "height": 800,
  "color_depth": 32,
  "fullscreen": false,
  "multi_monitor": false,
  "resize_mode": "smart",
  "keyboard_mode": "fullscreen",
  "audio_mode": "none",
  "audio_capture": false,
  "clipboard": true,
  "printers": false,
  "smartcards": false,
  "serial_ports": [],
  "drives": [],
  "plug_all_drives": false,
  "network": "auto",
  "persistent_bitmap_cache": true,
  "desktop_composition": true,
  "full_window_drag": true,
  "menu_animations": true,
  "visual_styles": true,
  "font_smoothing": true,
  "wallpaper": true,
  "reconnect_automatically": false,
  "alternate_shell": "",
  "shell_working_dir": "",
  "security": "tls",
  "cert_mode": "ignore",
  "gateway": {"enabled": false, "host": "", "username": "", "domain": "", "password": ""},
  "notes": "",
  "last_used": 0.0
}]
EOF

# 3) 用已保存 profile 直连（等价于主窗口双击）
DISPLAY="$DISPLAY_TEST" HOME="$HOME" XDG_CONFIG_HOME="$CFGDIR" \
    setsid stdbuf -oL -eL timeout -s INT 20 \
    "$BIN" --launch-profile "$PROFILE_NAME" >"$APP_LOG" 2>&1 </dev/null &
sleep 10

# 4) 判定
grep -q "connected" "$APP_LOG" || { echo "-- 客户端输出:"; cat "$APP_LOG"; fail "未连接（保存密码未生效）"; }
grep -q "pam_authenticate failure" "$SHADOW_LOG" && fail "PAM 认证失败（空密码特征）"
echo "PASS: 保存密码的连接直接使用已存密码，无需凭据框"
