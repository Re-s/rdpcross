#include "rdpview.h"

#include "constants.h"

#include <QPainter>
#include <QLabel>
#include <QTimer>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QScreen>
#include <QMouseEvent>

static const int kPollIntervalMs = 200;
static const int kPollMaxMs = 120000;

RdpView::RdpView(const ConnectionProfile &profile, const QString &password, QWidget *parent)
    : QWidget(parent), m_profile(profile), m_password(password) {
    // 需要 native 窗口以承接 xfreerdp 的 reparent
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    setAttribute(Qt::WA_NativeWindow, true);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    m_x11 = new X11Helper();
    m_binPath = freerdpargs::findBinary();

    m_hintLabel = new QLabel("正在连接 " + profile.host + " …", this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet("color: rgb(200,200,205); background: transparent;");
    m_hintLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &RdpView::pollChild);

    // 监听 xfreerdp 渲染窗口自身的尺寸变化（服务器强制分辨率、动态分辨率等），
    // 与「我们自己 ConfigureWindow 出去的尺寸」区分开，避免自激循环。
    m_sizeTimer = new QTimer(this);
    m_sizeTimer->setInterval(300);
    connect(m_sizeTimer, &QTimer::timeout, this, &RdpView::checkChildSize);

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyRead, this, &RdpView::onReadyRead);
    connect(m_proc, &QProcess::errorOccurred, this, &RdpView::onErrorOccurred);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RdpView::onFinished);
}

RdpView::~RdpView() {
    if (m_pollTimer) m_pollTimer->stop();
    if (m_sizeTimer) m_sizeTimer->stop();
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_aborting = true;
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
}

bool RdpView::start() {
    quint64 wid = (quint64)winId();
    if (!wid) {
        emit statusChanged("无法创建嵌入窗口");
        return false;
    }
    m_parentWid = wid;
    if (m_binPath.isEmpty()) {
        emit statusChanged("未找到 xfreerdp 可执行文件（请安装 freerdp3-x11）");
        return false;
    }
    if (m_profile.host.trimmed().isEmpty()) {
        emit statusChanged("未指定目标主机");
        return false;
    }

    QStringList args = freerdpargs::buildArgs(m_profile, m_parentWid);
    emit statusChanged("正在连接 " + m_profile.host + " …");
    m_aborting = false;
    m_errorMessage.clear();
    m_proc->start(m_binPath, args);
    if (!m_proc->waitForStarted(5000)) {
        emit statusChanged("无法启动 xfreerdp 进程");
        return false;
    }
    // 密码经 stdin 写入（FreeRDP /from-stdin:force 连接前读取一行）
    m_proc->write((m_password + "\n").toUtf8());
    m_proc->closeWriteChannel();

    m_pollElapsed = 0;
    m_pollTimer->start();
    return true;
}

void RdpView::stop() {
    if (m_pollTimer) m_pollTimer->stop();
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_aborting = true;
        m_proc->terminate();
    }
}

void RdpView::stopAndKill() {
    if (m_pollTimer) m_pollTimer->stop();
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_aborting = true;
        m_proc->kill();
    }
}

void RdpView::setHint(const QString &text) {
    m_hintLabel->setText(text);
    if (!m_connected) m_hintLabel->show();
}

// ------------------------------------------------------------------ 事件
void RdpView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor(12, 12, 14));
}

void RdpView::resizeEvent(QResizeEvent *) {
    m_hintLabel->setGeometry(rect());
    m_serverLocked = false;  // 视图变了：允许重新向服务器请求该尺寸的分辨率
    fitChild();
}

void RdpView::showEvent(QShowEvent *) {
    fitChild();
}

void RdpView::mousePressEvent(QMouseEvent *) {
    focusChild();
}

void RdpView::focusInEvent(QFocusEvent *) {
    focusChild();
}

QSize RdpView::sizeHint() const {
    return QSize(m_profile.width, m_profile.height);
}

// ------------------------------------------------------------------ 内部
void RdpView::pollChild() {
    quint32 parent = (quint32)winId();
    quint32 child = parent ? m_x11->largestChild(parent) : 0;
    m_pollElapsed += kPollIntervalMs;
    if (child) {
        attachChild(child);
    } else if (m_pollElapsed > kPollMaxMs) {
        m_pollTimer->stop();
    }
}

