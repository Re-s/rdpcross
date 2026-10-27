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
    int w = 0, h = 0;
    if (!m_x11->windowSize(m_childWid, &w, &h)) return;
    if (w <= 0 || h <= 0) return;
    const QSize sz(w, h);
    if (sz == m_lastChildSize) return;
    m_lastChildSize = sz;
    if (sz == m_appliedSize) return;  // fitChild 自己设的，不算会话变化
    emit sessionSizeChanged(sz);
}

void RdpView::notifyChildWindowSeen() {
    if (!m_connected) return;
    emit statusChanged("已连接：" + m_profile.host);
    emit connected();
}

void RdpView::fitChild() {
    if (!m_childWid) return;
    if (m_profile.resizeMode == constants::kResizeFixed) return;  // 固定分辨率：外层滚动条
    const QSize target(qMax(width(), 64), qMax(height(), 64));
    int cw = 0, ch = 0;
    if (m_x11->windowSize(m_childWid, &cw, &ch) && QSize(cw, ch) == target)
        return;  // 已经是目标尺寸，避免无意义的 ConfigureNotify 风暴
    m_appliedSize = target;
    m_lastChildSize = target;
    m_x11->resize(m_childWid, target.width(), target.height());
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
