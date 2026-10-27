#include "freerdpargs.h"

#include "constants.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <unistd.h>
#include <QProcessEnvironment>

namespace freerdpargs {

QString findBinary() {
    // PATH 查找 + 常见安装位置
    for (const auto &name : {"xfreerdp3", "xfreerdp"}) {
        QString p = QStandardPaths::findExecutable(name);
        if (!p.isEmpty()) return p;
    }
    for (const auto &fixed : {"/usr/bin/xfreerdp3", "/usr/local/bin/xfreerdp3",
                              "/opt/freerdp/bin/xfreerdp3"}) {
        if (QFileInfo::exists(fixed)) return fixed;
    }
    return {};
}

QString detectAudioSys() {
    // PipeWire/PulseAudio 都通过 pulse 协议暴露：只有 socket 真的存在才用它。
    // 仅凭「装了 pactl」就选 pulse 会在守护进程没运行时让音频通道连接失败并反复重试，
    // 表现为声音卡顿甚至会话卡顿。
    auto env = QProcessEnvironment::systemEnvironment();
    const QString xdg = env.value("XDG_RUNTIME_DIR");
    const QString uid = QString::number(::getuid());
    const QStringList socks = {
        xdg + "/pulse/native",
        "/run/user/" + uid + "/pulse/native",
        "/run/pulse/native",
        "/tmp/pulse/native",
    };
    for (const QString &s : socks) {
        if (!s.isEmpty() && QFileInfo::exists(s)) return "pulse";
    }
    return "alsa";
}

namespace {

void addBool(QStringList &args, bool on, const char *flag) {
    args.append((on ? "+" : "-") + QString(flag));
}

}  // namespace

QStringList buildArgs(const ConnectionProfile &p, quint64 parentWid) {
    using namespace constants;
    QStringList args;

    args << "/v:" + p.host.trimmed();
    if (!p.username.trimmed().isEmpty())
        args << "/u:" + p.username.trimmed();
    if (!p.domain.trimmed().isEmpty())
        args << "/d:" + p.domain.trimmed();

    // 密码一律走 stdin（/from-stdin:force 在连接前提示读取，
    // 避免出现在 /proc/<pid>/cmdline 中）
    args << "/from-stdin:force";

    args << "/t:" + p.displayTitle();

    args << QString("/size:%1x%2").arg(p.width).arg(p.height);
    args << QString("/bpp:%1").arg(p.colorDepth);

    // 窗口缩放行为
    if (p.resizeMode == kResizeSmart)
        args << "/smart-sizing";
    else if (p.resizeMode == kResizeDynamic) {
        // 注意：/smart-sizing 与 /dynamic-resolution 在 FreeRDP 3 中互斥，
        // 同时传会导致 "Smart sizing and dynamic resolution are mutually exclusive"
        // 解析失败、进程直接退出（曾经的真实 bug）。
        args << "/dynamic-resolution";
    }
    // fixed: 不缩放，由会话窗口用滚动条容器处理

    // 多监视器
    if (p.fullscreen && p.multiMonitor)
        args << "/multimon";

    // 键盘
    if (p.keyboardMode == kKeyboardRemote)
        args << "+grab-keyboard";
    else if (p.keyboardMode == kKeyboardLocal)
        args << "-grab-keyboard";

    // 音频
    const QString sys = detectAudioSys();
    if (p.audioMode == kAudioClient)
        args << QString("/sound:sys:%1").arg(sys);
    else if (p.audioMode == kAudioServer)
        args << "/audio-mode:1";
    else if (p.audioMode == kAudioNone)
        args << "/audio-mode:2";
    if (p.audioCapture)
        args << QString("/microphone:sys:%1").arg(sys);

    // 图形编码（GFX）：Windows 8.1+/Server 2012R2+ 支持 Graphics Pipeline，
    // 走 H.264/渐进式编码能显著降低跨网络时的带宽与卡顿；老服务器会自动回退。
    if (p.gfx == "avc444")
        args << "/gfx:AVC444";
    else if (p.gfx == "progressive")
        args << "/gfx:progressive";
    else if (p.gfx == "off")
        args << "-gfx";

    // 剪贴板
    addBool(args, p.clipboard, "clipboard");
    // 打印机
    if (p.printers) args << "/printer";
    // 智能卡
    if (p.smartcards) args << "/smartcard";
    // 串口
    for (int i = 0; i < p.serialPorts.size() && i < 4; ++i) {
        QString dev = p.serialPorts[i].trimmed();
        if (!dev.isEmpty())
            args << QString("/serial:COM%1,%2").arg(i + 1).arg(dev);
    }
    // 磁盘重定向
    for (const auto &drv : p.drives) {
        QString name = drv.name.trimmed();
        if (name.isEmpty()) name = "share";
        if (!drv.path.isEmpty())
            args << QString("/drive:%1,%2").arg(name, drv.path);
    }
    if (p.plugAllDrives)
        args << "/drive:hotplug,*";

    // 体验
    if (p.network != "auto")
        args << "/network:" + p.network;
    addBool(args, p.desktopComposition, "aero");
    addBool(args, p.fullWindowDrag, "window-drag");
    addBool(args, p.menuAnimations, "menu-anims");
    addBool(args, p.visualStyles, "themes");
    addBool(args, p.fontSmoothing, "fonts");
    addBool(args, p.wallpaper, "wallpaper");
    if (p.persistentBitmapCache)
        args << "/cache:persist";
    addBool(args, p.reconnectAutomatically, "auto-reconnect");

    // 程序（备用 shell）
    if (!p.alternateShell.trimmed().isEmpty())
        args << "/shell:" + p.alternateShell.trimmed();
    if (!p.shellWorkingDir.trimmed().isEmpty())
        args << "/shell-dir:" + p.shellWorkingDir.trimmed();

    // 安全层
    if (p.security == kSecNla) args << "/sec:nla";
    else if (p.security == kSecTls) args << "/sec:tls";
    else if (p.security == kSecRdp) args << "/sec:rdp";
    else if (p.security == kSecAad) args << "/sec:aad";
    // nego 用默认协商，不加参数

    // 证书策略
    if (p.certMode == kCertTofu) args << "/cert:tofu";
    else if (p.certMode == kCertIgnore) args << "/cert:ignore";
    else if (p.certMode == kCertDeny) args << "/cert:deny";
    // ask：不加参数，FreeRDP 默认行为（不可信证书将连接失败）

    // RD 网关
    const auto &gw = p.gateway;
    if (gw.enabled && !gw.host.trimmed().isEmpty()) {
        QStringList parts;
        parts << "/gateway:g:" + gw.host.trimmed();
        if (!gw.username.trimmed().isEmpty()) parts << "u:" + gw.username.trimmed();
        if (!gw.domain.trimmed().isEmpty()) parts << "d:" + gw.domain.trimmed();
        if (!gw.password.isEmpty()) parts << "p:" + gw.password;
        args << parts.join(',');
    }

    // 嵌入父窗口
    if (parentWid)
        args << QString("/parent-window:%1").arg(parentWid);

    // 让 FreeRDP 把日志打到 stderr（便于解析）
    args << "/log-level:WARN";
    return args;
}

QStringList buildArgsMasked(const ConnectionProfile &p, quint64 parentWid) {
    ConnectionProfile q = p;
    q.password.clear();
    auto args = buildArgs(q, parentWid);
    // 隐藏网关密码
    const auto &gw = p.gateway;
    if (!gw.password.isEmpty()) {
        for (auto &a : args) a.replace(gw.password, "***");
    }
    return args;
}

QString classifyError(const QString &output) {
    using rx = QRegularExpression;
    // (中文归因, 匹配规则列表)
    static const QList<QPair<QString, QList<rx>>> rules = {
        {"身份验证失败：用户名或密码不正确",
         {rx("Logon failure", rx::CaseInsensitiveOption),
          rx("ERRCONNECT_LOGON_FAILURE|ERRCONNECT_USERNAME_PASSWORD", rx::CaseInsensitiveOption)}},
        {"身份验证失败：账户被锁定、禁用或受限",
         {rx("ACCOUNT_(DISABLED|RESTRICTION|LOCKED)", rx::CaseInsensitiveOption),
          rx("account is (disabled|locked)", rx::CaseInsensitiveOption)}},
        {"密码已过期，需要修改",
         {rx("PASSWORD_EXPIRED", rx::CaseInsensitiveOption),
          rx("password (has )?expired", rx::CaseInsensitiveOption)}},
        {"需要网络级身份验证（NLA）：服务器要求 NLA 但未协商成功",
         {rx("Network Level Authentication", rx::CaseInsensitiveOption),
          rx("CredSSP", rx::CaseInsensitiveOption)}},
        {"无法连接到服务器：网络不可达或主机无法解析",
         {rx("(Name or service not known|getaddrinfo|resolve)", rx::CaseInsensitiveOption),
          rx("connection refused", rx::CaseInsensitiveOption),
          rx("(timed out|timeout)", rx::CaseInsensitiveOption),
          rx("network is unreachable", rx::CaseInsensitiveOption)}},
        {"服务器端口未响应：未运行远程桌面服务或被防火墙拦截",
         {rx("ERRCONNECT_CONNECT_FAILED|ERRCONNECT_TRANSPORT_FAILED", rx::CaseInsensitiveOption)}},
        {"服务器证书无效或不受信任",
         {rx("certificate", rx::CaseInsensitiveOption),
          rx("SSL|TLS", rx::CaseInsensitiveOption),
          rx("ERRCONNECT_SSL", rx::CaseInsensitiveOption)}},
        {"协议版本不被支持（服务器版本过旧或过新）",
         {rx("ERRCONNECT_PROTOCOL_ERROR|unsupported protocol", rx::CaseInsensitiveOption)}},
        {"超出服务器最大连接数限制",
         {rx("maximum connections", rx::CaseInsensitiveOption)}},
    };
    for (const auto &rule : rules) {
        for (const auto &r : rule.second) {
            auto m = r.match(output);
            if (m.hasMatch()) return rule.first;
        }
    }
    return {};
}

}  // namespace freerdpargs