void RdpView::attachChild(quint32 child) {
    bool first = (m_childWid != child);
    m_childWid = child;
    if (first) {
        m_pollTimer->stop();
        m_connected = true;
        m_hintLabel->hide();
        m_lastChildSize = QSize();
        m_appliedSize = QSize();
        m_sessionSize = QSize();
        m_dynamicActive = false;
        m_dynamicUnsupportedHinted = false;
        m_serverLocked = false;
        m_attachedAt.start();
        // 先按 xfreerdp 自己给出的尺寸校正宿主窗口，再让它填满视图，
        // 这样默认就是 1:1，不会出现缩放模糊/黑边。
        checkChildSize();
        m_sizeTimer->start();
        notifyChildWindowSeen();
    }
    fitChild();
}

// xfreerdp 渲染窗口的尺寸即远程桌面的真实分辨率。
// 只有「不是我们自己设的」尺寸变化才向上报告。
void RdpView::checkChildSize() {
    if (!m_childWid || !m_x11) return;
    probeDynamicActive();
    int w = 0, h = 0;
    if (!m_x11->windowSize(m_childWid, &w, &h)) return;
    if (w <= 0 || h <= 0) return;
    const QSize sz(w, h);
    if (sz != m_lastChildSize) {
        m_lastChildSize = sz;
        if (sz != m_appliedSize) {
            // 子窗口尺寸变化且不是我们配置的：权威的会话分辨率
            m_sessionSize = sz;
            // 若我们此前已拉伸/落位过，说明这是服务器对一次分辨率请求的响应：
            // 记为「支持动态分辨率」并锁定到服务器给的尺寸，直到用户再改窗口，
            // 避免「请求-回绝」来回打架。attach 时的初始尺寸不算响应。
            const bool isResponse = m_appliedSize.isValid();
            if (isResponse) {
                m_dynamicActive = true;
                m_serverLocked = true;
            }
            emit sessionSizeChanged(sz);
            // 服务器给的比当前视图小（窗口最大化等无法收缩的场景）：居中落位
            const QSize view(qMax(width(), 64), qMax(height(), 64));
            if (isResponse
                && (sz.width() < view.width() - 8 || sz.height() < view.height() - 8))
                QTimer::singleShot(0, this, &RdpView::fitChild);
        }
    }
    // dynamic 兜底：服务器既未宣告支持、也未响应过（hints 始终是默认签名），
    // 宽限期后以原会话分辨率居中，而不是让 1:1 内容堆在左上角留黑边
    if (m_profile.resizeMode == constants::kResizeDynamic && !m_dynamicActive && !m_serverLocked
        && m_sessionSize.isValid() && m_attachedAt.hasExpired(1500))
        fitChild();
}

void RdpView::probeDynamicActive() {
    if (m_dynamicActive || !m_childWid || !m_x11) return;
    int minW = -1, minH = -1, maxW = -1, maxH = -1;
    if (!m_x11->normalHints(m_childWid, &minW, &minH, &maxW, &maxH)) return;
    // xfreerdp 带 /dynamic-resolution 时默认写 min=1/max=16384（xf_ResizeDesktopWindow）；
    // 服务器宣告 DisplayControlCaps 后改写为 min=320/max=8192（xf_disp_set_window_resizable）。
    // 以「偏离默认签名」为判据，对数值变化保持容忍。
    const bool active = (maxW > 0 && maxW != 16384) || minW > 1;
    if (!active) return;
    m_dynamicActive = true;
    if (m_dynamicUnsupportedHinted) {
        m_dynamicUnsupportedHinted = false;
        emit statusChanged("检测到服务器支持动态分辨率，已铺满");
    }
    // 之前按「不支持」居中了，现在重新铺满（铺满即向服务器请求新分辨率）
    QTimer::singleShot(0, this, &RdpView::fitChild);
}

void RdpView::notifyChildWindowSeen() {
    if (!m_connected) return;
    emit statusChanged("已连接：" + m_profile.host);
    emit connected();
}

