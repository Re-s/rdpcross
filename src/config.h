#ifndef RDPCROSS_CONFIG_H
#define RDPCROSS_CONFIG_H

#include <QList>
#include <QDir>
#include <QJsonObject>
#include "models.h"

namespace config {

// ~/.config/rdpcross/profiles.json（长度为 0 时给出空列表）
QDir configDir();
QList<ConnectionProfile> loadProfiles();
bool saveProfiles(const QList<ConnectionProfile> &profiles);
QList<ConnectionProfile> upsertProfile(const ConnectionProfile &p);  // 写盘并返回最新列表
QList<ConnectionProfile> deleteProfile(const QString &id);
void touchProfile(ConnectionProfile &p);

QJsonObject loadSettings();
void saveSettings(const QJsonObject &settings);

}  // namespace config

#endif  // RDPCROSS_CONFIG_H
