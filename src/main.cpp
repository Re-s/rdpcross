#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <iostream>

#include "mainwindow.h"
#include "sessionwindow.h"
#include "config.h"
#include "constants.h"

// [rdpcross] 前缀的状态行：便于终端/自动化观察连接状态
void logStatus(const QString &msg) {
    std::cout << "[rdpcross] " << msg.toUtf8().constData() << std::endl;
    std::cout.flush();
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("RDPCross");
    app.setApplicationDisplayName("RDPCross 远程桌面");
    app.setApplicationVersion(constants::kAppVersion);
    app.setOrganizationName("RDPCross");

    QCommandLineParser parser;
    parser.setApplicationDescription("RDPCross 远程桌面客户端（基于 FreeRDP）");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption connectOpt({"c", "connect"},
        "直接连接 <host[:port]>（打开凭据对话框），跳过主窗口", "host");
    QCommandLineOption quickOpt({"q", "quick"},
        "直接连接（跳过凭据对话框），位置参数：<host[:port]> <用户名> <密码>");
    QCommandLineOption profileOpt("launch-profile",
        "用已保存的连接配置 <profile 名称> 直连（无凭据框，自动化测试用）", "name");
    parser.addOption(connectOpt);
    parser.addOption(quickOpt);
    parser.addOption(profileOpt);
    parser.addPositionalArgument("host", "目标主机（等价于 --connect）");
    parser.process(app);

    // 用已保存的 profile 直连：走 MainWindow::launch 路径，
    // 覆盖「保存了密码应直接使用」的行为（回归测试入口）
    if (parser.isSet(profileOpt)) {
        const QString name = parser.value(profileOpt);
        auto profiles = config::loadProfiles();
        const ConnectionProfile *hit = nullptr;
        for (const auto &p : profiles)
            if (p.name == name || p.host == name) { hit = &p; break; }
        if (!hit) {
            std::cerr << "未找到连接配置: " << name.toUtf8().constData() << std::endl;
            return 1;
        }
        MainWindow *mw = new MainWindow;
        mw->launchForTest(*hit);
        QObject::connect(mw, &MainWindow::destroyed, &app, [&app](QObject *) {});
        if (auto *s = mw->currentSession()) {
            QObject::connect(s, &SessionWindow::sessionClosed, &app,
                             [&app](const ConnectionProfile &) { QTimer::singleShot(600, &app, &QApplication::quit); });
            if (auto *v = s->view())
                QObject::connect(v, &RdpView::connected, &app, [] { logStatus("connected"); });
        }
        return app.exec();
    }

    if (parser.isSet(quickOpt)) {
        const QStringList pos = parser.positionalArguments();
        if (pos.size() < 1) {
            std::cerr << "--quick 用法: rdpcross --quick host[:port] [user] [password]" << std::endl;
            return 1;
        }
        ConnectionProfile p;
        p.host = pos.at(0);
        p.name = p.host;
        if (pos.size() > 1) p.username = pos.at(1);
        if (pos.size() > 2) p.password = pos.at(2);
        if (qEnvironmentVariableIsSet("RDPCROSS_TLS")) p.security = constants::kSecTls;
        if (qEnvironmentVariableIsSet("RDPCROSS_CERT_IGNORE")) p.certMode = constants::kCertIgnore;
        auto *win = new SessionWindow(p, p.password, true);
        QObject::connect(win, &SessionWindow::sessionClosed, &app, [&app](const ConnectionProfile &) {
            logStatus("session closed");
            // 异步关闭流程完成后再退出，避免打断 QProcess 清理
            QTimer::singleShot(600, &app, &QApplication::quit);
        });
        QObject::connect(win->view(), &RdpView::statusChanged, &app, logStatus);
        QObject::connect(win->view(), &RdpView::connected, &app, [] { logStatus("connected"); });
        win->show();
        return app.exec();
    }

    if (parser.isSet(connectOpt) || !parser.positionalArguments().isEmpty()) {
        const QString host = parser.isSet(connectOpt)
                                 ? parser.value(connectOpt)
                                 : parser.positionalArguments().first();
        ConnectionProfile p;
        p.host = host;
        p.name = host;
        if (qEnvironmentVariableIsSet("RDPCROSS_TLS")) p.security = constants::kSecTls;
        if (qEnvironmentVariableIsSet("RDPCROSS_CERT_IGNORE")) p.certMode = constants::kCertIgnore;
        auto *win = new SessionWindow(p, QString(), true);
        QObject::connect(win, &SessionWindow::sessionClosed, &app,
                         [](const ConnectionProfile &) { logStatus("session closed"); });
        win->show();
        return app.exec();
    }

    MainWindow win;
    win.show();
    return app.exec();
}
