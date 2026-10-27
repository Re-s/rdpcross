#ifndef RDPCROSS_SESSIONWINDOW_H
#define RDPCROSS_SESSIONWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QAction>
#include <QTimer>
#include <QPointer>

#include "models.h"
#include "rdpview.h"

// RDP 会话窗口：宿主 RdpView（嵌入 xfreerdp 渲染窗口），
// 提供工具条、全屏切换（F11）、浮动工具条（F8）、诊断与断开重连。
class SessionWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit SessionWindow(const ConnectionProfile &profile, const QString &password,
                           bool startNow = true, QWidget *parent = nullptr);
    ~SessionWindow() override;

    void reconnect();

public slots:
    void toggleFullscreen();
    void maybeClose();

    // 供 CLI 模式观察状态
    RdpView *view() const { return m_view; }

signals:
    void sessionClosed(const ConnectionProfile &profile);

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onStatus(const QString &text);
    void onFinished(const QString &reason, int exitCode);
    void showLog();

private:
    void buildUi();
    // 把窗口尺寸调整为「会话视口恰好等于 size」；超出屏幕时收敛到可用区域
    void fitWindowToSession(const QSize &size, bool center = true);
    void enterFullscreen();
    void leaveFullscreen();
    void forceKillAndClose();
    void realClose();
    void emitClosed();
    void scheduleFloatbar();
    void showFloatbar();

    ConnectionProfile m_profile;
    QString m_password;

    RdpView *m_view = nullptr;
    class QScrollArea *m_scroll = nullptr;
    QLabel *m_statusLabel = nullptr;
    QAction *m_actFullscreen = nullptr, *m_actReconnect = nullptr;
    QAction *m_actOneToOne = nullptr;

    bool m_full = false;
    bool m_framelessSpan = false;
    QRect m_savedGeometry;
    class FloatingBar *m_floating = nullptr;
    QTimer m_floatbarTimer;

    QSize m_pendingFit;      // 连接建立前拿到的会话尺寸
    bool m_sizedOnce = false;
    bool m_wantReconnect = false;
    bool m_userClosing = false;
    bool m_closingDone = false;
    bool m_closedEmitted = false;
};

#endif  // RDPCROSS_SESSIONWINDOW_H
