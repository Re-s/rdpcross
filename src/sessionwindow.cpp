#include "sessionwindow.h"

#include "constants.h"
#include "config.h"

#include <QApplication>
#include <QGuiApplication>
#include <QToolBar>
#include <QStatusBar>
#include <QScrollArea>
#include <QMessageBox>
#include <QKeySequence>
#include <QShortcut>
#include <QTimer>
#include <QCursor>
#include <QInputDialog>
#include <QTextEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QWindowStateChangeEvent>
#include <QScreen>

// 全屏中的浮动工具条（按 F8 或鼠标移到屏幕顶部唤出）
class FloatingBar : public QWidget {
    Q_OBJECT
public:
    explicit FloatingBar(SessionWindow *parent)
        : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint |
                          Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus) {
        m_owner = parent;
        setAttribute(Qt::WA_KeyboardFocusChange, false);
        setFocusPolicy(Qt::NoFocus);
        setAutoFillBackground(true);
        auto *lay = new QHBoxLayout(this);
        lay->setContentsMargins(4, 2, 4, 2);
        m_status = new QLabel(this);
        lay->addWidget(m_status);
        auto *bF = new QPushButton(tr("全屏(F11)"), this);
        auto *bR = new QPushButton(tr("重连"), this);
        auto *bC = new QPushButton(tr("断开"), this);
        connect(bF, &QPushButton::clicked, parent, &SessionWindow::toggleFullscreen);
        connect(bR, &QPushButton::clicked, parent, &SessionWindow::reconnect);
        connect(bC, &QPushButton::clicked, parent, [this] { m_owner->maybeClose(); });
        lay->addWidget(bF);
        lay->addWidget(bR);
        lay->addWidget(bC);
        adjustSize();
    }

    void setStatus(const QString &t) { m_status->setText("  " + t + "  "); adjustSize(); }

private:
    SessionWindow *m_owner = nullptr;
    QLabel *m_status = nullptr;
};

SessionWindow::SessionWindow(const ConnectionProfile &profile, const QString &password,
                             bool startNow, QWidget *parent)
    : QMainWindow(parent), m_profile(profile), m_password(password) {
    setWindowTitle(profile.displayTitle() + " - RDPCross");
    // 初始尺寸先粗估，真正对齐发生在 showEvent / 连接建立后的 fitWindowToSession()
    if (profile.width > 0 && profile.height > 0)
        resize(profile.width + 40, profile.height + 40);

    // --- 工具条 ---
    auto *tb = new QToolBar("会话", this);
    tb->setMovable(false);
    addToolBar(Qt::TopToolBarArea, tb);

    m_statusLabel = new QLabel("正在建立连接…", this);
    tb->addWidget(m_statusLabel);

    m_actFullscreen = new QAction("切换全屏", this);
    m_actFullscreen->setShortcut(QKeySequence("F11"));
    connect(m_actFullscreen, &QAction::triggered, this, &SessionWindow::toggleFullscreen);
    tb->addAction(m_actFullscreen);

    auto *actDiag = new QAction("诊断日志", this);
    connect(actDiag, &QAction::triggered, this, &SessionWindow::showLog);
    tb->addAction(actDiag);

    auto *actOneToOne = new QAction("原始大小", this);
    connect(actOneToOne, &QAction::triggered, this, [this] {
        if (m_full) leaveFullscreen();
        if (isMaximized()) showNormal();
        const QSize sz = m_view ? m_view->sessionSize() : QSize();
        fitWindowToSession(sz.isValid() ? sz
                                        : QSize(m_profile.width, m_profile.height));
    });
    m_actOneToOne = actOneToOne;
    tb->addAction(m_actOneToOne);

    m_actReconnect = new QAction("重新连接", this);
    connect(m_actReconnect, &QAction::triggered, this, &SessionWindow::reconnect);
    tb->addAction(m_actReconnect);

    auto *actDisc = new QAction("断开", this);
    connect(actDisc, &QAction::triggered, this, [this] { maybeClose(); });
    tb->addAction(actDisc);

    // --- 会话视图 ---
    m_scroll = new QScrollArea(this);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_scroll->setBackgroundRole(QPalette::Dark);
    // 视图随视口伸缩：smart 模式由 FreeRDP 缩放，dynamic 模式由服务器改分辨率，
    // 两种都不会留下黑边；fixed 模式靠 m_view 的固定尺寸触发滚动条。
    m_scroll->setWidgetResizable(true);
    setCentralWidget(m_scroll);

    m_floating = new FloatingBar(this);
    m_floating->hide();
    connect(&m_floatbarTimer, &QTimer::timeout, this, [this] { if (m_full) m_floating->hide(); });

    // 快捷键：F8 唤出浮动工具条；Esc 退出全屏
    auto *scF8 = new QShortcut(QKeySequence("F8"), this);
    connect(scF8, &QShortcut::activated, this, [this] {
        if (m_floating->isVisible()) m_floating->hide(); else showFloatbar();
    });
    auto *scEsc = new QShortcut(QKeySequence("Esc"), this);
    connect(scEsc, &QShortcut::activated, this, [this] {
        if (m_full) leaveFullscreen(); else maybeClose();
    });
    // Ctrl+Alt+End 提示（FreeRDP 子窗口聚焦时由 FreeRDP 自身映射为 Ctrl+Alt+Delete）
    auto *sc_hint = new QShortcut(QKeySequence("Ctrl+Alt+End"), this);
    connect(sc_hint, &QShortcut::activated, this, [this] {
        QMessageBox::information(this, "快捷键提示",
            "Ctrl+Alt+End 已由 FreeRDP 内置映射为远程 Ctrl+Alt+Delete。请先单击远程桌面区域"
            "让子窗口获得键盘焦点。");
    });

    if (startNow) buildUi();
}

