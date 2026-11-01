#include "aboutdialog.h"

#include "constants.h"
#include "version_gen.h"
#include "freerdpargs.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QProcess>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("关于 " + constants::kAppName);
    setMinimumWidth(440);

    auto *lay = new QVBoxLayout(this);
    lay->setSpacing(8);
    lay->setContentsMargins(24, 20, 24, 16);

    auto *title = new QLabel(constants::kAppName, this);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    lay->addWidget(title);

    // 版本号由构建时自动生成（cmake/VersionInfo.cmake -> version_gen.h）
    auto *ver = new QLabel(
        QString("<b>%1</b><br>"
                "<span style=\"color: rgb(110,110,120)\">由构建自动生成：基础版本 %2 · 第 %3 次提交 · 哈希 %4</span>")
            .arg(version::kAppVersionFull, version::kAppVersionBase,
                 version::kAppCommits, version::kAppBuildHash),
        this);
    ver->setWordWrap(true);
    lay->addWidget(ver);

    lay->addSpacing(4);

    auto *desc = new QLabel(
        "基于 Qt6 + FreeRDP3 的跨平台 RDP 远程桌面客户端。\n"
        "支持连接管理、.rdp 导入导出、嵌入多会话、智能缩放与动态分辨率。\n\n"
        "项目主页：<a href=\"https://github.com/Re-s/rdpcross\">https://github.com/Re-s/rdpcross</a>",
        this);
    desc->setWordWrap(true);
    desc->setTextInteractionFlags(Qt::TextBrowserInteraction);
    desc->setOpenExternalLinks(true);
    lay->addWidget(desc);

    lay->addSpacing(4);

    // 运行时技术信息：Qt 版本 + xfreerdp 版本（可选，缺失时给出提示）
    const QString rdpBin = freerdpargs::findBinary();
    QString tech = QString("Qt 运行时版本：%1").arg(QString::fromUtf8(qVersion()));
    if (rdpBin.isEmpty()) {
        tech += "\nxfreerdp：未找到（请安装 freerdp3-x11）";
    } else {
        QProcess proc;
        proc.start(rdpBin, {QStringLiteral("--version")});
        if (proc.waitForFinished(3000)) {
            const QString out = QString::fromUtf8(proc.readAllStandardOutput() +
                                                  proc.readAllStandardError());
            // 输出形如 "This is FreeRDP version 3.15.0 (n/a)"
            const QRegularExpressionMatch m =
                QRegularExpression("version\\s+([0-9][0-9.]*)").match(out);
            tech += QString("\nxfreerdp：FreeRDP %1").arg(
                m.hasMatch() ? m.captured(1) : QString("未知版本"));
        } else {
            tech += "\nxfreerdp：版本查询超时";
        }
    }
    auto *techLabel = new QLabel(tech, this);
    techLabel->setWordWrap(true);
    techLabel->setStyleSheet("color: rgb(110,110,120);");
    lay->addWidget(techLabel);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *copyBtn = btns->addButton("复制版本号", QDialogButtonBox::ActionRole);
    connect(copyBtn, &QPushButton::clicked, this, [] {
        QApplication::clipboard()->setText(version::kAppVersionFull);
    });
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(btns);
}