void RdpView::fitChild() {
    if (!m_childWid) return;
    if (m_profile.resizeMode == constants::kResizeFixed) return;  // 固定分辨率：外层滚动条
    const QSize view(qMax(width(), 64), qMax(height(), 64));

    if (m_profile.resizeMode == constants::kResizeDynamic) {
        // 1) 服务器已对一个分辨率作出决定（含上限场景）：以它为准居中，直到用户再改窗口
        if (m_serverLocked && m_sessionSize.isValid()) {
            placeChild(view, m_sessionSize);
            return;
        }
        // 2) 服务器不支持动态分辨率（caps 宣告缺失）且已确知会话分辨率：
        //    原分辨率居中 letterbox。窗口小于会话时同样如此（内容 1:1 完整，
        //    由外层滚动条处理），绝不压扁内容造成黑边/变形
        if (!m_dynamicActive && m_sessionSize.isValid() && m_attachedAt.hasExpired(1500)) {
            if (!m_dynamicUnsupportedHinted) {
                m_dynamicUnsupportedHinted = true;
                emit statusChanged("服务器不支持动态分辨率：以原分辨率居中显示，"
                                   "可点工具条「智能缩放铺满」切换为缩放铺满");
            }
            placeChild(view, m_sessionSize);
            return;
        }
        // 3) 已支持或仍在宽限期：铺满。ConfigureNotify 即向服务器请求该分辨率。
        //    若宽限期内视图小于已知会话分辨率，直接居中而不请求缩分辨率，
        //    避免服务器不支持时内容被压在左上角
        if (!m_dynamicActive && m_sessionSize.isValid()
            && (view.width() < m_sessionSize.width() - 8
                || view.height() < m_sessionSize.height() - 8)) {
            placeChild(view, m_sessionSize);
            return;
        }
        placeChild(view, view);
        return;
    }

    // smart：铺满，由 FreeRDP 缩放内容
    placeChild(view, view);
}

// 把子窗口按 content 尺寸放置到 view 上：content==view 铺满，content<view 居中，
// content>view 置于左上角（由外层滚动条处理）
void RdpView::placeChild(const QSize &view, const QSize &content) {
    if (!m_childWid || !m_x11) return;
    const QSize want(qMax(content.width(), 64), qMax(content.height(), 64));
    const int x = qMax((view.width() - want.width()) / 2, 0);
    const int y = qMax((view.height() - want.height()) / 2, 0);
    int cx = 0, cy = 0, cw = 0, ch = 0;
    if (m_x11->windowGeometry(m_childWid, &cx, &cy, &cw, &ch)
        && QSize(cw, ch) == want && cx == x && cy == y)
        return;  // 已就位，避免无意义的 ConfigureNotify 风暴
    m_appliedSize = want;
    m_lastChildSize = want;
    m_x11->moveResize(m_childWid, x, y, want.width(), want.height());
}

void RdpView::focusChild() {
    if (m_childWid) m_x11->setFocus(m_childWid);
}

void RdpView::onReadyRead() {
    m_buffer.append(m_proc->readAll());
    int idx;
    while ((idx = m_buffer.indexOf('\n')) >= 0) {
        QByteArray raw = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);
        handleLine(QString::fromUtf8(raw).trimmed());
    }
}

void RdpView::handleLine(const QString &line) {
    if (line.isEmpty()) return;
    m_log.append(line);
    if (m_log.size() > 4000) m_log.remove(0, 1000);
    emit logLine(line);

    // [hh:mm:ss:ms] [pid:tid] [LEVEL][channel] - [function]: message
    QRegularExpressionMatch m = QRegularExpression(
        "^\\[[^]]*\\]\\s*\\[[^]]*\\]\\s*\\[(ERROR|FATAL)\\]").match(line);
    if (m.hasMatch()) {
        QString reason = freerdpargs::classifyError(m_log.join('\n'));
        if (!reason.isEmpty()) {
            emit statusChanged(reason);
            m_errorMessage = reason;
        }
    }
}

void RdpView::onErrorOccurred(QProcess::ProcessError err) {
    QString msg;
    switch (err) {
    case QProcess::FailedToStart: msg = "xfreerdp 启动失败（请确认已安装 freerdp3-x11）"; break;
    case QProcess::Crashed: msg = "xfreerdp 进程崩溃"; break;
    case QProcess::Timedout: msg = "xfreerdp 超时"; break;
    case QProcess::WriteError: msg = "写入 xfreerdp stdin 失败"; break;
    case QProcess::ReadError: msg = "读取 xfreerdp 输出失败"; break;
    default: msg = "未知错误"; break;
    }
    emit statusChanged(msg);
    m_errorMessage = msg;
}

void RdpView::onFinished(int exitCode, QProcess::ExitStatus status) {
    onReadyRead();  // 冲刷残余输出
    m_pollTimer->stop();
    QString reason;
    if (m_aborting) {
        reason = "会话已断开";
    } else if (!m_errorMessage.isEmpty()) {
        reason = m_errorMessage;
    } else if (exitCode == 0) {
        reason = "会话已结束";
    } else {
        reason = freerdpargs::classifyError(m_log.join('\n'));
        if (reason.isEmpty()) reason = QString("连接已断开（退出码 %1）").arg(exitCode);
    }
    emit statusChanged(reason);
    emit sessionFinished(reason, exitCode);
}
