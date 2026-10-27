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

    // 修改子窗口尺寸（触发 ConfigureNotify，smart-sizing/dynamic-resolution 据此工作）
    bool resize(quint32 window, int w, int h) const;

    // 读取子窗口当前尺寸（用于让宿主窗口与会话分辨率严格一致）
    bool windowSize(quint32 window, int *w, int *h) const;

    // 把 X 输入焦点交给子窗口（键盘直达远程会话）
    bool setFocus(quint32 window) const;

private:
    struct Private;
    Private *d = nullptr;
};

#endif  // RDPCROSS_X11HELPER_H
