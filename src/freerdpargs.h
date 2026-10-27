#ifndef RDPCROSS_FREERDPARGS_H
#define RDPCROSS_FREERDPARGS_H

#include <QStringList>
#include "models.h"

namespace freerdpargs {

// 查找 xfreerdp 可执行文件路径
QString findBinary();

// 检测音频子系统：pulse / alsa
QString detectAudioSys();

// 由 ConnectionProfile 生成 xfreerdp3 参数（与 mstsc 设置语义一致）
QStringList buildArgs(const ConnectionProfile &p, quint64 parentWid = 0);

// 备用：仅诊断时使用（密码不落盘）
QStringList buildArgsMasked(const ConnectionProfile &p, quint64 parentWid = 0);

// 从 FreeRDP 输出中归因失败原因（中文）；无匹配返回空串
QString classifyError(const QString &output);

}  // namespace freerdpargs

#endif  // RDPCROSS_FREERDPARGS_H