SessionWindow::~SessionWindow() = default;

void SessionWindow::buildUi() {
    if (m_view) {
        m_view->stopAndKill();
        delete m_view;
        m_view = nullptr;
    }
    m_wantReconnect = false;
    m_view = new RdpView(m_profile, m_password, nullptr);
    connect(m_view, &RdpView::statusChanged, this, &SessionWindow::onStatus);
    connect(m_view, &RdpView::sessionFinished, this, &SessionWindow::onFinished);
    connect(m_view, &RdpView::connected, this, [this] { onStatus("已连接：" + m_profile.host); });
    connect(m_view, &RdpView::sessionSizeChanged, this, [this](const QSize &sz) {

        // 服务器给出的真实分辨率：窗口跟着精确贴合（用户已最大化/全屏时不打扰）
        fitWindowToSession(sz);
    });
    if (m_profile.resizeMode == constants::kResizeFixed)
        m_view->setFixedSize(m_profile.width, m_profile.height);
    else
        m_view->setMinimumSize(0, 0);
    m_scroll->setWidget(m_view);
    m_view->show();
    m_view->start();
}

void SessionWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);

    if (m_sizedOnce || m_full || isMaximized()) return;
    // 首次显示：把窗口精确调到会话分辨率（此刻才知道边框/工具条占多少）
    const QSize want = m_pendingFit.isValid() ? m_pendingFit
                                              : QSize(m_profile.width, m_profile.height);
    if (want.width() > 0 && want.height() > 0 && !m_profile.fullscreen) {
        m_sizedOnce = true;
        QTimer::singleShot(0, this, [this, want] { fitWindowToSession(want); });
    }
}

void SessionWindow::fitWindowToSession(const QSize &size, bool center) {
    if (!size.isValid() || size.width() < 64 || size.height() < 64) return;
    if (m_full || isMaximized()) return;  // 用户主动要了大窗口，不抢回去
    m_pendingFit = size;
    const QSize vp = m_scroll->viewport()->size();
    const QSize win = this->size();
    const int dw = qMax(0, win.width() - vp.width());
    const int dh = qMax(0, win.height() - vp.height());

    QSize target(size.width() + dw, size.height() + dh);
    const QRect avail = screen() ? screen()->availableGeometry() : QRect(0, 0, 1920, 1080);
    const bool tooBig = target.width() > avail.width() || target.height() > avail.height();
    if (tooBig) {
        // 放不下：按可用区域收缩（smart 会等比缩放、fixed 会出现滚动条）
        target = QSize(qMin(target.width(), avail.width()), qMin(target.height(), avail.height()));
    }
    if (this->size() != target) resize(target);
    if (center) {
        const QPoint c = avail.center();
        move(c.x() - target.width() / 2, c.y() - target.height() / 2);
    }
}

void SessionWindow::reconnect() {
    m_sizedOnce = false;
    m_wantReconnect = true;
    // 销毁旧视图（含进程）并重建
    if (m_view) {
        m_view->stopAndKill();
        delete m_view;
        m_view = nullptr;
    }
    buildUi();
}

void SessionWindow::onStatus(const QString &text) {
    m_statusLabel->setText("  " + text + "  ");
    m_floating->setStatus(text);
    if (m_view) m_view->setHint(text);
}

void SessionWindow::onFinished(const QString &reason, int exitCode) {
    onStatus(reason);
    if (m_userClosing) { realClose(); return; }
    if (m_wantReconnect) return;
    bool okExit = (exitCode == 0);
    if (!okExit) {
        QMessageBox::warning(this, "远程桌面连接已断开",
            QString("%1\n\n目标：%2\n可通过工具条「重新连接」再次连接。")
                .arg(reason, m_profile.host));
    }
    emitClosed();
    if (okExit) close();
}

