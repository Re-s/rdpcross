#ifndef RDPCROSS_CREDENTIALSDIALOG_H
#define RDPCROSS_CREDENTIALSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include "models.h"

// 连接前的凭据询问对话框（未保存密码时弹出，等价 mstsc「Windows 安全」提示）。
class CredentialsDialog : public QDialog {
    Q_OBJECT
public:
    explicit CredentialsDialog(const ConnectionProfile &profile, QWidget *parent = nullptr);

    // (用户名, 域, 密码, 是否记住密码)
    std::tuple<QString, QString, QString, bool> values() const;

    // 模态询问；取消返回 false
    static bool ask(const ConnectionProfile &profile, QWidget *parent,
                    QString &user, QString &domain, QString &password, bool &savePassword);

private:
    QLineEdit *m_user = nullptr;
    QLineEdit *m_domain = nullptr;
    QLineEdit *m_pw = nullptr;
    QCheckBox *m_save = nullptr;
};

#endif  // RDPCROSS_CREDENTIALSDIALOG_H
