#include "config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDateTime>

namespace config {

QDir configDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QDir d(base + "/rdpcross");
    if (!d.exists()) d.mkpath(".");
    QFile::setPermissions(d.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return d;
}

static bool writeJson(const QString &path, const QJsonDocument &doc) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    return true;
}

QList<ConnectionProfile> loadProfiles() {
    QFile f(configDir().filePath("profiles.json"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) return {};
    QList<ConnectionProfile> out;
    for (const auto &v : doc.array()) {
        if (v.isObject())
            out.append(ConnectionProfile::fromJson(v.toObject()));
    }
    return out;
}

bool saveProfiles(const QList<ConnectionProfile> &profiles) {
    QJsonArray a;
    for (const auto &p : profiles) a.append(p.toJson());
    return writeJson(configDir().filePath("profiles.json"), QJsonDocument(a));
}

QList<ConnectionProfile> upsertProfile(const ConnectionProfile &p) {
    auto profiles = loadProfiles();
    bool found = false;
    for (int i = 0; i < profiles.size(); ++i) {
        if (profiles[i].id == p.id) {
            profiles[i] = p;
            found = true;
            break;
        }
    }
    if (!found) profiles.append(p);
    saveProfiles(profiles);
    return profiles;
}

QList<ConnectionProfile> deleteProfile(const QString &id) {
    auto profiles = loadProfiles();
    auto it = std::remove_if(profiles.begin(), profiles.end(),
                             [&id](const ConnectionProfile &p) { return p.id == id; });
    profiles.erase(it, profiles.end());
    saveProfiles(profiles);
    return profiles;
}

void touchProfile(ConnectionProfile &p) {
    p.lastUsed = QDateTime::currentSecsSinceEpoch();
    upsertProfile(p);
}

QJsonObject loadSettings() {
    QFile f(configDir().filePath("settings.json"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    return (err.error == QJsonParseError::NoError && doc.isObject()) ? doc.object() : QJsonObject();
}

void saveSettings(const QJsonObject &settings) {
    writeJson(configDir().filePath("settings.json"), QJsonDocument(settings));
}

}  // namespace config
