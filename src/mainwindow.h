#ifndef RDPCROSS_MAINWINDOW_H
#define RDPCROSS_MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPointer>
#include <QHBoxLayout>
#include <QJsonObject>

#include "models.h"
#include "constants.h"

class SessionWindow;

// 主窗口：连接管理器（保存的连接列表 + 快速连接 + .rdp 导入导出）。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // 测试入口：用已保存 profile 直连（无凭据框），返回刚创建的会话窗口
    void launchForTest(const ConnectionProfile &profile) { launch(profile); }
    SessionWindow *currentSession() const {
        return m_sessions.isEmpty() ? nullptr : m_sessions.last().data();
    }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNewProfile();
    void onEditProfile();
    void onDeleteProfile();
    void onConnectSelected();
    void onItemDoubleClicked(QTreeWidgetItem *item, int col);
    void onContextMenu(const QPoint &pos);
    void onImportRdp();
    void onExportRdp();
    void onQuickConnect();
    void onSessionClosed(const ConnectionProfile &profile);

private:
    void refreshTree();
    void launch(ConnectionProfile profile, const QString &password = QString());
    void exportRdpFor(const ConnectionProfile &p);
    void pushQuickHistory(const QString &host);

    ConnectionProfile *currentProfile();
    static QIcon iconFor(const ConnectionProfile &p);

    QTreeWidget *m_tree = nullptr;
    QLineEdit *m_hostEdit = nullptr;
    QList<ConnectionProfile> m_profiles;
    QList<QPointer<SessionWindow>> m_sessions;
    QJsonObject m_settings;
};

// 快速连接条
class QuickConnectBar : public QWidget {
    Q_OBJECT
public:
    explicit QuickConnectBar(QWidget *parent = nullptr) : QWidget(parent) {
        auto *lay = new QHBoxLayout(this);
        lay->setContentsMargins(6, 4, 6, 4);
        lay->setSpacing(6);
        auto *label = new QLabel("计算机(C):", this);
        hostEdit = new QLineEdit(this);
        hostEdit->setPlaceholderText("计算机名、IP 地址或 主机:端口");
        connectBtn = new QPushButton("连接(N)", this);
        connectBtn->setDefault(true);
        lay->addWidget(label);
        lay->addWidget(hostEdit, 1);
        lay->addWidget(connectBtn);
        connect(connectBtn, &QPushButton::clicked, this, [this] { emit submitted(hostEdit->text().trimmed()); });
    }

    QLineEdit *hostEdit = nullptr;
    QPushButton *connectBtn = nullptr;

signals:
    void submitted(const QString &host);
};

#endif  // RDPCROSS_MAINWINDOW_H
