#ifndef RDPCROSS_RDPVIEW_H
#define RDPCROSS_RDPVIEW_H

#include <QWidget>
#include <QSize>
#include <QProcess>
#include <QTimer>
#include <QString>
#include <QLabel>

#include "constants.h"
#include "freerdpargs.h"
#include "models.h"
#include "x11helper.h"

// 嵌入 xfreerdp3 渲染窗口的控件。
//
// 工作原理：
// 1. 本控件 winId() 作为 /parent-window: 目标；
// 2. xfreerdp3 在远程会话建立后创建自己的 X11 窗口并 reparent 进来；
// 3. 轮询发现子窗口后按当前尺寸 ConfigureWindow；
// 4. 鼠标点击与聚焦时把 X 输入焦点交给子窗口，使键盘直达远程会话。
class RdpView : public QWidget {
    Q_OBJECT
public:
    explicit RdpView(const ConnectionProfile &profile, const QString &password,
                     QWidget *parent = nullptr);
    ~RdpView() override;

    bool start();              // 必须在控件已显示（有 native winId）后调用
    void stop();               // SIGTERM，不阻塞
    void stopAndKill();        // 强杀
    bool isConnected() const { return m_connected; }
    QSize sessionSize() const { return m_lastChildSize; }
    bool isRunning() const { return m_proc && m_proc->state() != QProcess::NotRunning; }

    void setHint(const QString &text);

    // 供诊断窗口使用
    QString fullLog() const { return m_log.join('\n'); }
    QString maskedArgs() const { return freerdpargs::buildArgsMasked(m_profile, m_parentWid).join(' '); }
    QString binaryPath() const { return m_binPath; }

signals:
    void connected();
    void statusChanged(const QString &text);
    void sessionFinished(const QString &reason, int exitCode);
    void logLine(const QString &line);
    // 远程会话的真实分辨率（读自 xfreerdp 渲染窗口），用于让宿主窗口严丝合缝
    void sessionSizeChanged(const QSize &size);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    QSize sizeHint() const override;

private slots:
    void pollChild();
    void checkChildSize();
    void onReadyRead();
    void onErrorOccurred(QProcess::ProcessError err);
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    void attachChild(quint32 child);
    void fitChild();
    void focusChild();
    void handleLine(const QString &line);
    void notifyChildWindowSeen();

    ConnectionProfile m_profile;
    QString m_password;
    QString m_binPath;
    quint64 m_parentWid = 0;
    quint32 m_childWid = 0;

    QProcess *m_proc = nullptr;
    QTimer *m_pollTimer = nullptr;
    QTimer *m_sizeTimer = nullptr;
    X11Helper *m_x11 = nullptr;
    QLabel *m_hintLabel = nullptr;
    QSize m_lastChildSize;
    QSize m_appliedSize;  // 我们最近一次 ConfigureWindow 出去的尺寸

    QByteArray m_buffer;
    QStringList m_log;
    bool m_connected = false;
    bool m_aborting = false;
    QString m_errorMessage;
    int m_pollElapsed = 0;
};

#endif  // RDPCROSS_RDPVIEW_H
