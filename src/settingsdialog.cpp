#include "settingsdialog.h"

#include "constants.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QListWidgetItem>
#include <QScreen>
#include <QGuiApplication>

SettingsDialog::SettingsDialog(ConnectionProfile &profile, bool connectAfter, QWidget *parent)
    : QDialog(parent), m_profile(profile) {
    setWindowTitle("远程桌面连接 - 设置");
    setMinimumWidth(560);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(tabGeneral(), "常规");
    tabs->addTab(tabDisplay(), "显示");
    tabs->addTab(tabLocalResources(), "本地资源");
    tabs->addTab(tabPrograms(), "程序");
    tabs->addTab(tabExperience(), "体验");
    tabs->addTab(tabAdvanced(), "高级");

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btns->button(QDialogButtonBox::Ok)->setText(connectAfter ? "连接" : "保存");
    connect(btns, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(tabs);
    lay->addWidget(btns);

    load(profile);
}

// ==================================================================== 常规
QWidget *SettingsDialog::tabGeneral() {
    auto *w = new QWidget(this);
    auto *form = new QFormLayout(w);
    m_name = new QLineEdit(this);
    m_group = new QLineEdit(this);
    m_host = new QLineEdit(this);
    m_host->setPlaceholderText("例如 192.168.1.10、pc.example.com、host:3389 或 [::1]");
    m_user = new QLineEdit(this);
    m_user->setPlaceholderText("用户名或 DOMAIN\\user 或 user@domain");
    m_domain = new QLineEdit(this);
    m_pw = new QLineEdit(this);
    m_pw->setEchoMode(QLineEdit::Password);
    m_savePw = new QCheckBox("记住密码（保存到本地配置文件）", this);
    m_notes = new QLineEdit(this);
    m_notes->setPlaceholderText("可选：这台机器的备注说明");

    form->addRow("名称:", m_name);
    form->addRow("分组:", m_group);
    form->addRow("计算机:*", m_host);
    form->addRow("用户名:", m_user);
    form->addRow("域:", m_domain);
    form->addRow("密码:", m_pw);
    form->addRow(m_savePw);
    form->addRow("备注:", m_notes);

    auto *hint = new QLabel(
        "<small>提示：用户名支持 domain\\user 或 user@domain 形式；"
        "留空时连接会提示输入凭据（与 mstsc 相同）。</small>", this);
    hint->setWordWrap(true);
    form->addRow(hint);
    return w;
}

// ==================================================================== 显示
QWidget *SettingsDialog::tabDisplay() {
    auto *w = new QWidget(this);
    auto *form = new QFormLayout(w);

    m_fullscreen = new QCheckBox("全屏", this);
    m_resolution = new QComboBox(this);
    m_resolution->setEditable(true);
    for (const auto &r : constants::kResolutionPresets) m_resolution->addItem(r.name);
    m_resolution->addItem("自定义…");
    connect(m_resolution, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onResolutionChanged);

    m_w = new QSpinBox(this);
    m_h = new QSpinBox(this);
    m_w->setRange(200, 8192);
    m_h->setRange(200, 8192);
    auto *sizeRow = new QHBoxLayout;
    sizeRow->addWidget(m_w);
    sizeRow->addWidget(new QLabel("×", this));
    sizeRow->addWidget(m_h);
    sizeRow->addStretch();
    m_sizeWidget = new QWidget(this);
    m_sizeWidget->setLayout(sizeRow);

    m_color = new QComboBox(this);
    for (int d : constants::kColorDepths) m_color->addItem(QString::number(d) + " 位", d);

    m_resize = new QComboBox(this);
    m_resize->addItem("智能缩放（远程画面随窗口等比缩放，默认）", constants::kResizeSmart);
    m_resize->addItem("动态分辨率（窗口变化时通知服务器改变分辨率）", constants::kResizeDynamic);
    m_resize->addItem("固定分辨率", constants::kResizeFixed);

    m_multimon = new QCheckBox("将桌面跨越到所有可用的监视器（仅全屏）", this);

    form->addRow(m_fullscreen);
    form->addRow("分辨率:", m_resolution);
    form->addRow("大小:", m_sizeWidget);
    form->addRow("颜色深度:", m_color);
    form->addRow("缩放与调整大小:", m_resize);
    form->addRow(m_multimon);
    return w;
}

void SettingsDialog::onResolutionChanged(int idx) {
    QString name = m_resolution->itemText(idx);
    m_sizeWidget->setVisible(name == "自定义…");
}

// ================================================================ 本地资源
QWidget *SettingsDialog::tabLocalResources() {
    auto *w = new QWidget(this);
    auto *form = new QFormLayout(w);

    m_keyboard = new QComboBox(this);
    m_keyboard->addItem("仅使用全屏时（默认）", constants::kKeyboardFullscreen);
    m_keyboard->addItem("在远程计算机上", constants::kKeyboardRemote);
    m_keyboard->addItem("在此计算机上", constants::kKeyboardLocal);

    m_audio = new QComboBox(this);
    m_audio->addItem("在此计算机上播放", constants::kAudioClient);
    m_audio->addItem("在远程计算机上播放", constants::kAudioServer);
    m_audio->addItem("不录制", constants::kAudioNone);

    m_mic = new QCheckBox("从此计算机录制音频（麦克风）", this);
    m_clipboard = new QCheckBox("剪贴板", this);
    m_printers = new QCheckBox("打印机", this);
    m_smartcards = new QCheckBox("智能卡", this);
    m_plugDrives = new QCheckBox("重定向所有即插即用磁盘", this);
    m_serial = new QLineEdit(this);
    m_serial->setPlaceholderText("例如 /dev/ttyUSB0,/dev/ttyS0（逗号分隔）");

    form->addRow("应用 Windows 组合键:", m_keyboard);
    form->addRow("远程音频播放:", m_audio);
    form->addRow(m_mic);
    form->addRow(new QLabel("<b>本地设备和资源</b>", this));
    form->addRow(m_clipboard);
    form->addRow(m_printers);
    form->addRow(m_smartcards);
    form->addRow(m_plugDrives);
    form->addRow("串行端口:", m_serial);

    auto *gb = new QGroupBox("驱动器/文件夹重定向", this);
    auto *gl = new QVBoxLayout(gb);
    m_drives = new QListWidget(this);
    m_drives->setMaximumHeight(110);
    gl->addWidget(m_drives);
    auto *row = new QHBoxLayout;
    auto *btnAdd = new QPushButton("添加…", this);
    auto *btnRemove = new QPushButton("移除", this);
    connect(btnAdd, &QPushButton::clicked, this, &SettingsDialog::addDrive);
    connect(btnRemove, &QPushButton::clicked, this, &SettingsDialog::removeDrive);
    row->addWidget(btnAdd);
    row->addWidget(btnRemove);
    row->addStretch();
    gl->addLayout(row);
    form->addRow(gb);
    return w;
}

void SettingsDialog::addDrive() {
    QString path = QFileDialog::getExistingDirectory(this, "选择要共享到远程的文件夹");
    if (path.isEmpty()) return;
    QString name = path.section('/', -1);
    if (name.isEmpty()) name = path;
    m_drives->addItem(name + " → " + path);
}

void SettingsDialog::removeDrive() {
    for (auto *item : m_drives->selectedItems()) delete item;
}

// ==================================================================== 程序
QWidget *SettingsDialog::tabPrograms() {
    auto *w = new QWidget(this);
    auto *form = new QFormLayout(w);
    m_shell = new QLineEdit(this);
    m_shell->setPlaceholderText("可选：启动程序（如 C:\\Tools\\app.exe）");
    m_shellDir = new QLineEdit(this);
    m_shellDir->setPlaceholderText("可选：Shell 工作目录");
    form->addRow("备用 Shell（启动程序）:", m_shell);
    form->addRow("工作目录:", m_shellDir);
    auto *tip = new QLabel(
        "<small>留空表示登录到完整桌面；填写后远程会话将只启动指定程序"
        "（等价 mstsc「程序」选项卡的「启动程序」）。</small>", this);
    tip->setWordWrap(true);
    form->addRow(tip);
    return w;
}

// ==================================================================== 体验
QWidget *SettingsDialog::tabExperience() {
    auto *w = new QWidget(this);
    auto *form = new QFormLayout(w);
    m_network = new QComboBox(this);
    for (const QString &n : constants::kNetworkPresets) m_network->addItem(n, n);

    m_gfx = new QComboBox(this);
    m_gfx->addItem("自动（由服务器决定）", "auto");
    m_gfx->addItem("H.264 / AVC444（推荐，最省带宽，需 Windows 8.1+）", "avc444");
    m_gfx->addItem("RemoteFX 渐进式（兼容性最好）", "progressive");
    m_gfx->addItem("关闭图形管线（老服务器排障用）", "off");

    m_cache = new QCheckBox("持久位图缓存", this);
    m_composition = new QCheckBox("桌面组合", this);
    m_drag = new QCheckBox("拖动时显示窗口内容", this);
    m_anims = new QCheckBox("菜单和窗口动画", this);
    m_styles = new QCheckBox("视觉样式", this);
    m_fonts = new QCheckBox("字体平滑(ClearType)", this);
    m_wallpaper = new QCheckBox("桌面背景", this);
    m_reconnect = new QCheckBox("连接断开时自动重新连接", this);

    form->addRow("连接速度:", m_network);
    form->addRow("图形编码(GFX):", m_gfx);
    auto *box = new QGroupBox("性能", this);
    auto *bl = new QVBoxLayout(box);
    for (auto *c : {m_cache, m_composition, m_drag, m_anims,
                    m_styles, m_fonts, m_wallpaper, m_reconnect}) bl->addWidget(c);
    form->addRow(box);
    return w;
}

// ==================================================================== 高级
QWidget *SettingsDialog::tabAdvanced() {
    auto *w = new QWidget(this);
    auto *form = new QFormLayout(w);
    m_security = new QComboBox(this);
    m_security->addItem("自动（默认，协商选择 NLA/TLS）", constants::kSecNegotiate);
    m_security->addItem("网络级身份验证（NLA）", constants::kSecNla);
    m_security->addItem("TLS", constants::kSecTls);
    m_security->addItem("RDP 标准安全", constants::kSecRdp);
    m_security->addItem("Microsoft Entra ID", constants::kSecAad);

    m_cert = new QComboBox(this);
    m_cert->addItem("首次使用即信任（推荐，等价 mstsc「不再询问」）", constants::kCertTofu);
    m_cert->addItem("每次询问", constants::kCertAsk);
    m_cert->addItem("忽略证书校验（不安全）", constants::kCertIgnore);
    m_cert->addItem("严格校验，不匹配即拒绝", constants::kCertDeny);

    form->addRow("身份验证方式:", m_security);
    form->addRow("服务器证书校验失败时:", m_cert);

    auto *gb = new QGroupBox("RD 网关", this);
    auto *gl = new QFormLayout(gb);
    m_gwEnable = new QCheckBox("使用 RD 网关服务器", this);
    m_gwHost = new QLineEdit(this);
    m_gwHost->setPlaceholderText("网关地址（可含端口）");
    m_gwUser = new QLineEdit(this);
    m_gwDomain = new QLineEdit(this);
    m_gwPw = new QLineEdit(this);
    m_gwPw->setEchoMode(QLineEdit::Password);
    m_gwUsage = new QComboBox(this);
    m_gwUsage->addItem("始终使用", "always");
    m_gwUsage->addItem("直连失败时使用", "detect");
    gl->addRow(m_gwEnable);
    gl->addRow("服务器:", m_gwHost);
    gl->addRow("用户名:", m_gwUser);
    gl->addRow("域:", m_gwDomain);
    gl->addRow("密码:", m_gwPw);
    gl->addRow("使用方式:", m_gwUsage);
    form->addRow(gb);
    return w;
}

// ================================================================ 数据读写
void SettingsDialog::load(const ConnectionProfile &p) {
    m_name->setText(p.name);
    m_group->setText(p.group);
    m_host->setText(p.host);
    m_user->setText(p.username);
    m_domain->setText(p.domain);
    if (p.savePassword) m_pw->setText(p.password);
    m_savePw->setChecked(p.savePassword);
    m_notes->setText(p.notes);

    m_fullscreen->setChecked(p.fullscreen);
    int match = -1;
    for (int i = 0; i < constants::kResolutionPresets.size(); ++i) {
        const auto &r = constants::kResolutionPresets[i];
        if (r.w == p.width && r.h == p.height) { match = i; break; }
    }
    m_resolution->setCurrentIndex(match < 0 ? m_resolution->count() - 1 : match);
    m_w->setValue(p.width);
    m_h->setValue(p.height);
    onResolutionChanged(m_resolution->currentIndex());
    for (int i = 0; i < m_color->count(); ++i)
        if (m_color->itemData(i).toInt() == p.colorDepth) m_color->setCurrentIndex(i);
    for (int i = 0; i < m_resize->count(); ++i)
        if (m_resize->itemData(i).toString() == p.resizeMode) m_resize->setCurrentIndex(i);
    m_multimon->setChecked(p.multiMonitor);

    for (int i = 0; i < m_keyboard->count(); ++i)
        if (m_keyboard->itemData(i).toString() == p.keyboardMode) m_keyboard->setCurrentIndex(i);
    for (int i = 0; i < m_audio->count(); ++i)
        if (m_audio->itemData(i).toString() == p.audioMode) m_audio->setCurrentIndex(i);
    m_mic->setChecked(p.audioCapture);
    m_clipboard->setChecked(p.clipboard);
    m_printers->setChecked(p.printers);
    m_smartcards->setChecked(p.smartcards);
    m_plugDrives->setChecked(p.plugAllDrives);
    m_serial->setText(p.serialPorts.join(","));
    m_drives->clear();
    for (const auto &d : p.drives)
        m_drives->addItem(d.name + " → " + d.path);

    m_shell->setText(p.alternateShell);
    m_shellDir->setText(p.shellWorkingDir);

    for (int i = 0; i < m_network->count(); ++i)
        if (m_network->itemData(i).toString() == p.network) m_network->setCurrentIndex(i);
    for (int i = 0; i < m_gfx->count(); ++i)
        if (m_gfx->itemData(i).toString() == p.gfx) m_gfx->setCurrentIndex(i);
    m_cache->setChecked(p.persistentBitmapCache);
    m_composition->setChecked(p.desktopComposition);
    m_drag->setChecked(p.fullWindowDrag);
    m_anims->setChecked(p.menuAnimations);
    m_styles->setChecked(p.visualStyles);
    m_fonts->setChecked(p.fontSmoothing);
    m_wallpaper->setChecked(p.wallpaper);
    m_reconnect->setChecked(p.reconnectAutomatically);

    for (int i = 0; i < m_security->count(); ++i)
        if (m_security->itemData(i).toString() == p.security) m_security->setCurrentIndex(i);
    for (int i = 0; i < m_cert->count(); ++i)
        if (m_cert->itemData(i).toString() == p.certMode) m_cert->setCurrentIndex(i);
    m_gwEnable->setChecked(p.gateway.enabled);
    m_gwHost->setText(p.gateway.host);
    m_gwUser->setText(p.gateway.username);
    m_gwDomain->setText(p.gateway.domain);
    m_gwPw->setText(p.gateway.password);
    m_gwUsage->setCurrentIndex(p.gateway.usage == "always" ? 0 : 1);
}

void SettingsDialog::onAccept() {
    if (m_host->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "缺少信息", "请填写目标计算机地址。");
        return;
    }
    writeBack();
    accept();
}

