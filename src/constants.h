#ifndef RDPCROSS_CONSTANTS_H
#define RDPCROSS_CONSTANTS_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>

namespace constants {

inline const QString kAppName = "RDPCross";
inline const QString kAppVersion = "2.0.0";

// 分辨率预设：(名称, 宽, 高)；宽高 0 表示全屏
struct ResPreset { QString name; int w; int h; };
inline const QList<ResPreset> kResolutionPresets = {
    {"全屏", 0, 0},
    {"1280 × 720", 1280, 720},
    {"1366 × 768", 1366, 768},
    {"1440 × 900", 1440, 900},
    {"1600 × 900", 1600, 900},
    {"1680 × 1050", 1680, 1050},
    {"1920 × 1080", 1920, 1080},
    {"1920 × 1200", 1920, 1200},
    {"2560 × 1440", 2560, 1440},
    {"3840 × 2160", 3840, 2160},
};

inline const QList<int> kColorDepths = {32, 24, 16, 15};

// 连接速度预设 -> FreeRDP /network:*
inline const QStringList kNetworkPresets = {
    "auto", "modem", "broadband-low", "broadband-high", "wan", "lan",
};

// 音频模式
inline const QString kAudioClient = "client";
inline const QString kAudioServer = "server";
inline const QString kAudioNone = "none";

// 键盘模式
inline const QString kKeyboardLocal = "local";
inline const QString kKeyboardRemote = "remote";
inline const QString kKeyboardFullscreen = "fullscreen";

// 调整模式
inline const QString kResizeSmart = "smart";
inline const QString kResizeDynamic = "dynamic";
inline const QString kResizeFixed = "fixed";

// 安全层
inline const QString kSecNegotiate = "nego";
inline const QString kSecNla = "nla";
inline const QString kSecTls = "tls";
inline const QString kSecRdp = "rdp";
inline const QString kSecAad = "aad";

// 证书策略
inline const QString kCertTofu = "tofu";
inline const QString kCertAsk = "ask";
inline const QString kCertIgnore = "ignore";
inline const QString kCertDeny = "deny";

}  // namespace constants

#endif  // RDPCROSS_CONSTANTS_H
