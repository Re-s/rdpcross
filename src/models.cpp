#include "models.h"

QJsonObject ConnectionProfile::toJson() const {
    QJsonObject o;
    o["id"] = id;
    o["name"] = name;
    o["group"] = group;
    o["host"] = host;
    o["username"] = username;
    o["domain"] = domain;
    o["password"] = password;
    o["save_password"] = savePassword;

    o["fullscreen"] = fullscreen;
    o["width"] = width;
    o["height"] = height;
    o["color_depth"] = colorDepth;
    o["resize_mode"] = resizeMode;
    o["multi_monitor"] = multiMonitor;

    o["keyboard_mode"] = keyboardMode;
    o["audio_mode"] = audioMode;
    o["audio_capture"] = audioCapture;
    o["clipboard"] = clipboard;
    o["printers"] = printers;
    QJsonArray dr;
    for (const auto &d : drives) dr.append(d.toJson());
    o["drives"] = dr;
    o["smartcards"] = smartcards;
    o["serial_ports"] = QJsonArray::fromStringList(serialPorts);
    o["plug_all_drives"] = plugAllDrives;

    o["network"] = network;
    o["gfx"] = gfx;
    o["persistent_bitmap_cache"] = persistentBitmapCache;
    o["desktop_composition"] = desktopComposition;
    o["full_window_drag"] = fullWindowDrag;
    o["menu_animations"] = menuAnimations;
    o["visual_styles"] = visualStyles;
    o["font_smoothing"] = fontSmoothing;
    o["wallpaper"] = wallpaper;
    o["reconnect_automatically"] = reconnectAutomatically;

    o["alternate_shell"] = alternateShell;
    o["shell_working_dir"] = shellWorkingDir;

    o["security"] = security;
    o["cert_mode"] = certMode;
    o["gateway"] = gateway.toJson();

    o["notes"] = notes;
    o["last_used"] = lastUsed;
    return o;
}

ConnectionProfile ConnectionProfile::fromJson(const QJsonObject &d) {
    ConnectionProfile p;
    p.id = d.value("id").toString(p.id);
    p.name = d.value("name").toString();
    p.group = d.value("group").toString();
    p.host = d.value("host").toString();
    p.username = d.value("username").toString();
    p.domain = d.value("domain").toString();
    p.password = d.value("password").toString();
    p.savePassword = d.value("save_password").toBool();

    p.fullscreen = d.value("fullscreen").toBool();
    p.width = d.value("width").toInt(1280);
    p.height = d.value("height").toInt(800);
    p.colorDepth = d.value("color_depth").toInt(32);
    p.resizeMode = d.value("resize_mode").toString("smart");
    p.multiMonitor = d.value("multi_monitor").toBool();

    p.keyboardMode = d.value("keyboard_mode").toString("fullscreen");
    p.audioMode = d.value("audio_mode").toString("client");
    p.audioCapture = d.value("audio_capture").toBool();
    p.clipboard = d.value("clipboard").toBool(true);
    p.printers = d.value("printers").toBool();
    for (const auto &v : d.value("drives").toArray())
        p.drives.append(DriveRedirect::fromJson(v.toObject()));
    p.smartcards = d.value("smartcards").toBool();
    for (const auto &v : d.value("serial_ports").toArray())
        p.serialPorts.append(v.toString());
    p.plugAllDrives = d.value("plug_all_drives").toBool();

    p.network = d.value("network").toString("auto");
    p.gfx = d.value("gfx").toString("progressive");
    p.persistentBitmapCache = d.value("persistent_bitmap_cache").toBool(true);
    p.desktopComposition = d.value("desktop_composition").toBool(true);
    p.fullWindowDrag = d.value("full_window_drag").toBool(true);
    p.menuAnimations = d.value("menu_animations").toBool(true);
    p.visualStyles = d.value("visual_styles").toBool(true);
    p.fontSmoothing = d.value("font_smoothing").toBool(true);
    p.wallpaper = d.value("wallpaper").toBool(true);
    p.reconnectAutomatically = d.value("reconnect_automatically").toBool(true);

    p.alternateShell = d.value("alternate_shell").toString();
    p.shellWorkingDir = d.value("shell_working_dir").toString();

    p.security = d.value("security").toString("nego");
    p.certMode = d.value("cert_mode").toString("tofu");
    p.gateway = GatewaySettings::fromJson(d.value("gateway").toObject());

    p.notes = d.value("notes").toString();
    p.lastUsed = d.value("last_used").toVariant().toLongLong();
    return p;
}
