#include "mainwindow.h"

#include "constants.h"
#include "version_gen.h"

#include <QApplication>
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QDateTime>
#include <QScreen>
#include <QGuiApplication>

#include "config.h"
#include "credentialsdialog.h"
#include "aboutdialog.h"
#include "freerdpargs.h"
#include "rdpfile.h"
#include "sessionwindow.h"
#include "settingsdialog.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(constants::kAppName + " 远程桌面");
    resize(760, 520);

    m_settings = config::loadSettings();

    // --- 快速连接条 ---
    auto *quick = new QuickConnectBar(this);
    connect(quick, &QuickConnectBar::submitted, this, &MainWindow::onQuickConnect);
    m_hostEdit = quick->hostEdit;

    // --- 连接树 ---
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({"连接", "计算机", "最后使用"});
    m_tree->setRootIsDecorated(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onItemDoubleClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &MainWindow::onContextMenu);
    m_tree->setColumnWidth(0, 220);
    m_tree->setColumnWidth(1, 180);

    auto *central = new QWidget(this);
    auto *cl = new QVBoxLayout(central);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    cl->addWidget(quick);
    cl->addWidget(m_tree, 1);
    setCentralWidget(central);

    // --- 工具条 ---
    auto *tb = new QToolBar("管理", this);
    tb->setMovable(false);
    addToolBar(Qt::TopToolBarArea, tb);

    auto *actNew = new QAction("新建连接", this);
    actNew->setShortcut(QKeySequence::New);
    connect(actNew, &QAction::triggered, this, &MainWindow::onNewProfile);
    tb->addAction(actNew);

    auto *actEdit = new QAction("编辑…", this);
    actEdit->setShortcut(QKeySequence("F2"));
    connect(actEdit, &QAction::triggered, this, &MainWindow::onEditProfile);
    tb->addAction(actEdit);

    auto *actConnect = new QAction("连接", this);
    actConnect->setShortcut(QKeySequence("Return"));
    connect(actConnect, &QAction::triggered, this, &MainWindow::onConnectSelected);
    tb->addAction(actConnect);

    auto *actDelete = new QAction("删除…", this);
    connect(actDelete, &QAction::triggered, this, &MainWindow::onDeleteProfile);
    tb->addAction(actDelete);

    tb->addSeparator();
    auto *actImport = new QAction("导入 .rdp…", this);
    connect(actImport, &QAction::triggered, this, &MainWindow::onImportRdp);
    tb->addAction(actImport);
    auto *actExport = new QAction("导出 .rdp…", this);
    connect(actExport, &QAction::triggered, this, &MainWindow::onExportRdp);
    tb->addAction(actExport);

    // --- 菜单栏（帮助）---
    auto *mHelp = menuBar()->addMenu("帮助(&H)");
    auto *actAbout = mHelp->addAction("关于 RDPCross(&A)…", QKeySequence::HelpContents);
    connect(actAbout, &QAction::triggered, this, &MainWindow::onAbout);

    // --- 状态栏 ---
    // 正常情况下二进制路径对用户毫无意义（技术噪音）；只有找不到它才是
    // 用户需要知道的阻塞问题，此时才在状态栏常驻一条醒目提示。
    auto *sb = statusBar();
    if (freerdpargs::findBinary().isEmpty()) {
        auto *warn = new QLabel("⚠ 未找到 xfreerdp，无法连接——请安装 freerdp3-x11", this);
        warn->setStyleSheet("color: #b00020; padding: 2px 8px;");
        sb->addPermanentWidget(warn);
    }

    // 主窗口几何记忆
    auto geo = m_settings.value("window_geometry").toArray();
    if (geo.size() == 4) {
        resize(geo.at(2).toInt(), geo.at(3).toInt());
        move(geo.at(0).toInt(), geo.at(1).toInt());
    }
    if (m_settings.value("window_maximized").toBool()) showMaximized();

    refreshTree();

    // 快速连接历史
    auto hist = m_settings.value("quick_connect_history").toArray();
    if (!hist.isEmpty()) {
        m_hostEdit->setText(hist.first().toString());
        m_hostEdit->selectAll();
    }
    // 历史可见化：历史只填第一条而看不到其余等于没有历史。
    // 输入时下拉补全，↓ 键也能翻阅最近 8 条。
    if (!hist.isEmpty()) {
        QStringList hosts;
        for (const auto &v : hist) hosts << v.toString();
        auto *comp = new QCompleter(hosts, m_hostEdit);
        comp->setFilterMode(Qt::MatchContains);
        comp->setCaseSensitivity(Qt::CaseInsensitive);
        comp->setMaxVisibleItems(8);
        m_hostEdit->setCompleter(comp);
    }
}

