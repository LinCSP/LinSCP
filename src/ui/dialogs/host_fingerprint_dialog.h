#pragma once
#include <QDialog>
#include "core/ssh/known_hosts.h"

namespace linscp::ui::dialogs {

/// Диалог верификации SSH fingerprint хоста.
/// Показывается когда хост новый (NewHost) или fingerprint изменился (Changed).
class HostFingerprintDialog : public QDialog {
    Q_OBJECT
public:
    enum class Decision { Accept, AcceptOnce, Reject };

    explicit HostFingerprintDialog(const QString &host, int port,
                                   const QByteArray &fingerprint,
                                   core::ssh::HostVerifyResult reason,
                                   QWidget *parent = nullptr);

    Decision decision() const { return m_decision; }

private:
    Decision m_decision = Decision::Reject;
};

} // namespace linscp::ui::dialogs
