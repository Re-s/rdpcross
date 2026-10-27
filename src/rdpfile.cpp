#include "rdpfile.h"

#include "constants.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStringDecoder>
#include <QJsonArray>
#include <QRegularExpression>

namespace rdpfile {

// .rdp 文本键值对结构：<key>:<type>:<value>
QJsonObject parseText(const QString &text) {
    QJsonObject d;
    for (const QString &rawLine : text.split(QRegularExpression("[\r\n]"))) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;
        // key:type:value（type 为单个字符）
        int p1 = line.indexOf(':');
        if (p1 < 1) continue;
        int p2 = line.indexOf(':', p1 + 1);
        if (p2 < p1 + 2 || p2 + 1 >= line.length()) continue;
        QString key = line.left(p1);
        QChar type = line.at(p1 + 1);
        if (type != 's' && type != 'i' && type != 'b') continue;
        d[key] = line.mid(p2 + 1);
    }
    return d;
}

namespace {

bool loadFileText(const QString &path, QString &out) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QByteArray raw = f.readAll();
    if (raw.startsWith("\xEF\xBB\xBF")) raw.remove(0, 3);
    // mstsc 默认 UTF-16LE
    if (raw.startsWith("\xFF\xFE")) {
        QStringDecoder dec(QStringDecoder::Utf16LE);
        out = dec(raw.mid(2));
        return true;
    }
    if (raw.startsWith("\xFE\xFF")) {
        QStringDecoder dec(QStringDecoder::Utf16BE);
        out = dec(raw.mid(2));
        return true;
    }
    out = QString::fromUtf8(raw);
    return true;
}

}  // namespace

bool exportProfile(const ConnectionProfile &p, const QString &path) {
    using namespace constants;
    QStringList lines;
    auto s = [&lines](const QString &k, const QString &v) { lines << k + ":s:" + v; };
    auto i = [&lines](const QString &k, int v) { lines << k + ":i:" + QString::number(v); };

    s("full address", p.host);
    if (!p.username.isEmpty()) s("username", p.username);
    if (!p.domain.isEmpty()) s("domain", p.domain);

    i("screen mode id", p.fullscreen ? 2 : 1);
    i("desktopwidth", p.width);
    i("desktopheight", p.height);
    i("session bpp", p.colorDepth);

    if (p.resizeMode == kResizeSmart) {
        i("smart sizing", 1);
    } else if (p.resizeMode == kResizeDynamic) {
        i("smart sizing", 1);
        i("dynamic resolution", 1);
    }
    if (p.fullscreen && p.multiMonitor) i("use multimon", 1);

    int hook = 1;
    if (p.keyboardMode == kKeyboardLocal) hook = 2;
    else if (p.keyboardMode == kKeyboardRemote) hook = 3;
    i("keyboardhook", hook);

    int amode = 0;
    if (p.audioMode == kAudioServer) amode = 1;
    else if (p.audioMode == kAudioNone) amode = 2;
    i("audiomode", amode);
    i("audiocapturemode", p.audioCapture ? 1 : 0);

    i("redirectclipboard", p.clipboard ? 1 : 0);
    i("redirectprinters", p.printers ? 1 : 0);
    i("redirectsmartcards", p.smartcards ? 1 : 0);
    i("redirectcomports", p.serialPorts.isEmpty() ? 0 : 1);
    if (p.plugAllDrives) {
        i("redirectdrives", 1);
    } else if (!p.drives.isEmpty()) {
        QStringList paths;
        for (const auto &d : p.drives) paths << d.path;
        s("drivestoredirect", paths.join(';'));
    }

    // 证书/身份验证
    int authLevel = 1;
    if (p.certMode == kCertIgnore) authLevel = 0;
    else if (p.certMode == kCertDeny) authLevel = 2;
    i("authentication level", authLevel);
    if (p.security != kSecNegotiate) i("negotiate security layer", 0);
    if (p.security == kSecNla) {
        i("enablecredsspsupport", 1);
        i("security protocol", 0);
    } else if (p.security == kSecTls) {
        i("security protocol", 1);
    } else if (p.security == kSecRdp) {
        i("security protocol", 0);
    }

    // 体验
    i("networkautodetect", p.network == "auto" ? 1 : 0);
    i("bandwidthautodetect", p.network == "auto" ? 1 : 0);
    i("disable themes", p.visualStyles ? 0 : 1);
    i("disable wallpaper", p.wallpaper ? 0 : 1);
    i("disable full window drag", p.fullWindowDrag ? 0 : 1);
    i("disable menu anims", p.menuAnimations ? 0 : 1);
    i("allow font smoothing", p.fontSmoothing ? 1 : 0);
    i("allow desktop composition", p.desktopComposition ? 1 : 0);
    i("bitmapcachepersistenable", p.persistentBitmapCache ? 1 : 0);
    i("autoreconnection enabled", p.reconnectAutomatically ? 1 : 0);

    if (!p.alternateShell.isEmpty()) s("alternate shell", p.alternateShell);
    if (!p.shellWorkingDir.isEmpty()) s("shell working directory", p.shellWorkingDir);

    const auto &gw = p.gateway;
    if (gw.enabled && !gw.host.isEmpty()) {
        s("gatewayhostname", gw.host);
        s("gatewayusername", gw.username);
        s("gatewaydomain", gw.domain);
        i("gatewayusagemethod", gw.usage == "always" ? 1 : 0);
    }

    // 本工具私有键（mstsc 与 FreeRDP 均忽略未知键）
    s("rdpcross name", p.name);
    s("rdpcross group", p.group);
    if (!p.notes.isEmpty()) s("rdpcross notes", p.notes);

    i("prompt for credentials", 0);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);
    ts.setGenerateByteOrderMark(false);
    ts.setEncoding(QStringConverter::Utf8);
    ts << lines.join("\r\n") << "\r\n";
    return true;
}