MainWindow::~MainWindow() = default;

// ============================================================== 列表维护
// "最后使用"列：今天显示 hh:mm，更早显示 MM-dd，工具提示含完整时间。
// 一列塞 yyyy-MM-dd hh:mm 太宽且信息重心错——用户要的是"多久前用过"，不是绝对时刻。
QString MainWindow::formatLastUsed(qint64 secs) {
    if (secs <= 0) return "—";
    QDateTime dt = QDateTime::fromSecsSinceEpoch(secs);
    QDateTime now = QDateTime::currentDateTime();
    if (dt.date() == now.date()) return dt.toString("hh:mm");
    if (dt.daysTo(now) == 1) return "昨天";
    if (dt.daysTo(now) < 7) return dt.toString("周") + QString::number(dt.date().dayOfWeek());
    return dt.toString("MM-dd");
}

void MainWindow::refreshTree() {
    m_tree->clear();
    m_profiles = config::loadProfiles();

    // 分组：先按组名字典序（空组在最后），组内按最后使用时间倒序
    QMap<QString, QList<int>> groupIndex;
    for (int i = 0; i < m_profiles.size(); ++i)
        groupIndex[m_profiles[i].group.isEmpty() ? "" : m_profiles[i].group].append(i);

    auto sortedGroups = groupIndex.keys();
    // 空组放最后
    sortedGroups.removeAll("");
    sortedGroups.append("");

    for (const QString &g : sortedGroups) {
        QTreeWidgetItem *parent = nullptr;
        if (!g.isEmpty()) {
            parent = new QTreeWidgetItem(m_tree, {g, "", ""});
            QFont f = parent->font(0);
            f.setBold(true);
            parent->setFont(0, f);
            parent->setData(0, Qt::UserRole, QString());
        } else {
            parent = m_tree->invisibleRootItem();
        }
        auto items = groupIndex.value(g);
        std::sort(items.begin(), items.end(), [this](int a, int b) {
            return m_profiles[a].lastUsed > m_profiles[b].lastUsed;
        });
        for (int idx : items) {
            const auto &p = m_profiles[idx];
        QString last = formatLastUsed(p.lastUsed);
        auto *item = new QTreeWidgetItem(parent, {p.name, p.host, last});
        item->setData(0, Qt::UserRole, p.id);
        item->setIcon(0, iconFor(p));
        // 相对时间便于扫读；悬停可看绝对时刻（完成注释承诺）。
        if (p.lastUsed > 0)
            item->setToolTip(2, QDateTime::fromSecsSinceEpoch(p.lastUsed)
                                    .toString("yyyy-MM-dd hh:mm"));
        }
        if (parent != m_tree->invisibleRootItem()) parent->setExpanded(true);
    }
    // 注意：不再调用 sortByColumn()——它按"连接"名升序会覆盖上面组内按
    // 最后使用时间的倒序（"最近用过的在最上"是 mstsc 式的核心预期）。
    // 组顺序已由 sortedGroups 的字典序（空组置尾）保证。

    // 空状态引导：首次使用时树是空的，用户不知道从哪开始。
    // 放一条不可选的提示行，指明最短路径（快速连接条就在正上方）。
    if (m_profiles.isEmpty()) {
        auto *hint = new QTreeWidgetItem(m_tree,
            {"暂无保存的连接——在上方「计算机」输入地址直接连接，或 Ctrl+N 新建", "", ""});
        hint->setFlags(Qt::NoItemFlags);
        QFont f = hint->font(0);
        f.setPointSize(f.pointSize() - 1);
        hint->setFont(0, f);
        QBrush b = hint->foreground(0);
        b.setColor(QColor(120, 120, 125));
        hint->setForeground(0, b);
    }
}

