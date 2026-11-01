#ifndef RDPCROSS_ABOUTDIALOG_H
#define RDPCROSS_ABOUTDIALOG_H

#include <QDialog>

// 「关于」对话框：展示应用名称、自动跟随构建的完整版本号（v基础版本-提交数-g哈希）、
// 功能简介与运行时技术信息（Qt 版本、xfreerdp 版本），提供版本号一键复制。
class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

#endif  // RDPCROSS_ABOUTDIALOG_H
