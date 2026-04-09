#include "host_fingerprint_dialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QIcon>

namespace linscp::ui::dialogs {

HostFingerprintDialog::HostFingerprintDialog(const QString &host, int port,
                                             const QByteArray &fingerprint,
                                             core::ssh::HostVerifyResult reason,
                                             QWidget *parent)
    : QDialog(parent)
{
    const bool isChanged = (reason == core::ssh::HostVerifyResult::Changed);

    setWindowTitle(isChanged ? tr("WARNING: Host Identification Changed!")
                             : tr("Unknown Host Key"));
    setMinimumWidth(480);

    auto *layout = new QVBoxLayout(this);

    // Иконка + заголовок
    auto *headerLabel = new QLabel(this);
    if (isChanged) {
        headerLabel->setText(
            tr("<b><font color='red'>WARNING: The fingerprint of %1 (port %2) "
               "has changed!</font></b><br><br>"
               "This may indicate a man-in-the-middle attack or the server key "
               "was regenerated. Verify with the server administrator before "
               "accepting.")
                .arg(host).arg(port));
    } else {
        headerLabel->setText(
            tr("The host <b>%1</b> (port %2) is not in your known hosts list. "
               "Verify the fingerprint below before connecting.")
                .arg(host).arg(port));
    }
    headerLabel->setWordWrap(true);
    layout->addWidget(headerLabel);

    // Fingerprint
    const QString hex = QString::fromLatin1(fingerprint.toHex(':'));
    auto *fpLabel = new QLabel(
        tr("<br><b>SHA-256 fingerprint:</b><br>"
           "<tt>%1</tt>").arg(hex),
        this);
    fpLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(fpLabel);

    layout->addSpacing(8);

    // Кнопки
    auto *buttons = new QDialogButtonBox(this);

    auto *acceptBtn = buttons->addButton(
        isChanged ? tr("Accept (update stored key)") : tr("Accept"),
        QDialogButtonBox::AcceptRole);
    auto *onceBtn = buttons->addButton(tr("Accept once (don't save)"),
                                       QDialogButtonBox::ActionRole);
    auto *rejectBtn = buttons->addButton(tr("Reject"), QDialogButtonBox::RejectRole);

    if (isChanged) {
        acceptBtn->setIcon(QIcon::fromTheme("dialog-warning"));
        rejectBtn->setDefault(true);
    } else {
        acceptBtn->setDefault(true);
    }

    connect(acceptBtn, &QPushButton::clicked, this, [this]() {
        m_decision = Decision::Accept;
        accept();
    });
    connect(onceBtn, &QPushButton::clicked, this, [this]() {
        m_decision = Decision::AcceptOnce;
        accept();
    });
    connect(rejectBtn, &QPushButton::clicked, this, [this]() {
        m_decision = Decision::Reject;
        reject();
    });

    layout->addWidget(buttons);
}

} // namespace linscp::ui::dialogs
