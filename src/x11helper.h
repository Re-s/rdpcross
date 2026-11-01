#ifndef RDPCROSS_X11HELPER_H
#define RDPCROSS_X11HELPER_H

#include <QWidget>

// X11 辅助操作：嵌入子窗口发现、resize、输入焦点。
// 用 Xlib (libX11) 直接操作。
class X11Helper {
public:
    X11Helper();
    ~X11Helper();

    X11Helper(const X11Helper &) = delete;
    X11Helper &operator=(const X11Helper &) = delete;

    bool valid() const;

    // 在 parent 的直接子窗口中找出面积最大的（xfreerdp 的渲染窗口）
    // 找不到返回 0
    quint32 largestChild(quint32 parent) const;

    // 移动并修改子窗口尺寸（相对父窗口坐标，用于会话分辨率小于视图时的居中 letterbox）
    bool moveResize(quint32 window, int x, int y, int w, int h) const;

    // 读取子窗口当前尺寸（用于让宿主窗口与会话分辨率严格一致）
    bool windowSize(quint32 window, int *w, int *h) const;

    // 读取子窗口几何（x/y 相对父窗口），用于判断子窗口是否已就位、避免重复 Configure
    bool windowGeometry(quint32 window, int *x, int *y, int *w, int *h) const;

    // 读取子窗口的 WM_NORMAL_HINTS（min/max）
    // 用途：检测服务器是否支持 RDPEDISP 动态分辨率——
    // xfreerdp 带 /dynamic-resolution 时默认写 min=1/max=16384；服务器宣告
    // DisplayControlCaps 后会改写为 min=320/max=8192（见 FreeRDP xf_disp.c）
    bool normalHints(quint32 window, int *minW, int *minH, int *maxW, int *maxH) const;

    // 把 X 输入焦点交给子窗口（键盘直达远程会话）
    bool setFocus(quint32 window) const;

private:
    struct Private;
    Private *d = nullptr;
};

#endif  // RDPCROSS_X11HELPER_H
