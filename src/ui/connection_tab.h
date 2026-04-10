#pragma once
#include <QWidget>
#include <QUuid>
#include <memory>

#include "core/session/session_profile.h"

class QSplitter;

namespace linscp::core::session { class SessionStore; class SessionManager; }
namespace linscp::core::transfer { class TransferQueue; class TransferManager; }
namespace linscp::core::sftp    { class SftpClient; }
namespace linscp::core::sync    { class SyncEngine; }
namespace linscp::core::ssh     { class SshSession; enum class HostVerifyResult; }

namespace linscp::ui {

namespace panels  { class LocalPanel; class RemotePanel; }
namespace dialogs { class HostFingerprintDialog; class ProgressDialog; }

/// Один таб подключения: LocalPanel (слева) + RemotePanel (справа).
/// Управляет собственной SSH-сессией независимо от других табов.
class ConnectionTab : public QWidget {
    Q_OBJECT
public:
    explicit ConnectionTab(core::session::SessionStore  *store,
                           core::transfer::TransferQueue *sharedQueue,
                           QWidget *parent = nullptr);
    ~ConnectionTab() override;

    // ── Подключение ──────────────────────────────────────────────────────────
    void connectToSession(const QUuid &profileId);
    /// Подключиться по профилю напрямую (не обязательно сохранённому в store).
    /// Если профиль ещё не в store — добавляется временно до отключения.
    void connectToProfile(const core::session::SessionProfile &profile);
    void disconnectSession();

    bool      isConnected()  const { return m_sftp != nullptr; }
    QUuid     profileId()    const { return m_profileId; }
    QString   title()        const { return m_title; }

    // ── Доступ к компонентам ─────────────────────────────────────────────────
    panels::LocalPanel    *localPanel()  const { return m_localPanel; }
    panels::RemotePanel   *remotePanel() const { return m_remotePanel; }
    core::sync::SyncEngine *syncEngine() const { return m_syncEngine; }

    /// Сохранить/восстановить позицию разделителя
    QByteArray splitterState() const;
    void       restoreSplitterState(const QByteArray &state);

signals:
    void titleChanged(const QString &title);
    void statusChanged(const QString &msg);
    void connectionEstablished();
    void connectionLost();

private slots:
    void onSshConnected();
    void onSshError(const QString &msg);

private:
    void showPlaceholder();
    void replaceRemotePanel(QWidget *w);

    core::session::SessionStore   *m_store;
    core::transfer::TransferQueue *m_sharedQueue;

    // Runtime (живут пока подключены)
    QUuid                              m_profileId;
    QUuid                              m_tempProfileId;  ///< временный профиль (удаляется при disconnect)
    QString                            m_title = tr("Not connected");
    std::unique_ptr<core::session::SessionManager> m_sessionManager;
    core::sftp::SftpClient            *m_sftp            = nullptr;
    core::transfer::TransferManager   *m_transferManager = nullptr;
    core::sync::SyncEngine            *m_syncEngine      = nullptr;

    // UI
    QSplitter              *m_splitter      = nullptr;
    panels::LocalPanel     *m_localPanel    = nullptr;
    panels::RemotePanel    *m_remotePanel   = nullptr;   ///< nullptr если не подключены
    dialogs::ProgressDialog *m_progressDlg = nullptr;   ///< показывается при активных передачах
};

} // namespace linscp::ui
