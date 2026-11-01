#include "x11helper.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

struct X11Helper::Private {
    Display *display = nullptr;
};

X11Helper::X11Helper() : d(new Private) {
    d->display = XOpenDisplay(nullptr);
}

X11Helper::~X11Helper() {
    if (d && d->display) XCloseDisplay(d->display);
    delete d;
}

bool X11Helper::valid() const { return d && d->display != nullptr; }

quint32 X11Helper::largestChild(quint32 parent) const {
    if (!valid()) return 0;
    Window root, parentWin;
    Window *children = nullptr;
    unsigned int n = 0;
    Status st = XQueryTree(d->display, parent, &root, &parentWin, &children, &n);
    if (!st) return 0;
    quint32 best = 0;
    int bestArea = 0;
    for (unsigned int i = 0; i < n; ++i) {
        XWindowAttributes attr;
        if (!XGetWindowAttributes(d->display, children[i], &attr)) continue;
        if (attr.map_state == IsUnmapped) continue;
        int area = attr.width * attr.height;
        if (area > bestArea) {
            bestArea = area;
            best = children[i];
        }
    }
    if (children) XFree(children);
    return best;
}

bool X11Helper::moveResize(quint32 window, int x, int y, int w, int h) const {
    if (!valid() || !window) return false;
    XWindowChanges ch;
    ch.x = x;
    ch.y = y;
    ch.width = w;
    ch.height = h;
    XConfigureWindow(d->display, window, CWX | CWY | CWWidth | CWHeight, &ch);
    XFlush(d->display);
    return true;
}

bool X11Helper::windowSize(quint32 window, int *w, int *h) const {
    if (!valid() || !window) return false;
    XWindowAttributes attr;
    if (!XGetWindowAttributes(d->display, window, &attr)) return false;
    if (w) *w = attr.width;
    if (h) *h = attr.height;
    return true;
}

bool X11Helper::windowGeometry(quint32 window, int *x, int *y, int *w, int *h) const {
    if (!valid() || !window) return false;
    XWindowAttributes attr;
    if (!XGetWindowAttributes(d->display, window, &attr)) return false;
    if (x) *x = attr.x;
    if (y) *y = attr.y;
    if (w) *w = attr.width;
    if (h) *h = attr.height;
    return true;
}

bool X11Helper::normalHints(quint32 window, int *minW, int *minH, int *maxW, int *maxH) const {
    if (!valid() || !window) return false;
    XSizeHints hints{};
    long supplied = 0;
    if (!XGetWMNormalHints(d->display, window, &hints, &supplied)) return false;
    if (minW) *minW = (supplied & PMinSize) ? hints.min_width : -1;
    if (minH) *minH = (supplied & PMinSize) ? hints.min_height : -1;
    if (maxW) *maxW = (supplied & PMaxSize) ? hints.max_width : -1;
    if (maxH) *maxH = (supplied & PMaxSize) ? hints.max_height : -1;
    return true;
}

bool X11Helper::setFocus(quint32 window) const {
    if (!valid() || !window) return false;
    XSetInputFocus(d->display, window, RevertToParent, CurrentTime);
    XFlush(d->display);
    return true;
}
