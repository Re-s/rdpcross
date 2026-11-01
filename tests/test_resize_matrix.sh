#!/bin/bash
# 分辨率矩阵回归测试（黑边/铺满）
# 用法: bash tests/test_resize_matrix.sh [显示器]
# 需要预先启动: Xvfb :99 (1920x1080, 已画四象限) + freerdp-shadow-cli3 端口13398
set -u
cd "$(dirname "$0")/.."
DISP="${1:-:100}"
PY=python3
PASS=0; FAIL=0

run_case() {
    local label="$1" mode="$2" win_w="$3" win_h="$4" session_w="$5" session_h="$6" min_fill="$7"
    kill_client
    rm -f /tmp/_case.log
    DISPLAY=$DISP RDPCROSS_RESIZE=$mode RDPCROSS_TLS=1 RDPCROSS_CERT_IGNORE=1 \
        setsid nohup ./build/rdpcross --quick 127.0.0.1:13398 e2euser e2epass \
        >/tmp/_case.log 2>&1 </dev/null &
    sleep 9
    local wid
    wid=$(DISPLAY=$DISP xwininfo -root -children 2>/dev/null | grep "127.0.0.1:13398" | awk '{print $1}' | head -1)
    if [ -z "$wid" ]; then
        echo "  ✗ $label  [客户端未启动]"; FAIL=$((FAIL+1)); kill_client; return
    fi
    if [ "$win_w" != "0" ]; then
        DISPLAY=$DISP xdotool windowsize "$wid" "$win_w" "$win_h" 2>/dev/null
        DISPLAY=$DISP xdotool windowmove "$wid" 0 0 2>/dev/null
        sleep 4
    fi
    # 测量子窗口（内容）相对视图的几何
    local result
    result=$(DISPLAY=$DISP $PY - "$DISP" <<'PY'
import sys, subprocess, json, re
disp = sys.argv[1]
out = subprocess.run(['xwininfo','-display',disp,'-root','-children'],
                     capture_output=True,text=True).stdout
top = None
for line in out.splitlines():
    if line.strip().startswith('0x') and '13398' in line:
        top = line.strip().split()[0]; break
if not top:
    print(json.dumps({"err":"no top"})); sys.exit()
# 查 top 的子树，找 xfreerdp 渲染窗口
q = subprocess.run(['xwininfo','-display',disp,'-id',top,'-tree'],
                   capture_output=True,text=True).stdout
child = None
for line in q.splitlines():
    if '"xfreerdp"' in line:
        m = re.search(r'(\d+)x(\d+)\+(\d+)\+(\d+)', line)
        if m:
            child = [int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4))]
            break
if child is None:
    print(json.dumps({"err":"no xfreerdp child","top":top})); sys.exit()
print(json.dumps({"child":child}))
PY
)
    local cw ch cx cy
    cw=$(echo "$result" | $PY -c "import json,sys; d=json.load(sys.stdin); print(d['child'][0])" 2>/dev/null)
    ch=$(echo "$result" | $PY -c "import json,sys; d=json.load(sys.stdin); print(d['child'][1])" 2>/dev/null)
    cx=$(echo "$result" | $PY -c "import json,sys; d=json.load(sys.stdin); print(d['child'][2])" 2>/dev/null)
    cy=$(echo "$result" | $PY -c "import json,sys; d=json.load(sys.stdin); print(d['child'][3])" 2>/dev/null)
    if [ -z "$cw" ]; then
        echo "  ✗ $label  [几何读取失败] $result"; FAIL=$((FAIL+1)); kill_client; return
    fi
    # 视图区 = 顶层窗口内 - 工具条高度(假设约 30~34)。简单起见：取 top 减去已知 chrome
    # 更准确：用 view = child 的父容器（最大那个窗口已在 wins[0]）
    local vw vh
    vw=$(DISPLAY=$DISP xwininfo -id "$wid" 2>/dev/null | grep -E "^[[:space:]]+Width" | awk '{print $2}')
    vh=$(DISPLAY=$DISP xwininfo -id "$wid" 2>/dev/null | grep -E "^[[:space:]]+Height" | awk '{print $2}')
    # 判定：子窗口面积覆盖 session_w x session_h 的比例；对称黑边算合法居中
    # 1) smart: 期望子窗口 == 视图区 (cw>=vw-4, ch>=vh-38)
    # 2) dynamic: 期望 cw==session_w 且 ch==session_h （服务器固定分辨率，客户端应居中不改尺寸）
    local ok=0; local detail=""
    if [ "$mode" = "smart" ]; then
        if [ "$cw" -ge $((win_w - 6)) ] && [ "$ch" -ge $((win_h - 40)) ]; then
            ok=1; detail="子窗口 ${cw}x${ch} ≈ 视图 ${win_w}x${win_h} 缩放铺满"
        else
            detail="子窗口 ${cw}x${ch} < 视图 ${win_w}x${win_h} 未铺满"
        fi
    elif [ "$mode" = "dynamic" ]; then
        # shadow 不支持动态分辨率：子窗口必须保持会话原分辨率（内容 1:1 完整）
        if [ "$cw" -eq "$session_w" ] && [ "$ch" -eq "$session_h" ]; then
            ok=1; detail="子窗口 ${cw}x${ch} == 会话 ${session_w}x${session_h} 内容完整"
        elif [ "$cw" -ge $((session_w - 4)) ] && [ "$ch" -ge $((session_h - 4)) ] && \
             [ "$cw" -le $((session_w + 4)) ] && [ "$ch" -le $((session_h + 4)) ]; then
            ok=1; detail="子窗口 ${cw}x${ch} ≈ 会话 ${session_w}x${session_h} 内容完整"
        else
            detail="子窗口 ${cw}x${ch} ≠ 会话 ${session_w}x${session_h}（内容被压扁/拉伸）"
        fi
    fi
    if [ "$ok" = "1" ]; then
        echo "  ✓ $label  $detail"; PASS=$((PASS+1))
    else
        echo "  ✗ $label  $detail"; FAIL=$((FAIL+1))
    fi
    kill_client
}

kill_client() {
    for p in $(pgrep -f "build/rdpcross" 2>/dev/null); do kill "$p" 2>/dev/null; done
    sleep 0.7
}

echo "=== 分辨率矩阵测试 (DISPLAY=$DISP) ==="
echo "--- smart 模式（应缩放铺满所有尺寸） ---"
run_case "smart 会话1920x1080 窗口1280x800"  smart 1280 800  1920 1080 100
run_case "smart 会话1920x1080 窗口1920x1080" smart 1920 1080 1920 1080 100
run_case "smart 会话1920x1080 窗口2400x1300" smart 2400 1300 1920 1080 100
echo "--- dynamic 模式（shadow 不支持动态分辨率 → 内容应完整居中） ---"
run_case "dynamic 会话1920x1080 窗口1280x800"  dynamic 1280 800  1920 1080 100
run_case "dynamic 会话1920x1080 窗口2400x1300" dynamic 2400 1300 1920 1080 100
echo "=== 结果: PASS=$PASS FAIL=$FAIL ==="
[ "$FAIL" = "0" ]
