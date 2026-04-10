#include "connection_tab.h"
#include "panels/local_panel.h"
#include "panels/remote_panel.h"
#include "dialogs/host_fingerprint_dialog.h"
#include "dialogs/auth_dialog.h"
#include "dialogs/progress_dialog.h"

#include "core/session/session_store.h"
#include "core/session/session_manager.h"
#include "core/session/session_profile.h"
#include "core/session/path_state_store.h"
#include "core/ssh/ssh_session.h"
#include "core/sftp/sftp_client.h"
#include "core/transfer/transfer_manager.h"
#include "core/transfer/transfer_queue.h"
#include "core/sync/sync_engine.h"

#include <QSplitter>
#include <QLabel>
#include <QVBoxLayout>

namespace linscp::ui {

ConnectionTab::ConnectionTab(core::session::SessionStore    *store,
                             core::transfer::TransferQueue  *sharedQueue,
                             core::session::PathStateStore  *pathState,
                             QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_sharedQueue(sharedQueue)
    , m_pathState(pathState)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_localPanel = new panels::LocalPanel(this);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(4);
    m_splitter->addWidget(m_localPanel);
    showPlaceholder();

    layout->addWidget(m_splitter);
}

ConnectionTab::~ConnectionTab()
{
    QObject::disconnect(this, nullptr, nullptr, nullptr);
    disconnectSession();
}

// ── Подключение ───────────────────────────────────────────────────────────────

void ConnectionTab::connectToSession(const QUuid &profileId)
{
    if (isConnected()) disconnectSession();

    m_profileId = profileId;
    m_sessionManager = std::make_unique<core::session::SessionManager>(m_store, this);

    connect(m_sessionManager.get(), &core::session::SessionManager::sessionOpened,
            this, &ConnectionTab::onSshConnected);
    connect(m_sessionManager.get(), &core::session::SessionManager::sessionError,
            this, [this](const QUuid &, const QString &msg) { onSshError(msg); });

    auto *sshSession = m_sessionManager->open(profileId);
    if (!sshSession) {
        emit statusChanged(tr("Failed to create session"));
        return;
    }

    auto *authDlg = new dialogs::AuthDialog(sshSession, this);
    authDlg->setAttribute(Qt::WA_DeleteOnClose);
    authDlg->setModal(true);
    authDlg->show();

    connect(sshSession, &core::ssh::SshSession::hostVerificationRequired,
            this, [this, sshSession, authDlg](core::ssh::HostVerifyResult reason,
                                               const QByteArray &fp) {
        authDlg->hide();
        const auto profile = m_store->find(m_profileId);
        dialogs::HostFingerprintDialog dlg(profile.host, profile.port, fp, reason, this);
        dlg.exec();
        switch (dlg.decision()) {
        case dialogs::HostFingerprintDialog::Decision::Accept:
            authDlg->show();
            sshSession->acceptHost();
            break;
        case dialogs::HostFingerprintDialog::Decision::AcceptOnce:
            authDlg->show();
            sshSession->acceptHostOnce();
            break;
        case dialogs::HostFingerprintDialog::Decision::Reject:
            authDlg->close();
            sshSession->rejectHost();
            break;
        }
    }, Qt::QueuedConnection);

    const auto profile = m_store->find(profileId);
    m_title = tr("Connecting…");
    emit titleChanged(m_title);
    emit statusChanged(tr("Connecting to %1…").arg(profile.host));
}

void ConnectionTab::connectToProfile(const core::session::SessionProfile &profile)
{
    const auto existing = m_store->find(profile.id);
    if (!existing.isValid()) {
        m_store->add(profile);
        m_tempProfileId = profile.id;
    }
    connectToSession(profile.id);
}

void ConnectionTab::disconnectSession()
{
    delete m_progressDlg;     m_progressDlg     = nullptr;
    delete m_transferManager; m_transferManager = nullptr;
    delete m_syncEngine;      m_syncEngine      = nullptr;
    delete m_sftp;            m_sftp            = nullptr;

    if (m_sessionManager)
        m_sessionManager->closeAll();

    if (!m_tempProfileId.isNull()) {
        m_store->remove(m_tempProfileId);
        m_tempProfileId = {};
    }

    m_sessionManager.reset();
    m_profileId = {};
    m_title = tr("Not connected");

    if (m_remotePanel) {
        m_remotePanel->deleteLater();
        m_remotePanel = nullptr;
    }
    showPlaceholder();

    emit titleChanged(m_title);
    emit connectionLost();
}

// ── Слоты SessionManager ──────────────────────────────────────────────────────

void ConnectionTab::onSshConnected()
{
    auto *ssh = m_sessionManager->active(m_profileId);
    if (!ssh) return;

    m_sftp = new core::sftp::SftpClient(ssh, this);

    m_transferManager = new core::transfer::TransferManager(
        m_sftp, m_sharedQueue, this);
    m_transferManager->start();

    m_progressDlg = new dialogs::ProgressDialog(
        m_transferManager, m_sharedQueue, this);
    connect(m_transferManager, &core::transfer::TransferManager::transferStarted,
            m_progressDlg, &dialogs::ProgressDialog::trackTransfer);

    m_syncEngine = new core::sync::SyncEngine(m_sftp, m_sharedQueue, this);

    if (m_remotePanel) {
        m_remotePanel->deleteLater();
        m_remotePanel = nullptr;
    }
    m_remotePanel = new panels::RemotePanel(m_sftp, m_sharedQueue, this);
    replaceRemotePanel(m_remotePanel);

    connect(m_localPanel,  &panels::LocalPanel::uploadRequested,
            this, [this](const QStringList &files, const QString &) {
        if (m_remotePanel) m_remotePanel->uploadFiles(files);
    });
    connect(m_remotePanel, &panels::RemotePanel::downloadRequested,
            this, [this](const QStringList &, const QString &) {
        if (m_remotePanel)
            m_remotePanel->downloadSelected(m_localPanel->currentPath());
    });

    // Сохранять текущие пути при навигации
    if (m_pathState) {
        connect(m_localPanel, &panels::FilePanel::pathChanged,
                this, [this](const QString &path) {
            auto state = m_pathState->load(m_profileId);
            state.localPath = path;
            m_pathState->save(m_profileId, state);
        });
        connect(m_remotePanel, &panels::FilePanel::pathChanged,
                this, [this](const QString &path) {
            auto state = m_pathState->load(m_profileId);
            state.remotePath = path;
            m_pathState->save(m_profileId, state);
        });
    }

    const auto profile = m_store->find(m_profileId);
    m_title = profile.name.isEmpty()
                  ? tr("%1@%2").arg(profile.username, profile.host)
                  : profile.name;
    emit titleChanged(m_title);
    emit statusChanged(tr("Connected: %1@%2:%3")
                           .arg(profile.username, profile.host)
                           .arg(profile.port));
    emit connectionEstablished();

    // Восстановить пути: сначала из PathStateStore, иначе из профиля
    if (m_pathState) {
        const auto state = m_pathState->load(m_profileId);
        if (!state.localPath.isEmpty())
            m_localPanel->navigateTo(state.localPath);
        const QString remotePath = !state.remotePath.isEmpty()
                                       ? state.remotePath
                                       : (profile.initialRemotePath.isEmpty()
                                              ? QStringLiteral("/")
                                              : profile.initialRemotePath);
        m_remotePanel->navigateTo(remotePath);
    } else {
        m_remotePanel->navigateTo(
            profile.initialRemotePath.isEmpty() ? QStringLiteral("/") : profile.initialRemotePath);
    }
}

void ConnectionTab::onSshError(const QString &msg)
{
    m_title = tr("Error");
    emit titleChanged(m_title);
    emit statusChanged(tr("Error: %1").arg(msg));
    emit connectionLost();
}

// ── Утилиты ───────────────────────────────────────────────────────────────────

core::ssh::SshSession *ConnectionTab::sshSession() const
{
    if (!m_sessionManager || m_profileId.isNull()) return nullptr;
    return m_sessionManager->active(m_profileId);
}

void ConnectionTab::showPlaceholder()
{
    auto *ph = new QLabel(
        tr("<center>"
           "<br><br>"
           "<b>Not connected</b><br><br>"
           "Select a session in the toolbar and click <b>Connect</b>,<br>"
           "or create a new session via <b>File → New Session…</b>"
           "</center>"),
        this);
    ph->setAlignment(Qt::AlignCenter);
    ph->setStyleSheet("QLabel { color: palette(mid); font-size: 13px; }");
    replaceRemotePanel(ph);
}

void ConnectionTab::replaceRemotePanel(QWidget *w)
{
    if (m_splitter->count() == 1) {
        m_splitter->addWidget(w);
    } else {
        QWidget *old = m_splitter->widget(1);
        m_splitter->replaceWidget(1, w);
        if (old && old != m_remotePanel)
            old->deleteLater();
    }
    m_splitter->setSizes({500, 500});
}

QByteArray ConnectionTab::splitterState() const
{
    return m_splitter->saveState();
}

void ConnectionTab::restoreSplitterState(const QByteArray &state)
{
    m_splitter->restoreState(state);
}

} // namespace linscp::ui