void SettingsDialog::writeBack() {
    using namespace constants;
    auto &p = m_profile;
    p.name = m_name->text().trimmed().isEmpty() ? m_host->text().trimmed() : m_name->text().trimmed();
    p.group = m_group->text().trimmed();
    p.host = m_host->text().trimmed();
    p.username = m_user->text().trimmed();
    p.domain = m_domain->text().trimmed();
    p.savePassword = m_savePw->isChecked();
    if (p.savePassword && !m_pw->text().isEmpty()) p.password = m_pw->text();
    else if (!p.savePassword) p.password = "";
    p.notes = m_notes->text().trimmed();

    p.fullscreen = m_fullscreen->isChecked();
    if (p.fullscreen) {
        QScreen *scr = screen();
        if (scr) {
            p.width = scr->availableGeometry().width();
            p.height = scr->availableGeometry().height();
        }
    } else {
        QString text = m_resolution->currentText();
        bool matched = false;
        for (const auto &r : kResolutionPresets) {
            if (r.name == text) { p.width = r.w; p.height = r.h; matched = true; break; }
        }
        if (!matched) { p.width = m_w->value(); p.height = m_h->value(); }
    }
    p.colorDepth = m_color->currentData().toInt();
    p.resizeMode = m_resize->currentData().toString();
    p.multiMonitor = m_multimon->isChecked();

    p.keyboardMode = m_keyboard->currentData().toString();
    p.audioMode = m_audio->currentData().toString();
    p.audioCapture = m_mic->isChecked();
    p.clipboard = m_clipboard->isChecked();
    p.printers = m_printers->isChecked();
    p.smartcards = m_smartcards->isChecked();
    p.plugAllDrives = m_plugDrives->isChecked();
    p.serialPorts.clear();
    for (const QString &x : m_serial->text().split(',')) {
        QString s = x.trimmed();
        if (!s.isEmpty()) p.serialPorts.append(s);
    }
    p.drives.clear();
    for (int i = 0; i < m_drives->count(); ++i) {
        QString text = m_drives->item(i)->text();
        int idx = text.indexOf(" → ");
        if (idx > 0) {
            DriveRedirect d;
            d.name = text.left(idx).trimmed();
            d.path = text.mid(idx + 3).trimmed();
            if (!d.path.isEmpty()) p.drives.append(d);
        }
    }

    p.alternateShell = m_shell->text().trimmed();
    p.shellWorkingDir = m_shellDir->text().trimmed();

    p.network = m_network->currentData().toString();
    p.gfx = m_gfx->currentData().toString();
    p.persistentBitmapCache = m_cache->isChecked();
    p.desktopComposition = m_composition->isChecked();
    p.fullWindowDrag = m_drag->isChecked();
    p.menuAnimations = m_anims->isChecked();
    p.visualStyles = m_styles->isChecked();
    p.fontSmoothing = m_fonts->isChecked();
    p.wallpaper = m_wallpaper->isChecked();
    p.reconnectAutomatically = m_reconnect->isChecked();

    p.security = m_security->currentData().toString();
    p.certMode = m_cert->currentData().toString();
    auto &gw = p.gateway;
    gw.enabled = m_gwEnable->isChecked();
    gw.host = m_gwHost->text().trimmed();
    gw.username = m_gwUser->text().trimmed();
    gw.domain = m_gwDomain->text().trimmed();
    if (!m_gwPw->text().isEmpty()) gw.password = m_gwPw->text();
    gw.usage = m_gwUsage->currentData().toString();
}