// ------------------------------------------------------------------ 日志
void SessionWindow::showLog() {
    if (!m_view) return;
    QString args = m_view->maskedArgs();
    QString log = QString("目标: %1\nxfreerdp: %2\n参数(密码已隐藏): %3\n\n---- FreeRDP 日志 ----\n%4")
        .arg(m_profile.host, m_view->binaryPath(), args,
             m_view->fullLog().isEmpty() ? "(无日志)" : m_view->fullLog());
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle("诊断日志 - " + m_profile.host);
    dlg->resize(720, 480);
    auto *lay = new QVBoxLayout(dlg);
    auto *edit = new QTextEdit(dlg);
    edit->setReadOnly(true);
    edit->setPlainText(log);
    lay->addWidget(edit);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    connect(btns->button(QDialogButtonBox::Close), &QPushButton::clicked, dlg, &QDialog::reject);
    lay->addWidget(btns);
    dlg->exec();
    delete dlg;
}

// ------------------------------------------------------------------ 全屏
void SessionWindow::toggleFullscreen() {
    if (!m_full) enterFullscreen(); else leaveFullscreen();
}

void SessionWindow::enterFullscreen() {
    m_full = true;
    m_savedGeometry = geometry();
    if (isMaximized()) showNormal();
    // 多监视器：铺满虚拟桌面（mstsc「使用所有监视器」）
    if (m_profile.fullscreen && m_profile.multiMonitor) {
        m_framelessSpan = true;
        setWindowFlags(Qt::FramelessWindowHint);
        showNormal();
        setGeometry(QGuiApplication::primaryScreen()->virtualGeometry());
    } else {
        m_framelessSpan = false;
        setWindowFlags(Qt::Window);
        showNormal();
        setGeometry(screen()->geometry());
    }
    show();
    raise();
    activateWindow();
    onStatus(QString("全屏模式") + (m_framelessSpan ? " · 多监视器" : ""));
}

void SessionWindow::leaveFullscreen() {
    m_full = false;
    m_framelessSpan = false;
    setWindowFlags(Qt::Window);
    showNormal();
    setGeometry(m_savedGeometry);
    onStatus(m_view && m_view->isConnected() ? "已连接：" + m_profile.host : "窗口模式");
    raise();
    activateWindow();
}

// ------------------------------------------------------------------ 关闭
void SessionWindow::maybeClose() {
    m_wantReconnect = false;
    close();
}

void SessionWindow::forceKillAndClose() {
    // stop() 后 2.5 秒进程仍存活则强杀，随后真正关闭窗口
    if (m_view && m_view->isRunning()) {
        m_view->stopAndKill();
        QTimer::singleShot(500, this, [this] { realClose(); });
    } else {
        realClose();
    }
}

void SessionWindow::realClose() {
    if (m_closingDone) return;
    m_closingDone = true;
    if (m_view) m_view->stopAndKill();
    emitClosed();
    deleteLater();
    close();
}

void SessionWindow::emitClosed() {
    if (m_closedEmitted) return;
    m_closedEmitted = true;
    config::touchProfile(m_profile);
    emit sessionClosed(m_profile);
}

void SessionWindow::closeEvent(QCloseEvent *event) {
    if (m_closingDone) { event->accept(); return; }
    if (m_view && m_view->isRunning() && !m_userClosing) {
        // 异步停止：隐藏窗口，等 finished 后真正关闭；
        // 2.5 秒未结束则强杀（避免 GUI 主线程同步等待死锁）
        m_userClosing = true;
        m_wantReconnect = false;
        m_view->stop();
        hide();
        emitClosed();
        QTimer::singleShot(2500, this, [this] { forceKillAndClose(); });
        event->ignore();
        return;
    }
    if (m_view) m_view->stopAndKill();
    emitClosed();
    m_closingDone = true;
    event->accept();
}

// ------------------------------------------------------------------ 浮动条
void SessionWindow::keyPressEvent(QKeyEvent *event) {
    QMainWindow::keyPressEvent(event);
}

void SessionWindow::mouseMoveEvent(QMouseEvent *event) {
    QMainWindow::mouseMoveEvent(event);
    if (m_full) {
        QPointF pos = mapFromGlobal(QCursor::pos());
        if (pos.y() < 6 && !m_floating->isVisible()) scheduleFloatbar();
    }
}

void SessionWindow::scheduleFloatbar() {
    m_floatbarTimer.start(2500);
    showFloatbar();
}

void SessionWindow::showFloatbar() {
    if (!m_full) return;
    m_floating->move(8, 4);
    m_floating->show();
    m_floating->raise();
    m_floatbarTimer.start(2500);
}

#include "sessionwindow.moc"
