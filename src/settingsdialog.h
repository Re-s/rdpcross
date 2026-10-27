#ifndef RDPCROSS_SETTINGSDIALOG_H
#define RDPCROSS_SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QListWidget>
#include <QPushButton>

#include "constants.h"
#include "models.h"

// 连接设置对话框：选项卡与 mstsc（Windows 远程桌面连接）一一对齐。
// 常规 / 显示 / 本地资源 / 程序 / 体验 / 高级。
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(ConnectionProfile &profile, bool connectAfter = false,
                            QWidget *parent = nullptr);

    // 确定（_onAccept）时把界面值写回 profile

private slots:
    void onAccept();
    void onResolutionChanged(int idx);
    void addDrive();
    void removeDrive();

private:
    void load(const ConnectionProfile &p);
    void writeBack();

    QWidget *tabGeneral();
    QWidget *tabDisplay();
    QWidget *tabLocalResources();
    QWidget *tabPrograms();
    QWidget *tabExperience();
    QWidget *tabAdvanced();

    ConnectionProfile &m_profile;

    // 常规
    QLineEdit *m_name = nullptr, *m_group = nullptr, *m_host = nullptr,
              *m_user = nullptr, *m_domain = nullptr, *m_pw = nullptr, *m_notes = nullptr;
    QCheckBox *m_savePw = nullptr;
    // 显示
    QCheckBox *m_fullscreen = nullptr, *m_multimon = nullptr;
    QComboBox *m_resolution = nullptr, *m_color = nullptr, *m_resize = nullptr;
    QSpinBox *m_w = nullptr, *m_h = nullptr;
    QWidget *m_sizeWidget = nullptr;
    // 本地资源
    QComboBox *m_keyboard = nullptr, *m_audio = nullptr;
    QCheckBox *m_mic = nullptr, *m_clipboard = nullptr, *m_printers = nullptr,
              *m_smartcards = nullptr, *m_plugDrives = nullptr;
    QLineEdit *m_serial = nullptr;
    QListWidget *m_drives = nullptr;
    // 程序
    QLineEdit *m_shell = nullptr, *m_shellDir = nullptr;
    // 体验
    QComboBox *m_network = nullptr;
    QComboBox *m_gfx = nullptr;
    QCheckBox *m_cache = nullptr, *m_composition = nullptr, *m_drag = nullptr,
              *m_anims = nullptr, *m_styles = nullptr, *m_fonts = nullptr,
              *m_wallpaper = nullptr, *m_reconnect = nullptr;
    // 高级
    QComboBox *m_security = nullptr, *m_cert = nullptr;
    QCheckBox *m_gwEnable = nullptr;
    QLineEdit *m_gwHost = nullptr, *m_gwUser = nullptr, *m_gwDomain = nullptr,
              *m_gwPw = nullptr;
    QComboBox *m_gwUsage = nullptr;
};

#endif  // RDPCROSS_SETTINGSDIALOG_H