QIcon MainWindow::iconFor(const ConnectionProfile &p) {
    QPixmap pix(16, 16);
    pix.fill(p.fullscreen ? Qt::darkGreen : Qt::darkBlue);
    return QIcon(pix);
}

ConnectionProfile *MainWindow::currentProfile() {
    auto *item = m_tree->currentItem();
    if (!item) return nullptr;
    QString id = item->data(0, Qt::UserRole).toString();
    if (id.isEmpty()) return nullptr;
    for (auto &p : m_profiles)
        if (p.id == id) return &p;
    return nullptr;
}

// ================================================================== 命令
void MainWindow::onNewProfile() {
    ConnectionProfile p;
    p.host = m_hostEdit->text().trimmed();
    SettingsDialog dlg(p, false, this);
    if (dlg.exec() == QDialog::Accepted) {
        config::upsertProfile(p);
        refreshTree();
    }
}

void MainWindow::onEditProfile() {
    auto *p = currentProfile();
    if (!p) return;
    ConnectionProfile copy = *p;
    SettingsDialog dlg(copy, false, this);
    if (dlg.exec() == QDialog::Accepted) {
        config::upsertProfile(copy);
        refreshTree();
    }
}

void MainWindow::onDeleteProfile() {
    auto *p = currentProfile();
    if (!p) return;
    if (QMessageBox::question(this, "删除连接", QString("确定删除「%1」吗？").arg(p->name),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    config::deleteProfile(p->id);
    refreshTree();
}

void MainWindow::onConnectSelected() {
    auto *p = currentProfile();
    if (p) launch(*p);
}

void MainWindow::onItemDoubleClicked(QTreeWidgetItem *item, int col) {
    (void)col; (void)item;
    auto *p = currentProfile();
    if (p) launch(*p);
}

void MainWindow::onContextMenu(const QPoint &pos) {
    auto *item = m_tree->itemAt(pos);
    QMenu menu(this);
    auto *actOpen = menu.addAction("连接");
    auto *actEdit = menu.addAction("编辑…");
    auto *actDup = menu.addAction("复制…");
    auto *actExp = menu.addAction("导出 .rdp…");
    menu.addSeparator();
    auto *actDel = menu.addAction("删除…");
    if (!item) {
        for (auto *a : {actOpen, actEdit, actDup, actExp, actDel}) a->setEnabled(false);
    }
    auto *p = currentProfile();
    auto *action = menu.exec(m_tree->mapToGlobal(pos));
    if (!action || !p) return;
    if (action == actOpen) launch(*p);
    else if (action == actEdit) onEditProfile();
    else if (action == actDup) {
        ConnectionProfile np = *p;
        np.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        np.name = p->name + "（副本）";
        config::upsertProfile(np);
        refreshTree();
    } else if (action == actExp) {
        exportRdpFor(*p);
    } else if (action == actDel) {
        onDeleteProfile();
    }
}

// ================================================================ 导入导出
void MainWindow::onImportRdp() {
    QString path = QFileDialog::getOpenFileName(this, "导入 .rdp 文件",
                                                QDir::homePath(), "RDP 文件 (*.rdp);;所有文件 (*)");
    if (path.isEmpty()) return;
    ConnectionProfile p;
    if (!rdpfile::importProfile(path, p)) {
        QMessageBox::critical(this, "导入失败", "无法解析 .rdp 文件。");
        return;
    }
    if (p.name.isEmpty()) p.name = QFileInfo(path).completeBaseName();
    config::upsertProfile(p);
    refreshTree();
    QMessageBox::information(this, "已导入", QString("已保存连接「%1」。").arg(p.name));
}

void MainWindow::onExportRdp() {
    auto *p = currentProfile();
    if (!p) {
        QMessageBox::information(this, "导出 .rdp", "请先选择一个连接。");
        return;
    }
    exportRdpFor(*p);
}

void MainWindow::exportRdpFor(const ConnectionProfile &p) {
    QString def = QDir::homePath() + "/" + p.name + ".rdp";
    QString path = QFileDialog::getSaveFileName(this, "导出 .rdp 文件", def, "RDP 文件 (*.rdp)");
    if (path.isEmpty()) return;
    if (!rdpfile::exportProfile(p, path)) {
        QMessageBox::critical(this, "导出失败", "写入文件失败。");
        return;
    }
    QMessageBox::information(this, "已导出",
                             QString("已写入 %1\n可在 Windows mstsc 中直接打开。").arg(path));
}

// ================================================================ 连接动作
void MainWindow::onQuickConnect() {
    QString host = m_hostEdit->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::information(this, "快速连接", "请输入目标计算机地址。");
        return;
    }
    ConnectionProfile p;
    p.name = host;
    p.host = host;
    QString user, domain, password;
    bool savePw = false;
    if (!CredentialsDialog::ask(p, this, user, domain, password, savePw)) return;
    p.username = user;
    p.domain = domain;
    if (savePw) {
        p.password = password;
        p.savePassword = true;
        config::upsertProfile(p);
    }
    launch(p, password);
    pushQuickHistory(host);
}

void MainWindow::pushQuickHistory(const QString &host) {
    auto hist = m_settings.value("quick_connect_history").toArray();
    QJsonArray out;
    out.append(host);
    for (const auto &v : hist) {
        if (v.toString() != host) out.append(v);
        if (out.size() >= 8) break;
    }
    m_settings["quick_connect_history"] = out;
    config::saveSettings(m_settings);
}

void MainWindow::launch(ConnectionProfile profile, const QString &password) {
    QString bin = freerdpargs::findBinary();
    if (bin.isEmpty()) {
        QMessageBox::critical(this, "缺少依赖",
            "未找到 xfreerdp 可执行文件。\n请安装 FreeRDP 客户端，例如：\n"
            "  Debian/Ubuntu: apt install freerdp3-x11\n"
            "  Fedora:        dnf install freerdp\n"
            "  Arch:          pacman -S freerdp");
        return;
    }

    QString pw = password;
    // 已保存密码 → 直接使用（等价 mstsc "记住密码"）
    if (pw.isEmpty() && profile.savePassword && !profile.password.isEmpty())
        pw = profile.password;
    // 未保存密码 → 弹凭据框（等价 mstsc 提示凭据）
    else if (pw.isEmpty()) {
        QString user, domain, savePwPass;
        bool savePw = false;
        if (!CredentialsDialog::ask(profile, this, user, domain, savePwPass, savePw)) return;
        profile.username = user;
        profile.domain = domain;
        pw = savePwPass;
        if (savePw) {
            profile.password = savePwPass;
            profile.savePassword = true;
            config::upsertProfile(profile);
        }
    }

    auto *session = new SessionWindow(profile, pw, true, nullptr);
    connect(session, &SessionWindow::sessionClosed, this, &MainWindow::onSessionClosed);
    session->show();
    m_sessions.append(QPointer<SessionWindow>(session));
}

void MainWindow::onSessionClosed(const ConnectionProfile &profile) {
    (void)profile;
    refreshTree();
}

void MainWindow::onAbout() {
    AboutDialog dlg(this);
    dlg.exec();
}

// ================================================================== 关闭
void MainWindow::closeEvent(QCloseEvent *event) {
    QJsonArray geo;
    geo.append(x()); geo.append(y()); geo.append(width()); geo.append(height());
    m_settings["window_geometry"] = geo;
    m_settings["window_maximized"] = isMaximized();
    config::saveSettings(m_settings);
    QMainWindow::closeEvent(event);
}
