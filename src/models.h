#ifndef RDPCROSS_MODELS_H
#define RDPCROSS_MODELS_H

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDateTime>

struct DriveRedirect {
    QString name;
    QString path;

    QJsonObject toJson() const {
        QJsonObject o;
        o["name"] = name;
        o["path"] = path;
        return o;
    }
    static DriveRedirect fromJson(const QJsonObject &o) {
        return {o.value("name").toString(), o.value("path").toString()};
    }
};

struct GatewaySettings {
    bool enabled = false;
    QString host;
    QString username;
    QString domain;
    QString password;
    QString usage = "always";  // always | detect

    QJsonObject toJson() const {
        QJsonObject o;
        o["enabled"] = enabled;
        o["host"] = host;
        o["username"] = username;
        o["domain"] = domain;
        o["password"] = password;
        o["usage"] = usage;
        return o;
    }
    static GatewaySettings fromJson(const QJsonObject &o) {
        return {o.value("enabled").toBool(), o.value("host").toString(),
                o.value("username").toString(), o.value("domain").toString(),
                o.value("password").toString(), o.value("usage").toString("always")};
    }
};

struct ConnectionProfile {
    QString id;
    QString name;
    QString group;
    QString host;
    QString username;
    QString domain;
    QString password;
    bool savePassword = false;

    // 显示
    bool fullscreen = false;
    int width = 1280;
    int height = 800;
    int colorDepth = 32;
    QString resizeMode = "smart";
    bool multiMonitor = false;

    // 本地资源
    QString keyboardMode = "fullscreen";
    QString audioMode = "client";
    bool audioCapture = false;
    bool clipboard = true;
    bool printers = false;
    QList<DriveRedirect> drives;
    bool smartcards = false;
    QStringList serialPorts;
    bool plugAllDrives = false;

    // 体验
    QString network = "auto";
    // 图形编码：auto(不指定) | progressive | avc444 | off
    // 默认 progressive：mstsc 默认就走图形管线，纯位图更新在跨网络时明显更卡
    QString gfx = "progressive";
    bool persistentBitmapCache = true;
    bool desktopComposition = true;
    bool fullWindowDrag = true;
    bool menuAnimations = true;
    bool visualStyles = true;
    bool fontSmoothing = true;
    bool wallpaper = true;
    bool reconnectAutomatically = true;

    // 程序
    QString alternateShell;
    QString shellWorkingDir;

    // 高级
    QString security = "nego";
    QString certMode = "tofu";
    GatewaySettings gateway;

    // 其他
    QString notes;
    qint64 lastUsed = 0;

    ConnectionProfile() : id(QUuid::createUuid().toString(QUuid::Id128)) {}

    QJsonObject toJson() const;
    static ConnectionProfile fromJson(const QJsonObject &d);

    QString hostOnly() const {
        QString h = host.trimmed();
        if (h.contains(':') && !h.startsWith('['))
            return h.section(':', 0, -2);
        return h;
    }
    int port() const {
        QString h = host.trimmed();
        if (h.contains(':') && !h.startsWith('[')) {
            bool ok = false;
            int p = h.section(':', -1).toInt(&ok);
            if (ok) return p;
        }
        return 3389;
    }
    QPair<int, int> resolution() const { return {width, height}; }
    QString displayTitle() const {
        QString t = name.trimmed();
        return t.isEmpty() ? (host + " - 远程桌面") : t;
    }
};

#endif  // RDPCROSS_MODELS_H