bool importProfile(const QString &path, ConnectionProfile &out) {
    using namespace constants;
    QString text;
    if (!loadFileText(path, text)) return false;
    QJsonObject d = parseText(text);

    QString fullAddress = d.value("full address").toString();
    if (fullAddress.isEmpty()) return false;

    auto vi = [&d](const QString &k, int def) {
        bool ok = false;
        int v = d.value(k).toString().toInt(&ok);
        return ok ? v : def;
    };

    ConnectionProfile p;
    p.host = fullAddress;
    // 名称：私有键 > 文件名 > 主机
    QString privName = d.value("rdpcross name").toString().trimmed();
    if (!privName.isEmpty()) p.name = privName;
    else p.name = QFileInfo(path).completeBaseName();
    p.group = d.value("rdpcross group").toString().trimmed();
    p.notes = d.value("rdpcross notes").toString();
    p.username = d.value("username").toString();
    p.domain = d.value("domain").toString();

    // 显示
    int screenMode = vi("screen mode id", 1);
    p.fullscreen = (screenMode == 2);
    p.width = vi("desktopwidth", 1280);
    p.height = vi("desktopheight", 800);
    p.colorDepth = vi("session bpp", 32);
    if (vi("smart sizing", 0)) {
        p.resizeMode = vi("dynamic resolution", 0) ? kResizeDynamic : kResizeSmart;
    } else {
        p.resizeMode = kResizeFixed;
    }
    p.multiMonitor = vi("use multimon", 0);

    // 键盘
    int hook = vi("keyboardhook", 1);
    if (hook == 2) p.keyboardMode = kKeyboardLocal;
    else if (hook == 3) p.keyboardMode = kKeyboardRemote;
    else p.keyboardMode = kKeyboardFullscreen;

    // 音频
    int amode = vi("audiomode", 0);
    if (amode == 1) p.audioMode = kAudioServer;
    else if (amode == 2) p.audioMode = kAudioNone;
    else p.audioMode = kAudioClient;
    p.audioCapture = vi("audiocapturemode", 0);

    // 本地资源
    p.clipboard = vi("redirectclipboard", 1);
    p.printers = vi("redirectprinters", 0);
    p.smartcards = vi("redirectsmartcards", 0);
    p.plugAllDrives = vi("redirectdrives", 0);
    QString drivesDirect = d.value("drivestoredirect").toString();
    if (!drivesDirect.isEmpty() && drivesDirect != "*") {
        for (const QString &part : drivesDirect.split(';')) {
            QString t = part.trimmed();
            if (t.isEmpty()) continue;
            QString label = QFileInfo(t).fileName();
            if (label.isEmpty()) label = t;
            p.drives.append({label, t});
        }
    }
    // 串口：.rdp 无逐项串口键
    (void) d;

    // 安全
    if (!vi("negotiate security layer", 1)) {
        if (vi("enablecredsspsupport", 0)) p.security = kSecNla;
        else if (vi("security protocol", 0) == 1) p.security = kSecTls;
        else p.security = kSecRdp;
    } else {
        p.security = kSecNegotiate;
    }
    int authLevel = vi("authentication level", 1);
    if (authLevel == 0) p.certMode = kCertIgnore;
    else if (authLevel == 2) p.certMode = kCertDeny;
    else p.certMode = kCertTofu;

    // 体验
    int connType = vi("connection type", 7);
    static const QHash<int, QString> connToNet = {
        {1, "modem"}, {2, "broadband-low"}, {3, "broadband-low"}, {4, "broadband-high"},
        {5, "wan"}, {6, "lan"}, {7, "auto"},
    };
    p.network = (vi("networkautodetect", 1) || vi("bandwidthautodetect", 1))
                    ? "auto"
                    : connToNet.value(connType, "auto");
    p.visualStyles = !vi("disable themes", 0);
    p.wallpaper = !vi("disable wallpaper", 0);
    p.fullWindowDrag = !vi("disable full window drag", 0);
    p.menuAnimations = !vi("disable menu anims", 0);
    p.fontSmoothing = vi("allow font smoothing", 1);
    p.desktopComposition = vi("allow desktop composition", 1);
    p.reconnectAutomatically = vi("autoreconnection enabled", 1);

    // 程序
    p.alternateShell = d.value("alternate shell").toString();
    p.shellWorkingDir = d.value("shell working directory").toString();

    // 网关
    QString gwhost = d.value("gatewayhostname").toString();
    if (!gwhost.isEmpty()) {
        p.gateway.enabled = true;
        p.gateway.host = gwhost;
        p.gateway.username = d.value("gatewayusername").toString();
        p.gateway.domain = d.value("gatewaydomain").toString();
        p.gateway.usage = (vi("gatewayusagemethod", 1) == 1) ? "always" : "detect";
    }

    out = p;
    return true;
}

}  // namespace rdpfile
