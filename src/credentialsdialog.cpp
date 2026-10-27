#include "credentialsdialog.h"

#include <QFormLayout>
#include <QDialogButtonBox>
#include <QVBoxLayout>

CredentialsDialog::CredentialsDialog(const ConnectionProfile &profile, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Windows 安全");
    setMinimumWidth(380);

    m_user = new QLineEdit(profile.username, this);
    m_domain = new QLineEdit(profile.domain, this);
    m_pw = new QLineEdit(this);
    m_pw->setEchoMode(QLineEdit::Password);
    m_save = new QCheckBox("记住密码（保存在本地配置文件）", this);
    m_save->setChecked(profile.savePassword);

    auto *form = new QFormLayout;
    form->addRow("用户名:", m_user);
    form->addRow("域:", m_domain);
    form->addRow("密码:", m_pw);
    form->addRow(m_save);

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addWidget(btns);

    if (m_user->text().isEmpty()) m_user->setFocus();
    else m_pw->setFocus();
}

std::tuple<QString, QString, QString, bool> CredentialsDialog::values() const {
    return {m_user->text().trimmed(), m_domain->text().trimmed(), m_pw->text(),
            m_save->isChecked()};
}

bool CredentialsDialog::ask(const ConnectionProfile &profile, QWidget *parent,
                            QString &user, QString &domain, QString &password,
                            bool &savePassword) {
    CredentialsDialog dlg(profile, parent);
    if (dlg.exec() != QDialog::Accepted) return false;
    auto [u, d, p, s] = dlg.values();
    user = u; domain = d; password = p; savePassword = s;
    return true;
}
