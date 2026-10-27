# RDPCross 远程桌面（C++/Qt6 版）

Linux 平台的 Windows RDP 远程桌面客户端，功能与交互对齐 Windows 官方 mstsc（远程桌面连接），
底层基于 [FreeRDP](https://github.com/FreeRDP/FreeRDP)（xfreerdp3）。

这是 C++/Qt6 重构版；同仓库另有经过完整验证的 Python/PyQt6 参照实现（`../linux-rdp-client`）。

## 功能

- 连接管理：分组树、快速连接、.rdp 文件导入/导出、双击连接
- 显示：分辨率/颜色深度/全屏多监视器、智能缩放（smart-sizing）、动态分辨率
- 体验：壁纸/主题/字体平滑/窗口拖拽/菜单动画/桌面组合开关，持久位图缓存
- 资源重定向：剪贴板、音频（本地/远程/不播放）、打印机、智能卡、串口、磁盘重定向
- 安全：NLA/TLS/RDP/AAD 安全层，证书策略（询问/信任/忽略/拒绝），RD 网关
- 会话窗口：工具条、F11 全屏、悬浮工具条、诊断日志、异步断开流程
- 命令行：`rdpcross`（主窗口）、`rdpcross --connect host[:port]`、`rdpcross --quick host user pass`

## 构建与安装

```bash
# 构建依赖
sudo apt install build-essential cmake ninja-build pkg-config \
    qt6-base-dev libx11-dev freerdp3-dev
# 运行依赖
sudo apt install freerdp3-x11 qt6-base-runtime  # 或对应的运行时包

# 编译（内存较小的机器请务必单核：-j1）
cmake -B build -G Ninja
cmake --build build -j1

# 安装
sudo ./install.sh              # 安装到 /usr/local
```

## 构建 AppImage（Anylinux 方案）

```bash
./appimage/build-appimage.sh   # 产物在 dist/ 下
```

使用 [pkgforge-dev/appimagetool](https://github.com/pkgforge-dev/appimagetool)（0.5.3，full 版内嵌 mkdwarfs/uruntime）：
AppDir 内打包 Qt6 运行时与 xcb 平台插件，DWARFS 压缩（~71MiB → ~24MiB），
生成的 `RDPCross_远程桌面-<版本>-anylinux-x86_64.AppImage` 可在任意 Linux 发行版上直接下载运行，
无需安装 Qt6，也不强依赖 FUSE。

## 使用

```bash
rdpcross                                # 打开连接管理器
rdpcross --connect 192.168.1.10         # 直连（弹凭据对话框）
rdpcross --quick 192.168.1.10 user pass # 直连（跳过对话框）
```

- 配置目录：`~/.config/rdpcross/`（0600 权限）
- 密码一律经 stdin 传递（`/from-stdin:force`），不会出现在 `ps`/`/proc/*/cmdline`

## 架构

| 模块 | 职责 |
|---|---|
| `models` | ConnectionProfile 等数据模型 |
| `config` | JSON 持久化 |
| `rdpfile` | .rdp 导入/导出 |
| `freerdpargs` | xfreerdp3 参数构建、二进制查找、错误归因 |
| `x11helper` | X11 子窗口嵌入辅助 |
| `rdpview` | 嵌入视图：QProcess + 子窗口轮询 |
| `sessionwindow` | 会话窗口（工具条/全屏/浮动条/诊断日志） |
| `settingsdialog`/`credentialsdialog` | 设置（6 选项卡）/凭据对话框 |
| `mainwindow` | 连接管理器主窗口 |

## 与 mstsc 的对应关系

详细默认值见 `src/constants.h`。
