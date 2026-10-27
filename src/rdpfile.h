#ifndef RDPCROSS_RDPFILE_H
#define RDPCROSS_RDPFILE_H

#include <QJsonObject>
#include <QList>
#include "models.h"

namespace rdpfile {

// 导出 ConnectionProfile 为 .rdp 文件（与 mstsc 互通）
bool exportProfile(const ConnectionProfile &p, const QString &path);

// 从 .rdp 文件构造 ConnectionProfile；失败返回 false
bool importProfile(const QString &path, ConnectionProfile &out);

// 解析 .rdp 文本为 key->value 字典（供测试使用）
QJsonObject parseText(const QString &text);

}  // namespace rdpfile

#endif  // RDPCROSS_RDPFILE_H
