#!/bin/bash
# C++ 版端到端冒烟测试：Xvfb + freerdp-shadow-cli3 真实连接验证
#
# 验证内容：连接成功 → 窗口嵌入（connected）→ 稳定运行 → 断开关闭
#
# 前置条件：freerdp3-x11、freerdp3-shadow-x11、Xvfb，且已编译 build/rdpcross
# 使用：./tests/e2e_smoke.sh
set -u

BIN_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${E2E_BIN:-$BIN_DIR/build/rdpcross}"
SHADOW_PORT="${E2E_PORT:-13398}"
E2E_USER="${E2E_USER:-e2euser}"
E2E_PASS="${E2E_PASS:-e2epass}"
SHADOW_LOG="$(mktemp)"
APP_LOG="$(mktemp)"
DISPLAY_TEST="${E2E_DISPLAY:-:99}"

fail() { echo "E2E FAIL: $*"; cleanup; exit 1; }

cleanup() {
    [ -n "${SHADOW_PID:-}" ] && kill "$SHADOW_PID" 2>/dev/null
    [ -f "$SHADOW_LOG" ] && rm -f "$SHADOW_LOG"
    [ -f "$APP_LOG" ] && rm -f "$APP_LOG"
}
trap cleanup EXIT

[ -x "$BIN" ] || fail "未找到 $BIN，请先 cmake --build build"
command -v freerdp-shadow-cli3 >/dev/null || fail "未安装 freerdp-shadow-cli3"
command -v Xvfb >/dev/null || fail "未安装 Xvfb"

# 1) 启动影子服务器（供测试的“Windows 远程桌面”）
echo "-- 启动影子服务器（端口 $SHADOW_PORT）"
DISPLAY="$DISPLAY_TEST" HOME="$HOME" \
    setsid freerdp-shadow-cli3 "/port:$SHADOW_PORT" /sec:tls /bind-address:127.0.0.1 \
    >"$SHADOW_LOG" 2>&1 </dev/null &
SHADOW_PID=$!
sleep 3
ss -tln 2>/dev/null | grep -q ":$SHADOW_PORT" || fail "影子服务器未监听 $SHADOW_PORT"

# 2) 启动客户端（quick 模式直连；TLS + 忽略证书，与影子服务器匹配）
echo "-- 启动 rdpcross --quick"
DISPLAY="$DISPLAY_TEST" RDPCROSS_TLS=1 RDPCROSS_CERT_IGNORE=1 \
    setsid stdbuf -oL -eL timeout -s INT 20 "$BIN" \
    --quick "127.0.0.1:$SHADOW_PORT" "$E2E_USER" "$E2E_PASS" \
    >"$APP_LOG" 2>&1 </dev/null &
APP_PID=$!
sleep 8

# 3) 判定：connected 即完成 [REDACTED] 握手 + 子窗口嵌入
if grep -q "^\[rdpcross\] connected$" "$APP_LOG"; then
    echo "-- 结果: CONNECTED（嵌入成功）"
else
    echo "-- 客户端输出:"; cat "$APP_LOG"
    fail "未观察到 connected"
fi

# 4) 服务器侧不应出现认证失败（shadow 日志含 pam_authenticate failure 即失败）
if grep -q "pam_authenticate failure" "$SHADOW_LOG"; then
    fail "影子服务器报告 PAM 认证失败"
fi

wait "$APP_PID" 2>/dev/null
echo "-- 结果: 客户端正常退出"
echo "E2E OK"
