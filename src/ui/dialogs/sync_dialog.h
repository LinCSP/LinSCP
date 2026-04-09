#pragma once
#include <QDialog>
#include "core/sync/sync_profile.h"
#include "core/sync/sync_comparator.h"

class QTreeWidget;
class QRadioButton;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QStackedWidget;

namespace linscp::core::sync  { class SyncEngine; }

namespace linscp::ui::dialogs {

/// Диалог синхронизации директорий.
/// Страница 1: настройки профиля
/// Страница 2: preview изменений (dry-run)
/// Страница 3: прогресс применения
class SyncDialog : public QDialog {
    Q_OBJECT
public:
    explicit SyncDialog(core::sync::SyncEngine *engine,
                        const QString &localPath,
                        const QString &remotePath,
                        QWidget *parent = nullptr);

private slots:
    void onPreview();
    void onApply();
    void onPreviewReady(const QList<core::sync::SyncDiffEntry> &diff);
    void onSyncProgress(int percent);
    void onSyncFinished(bool success, const QString &error);

private:
    void setupUi(const QString &localPath, const QString &remotePath);
    void showPage(int page);
    void populateDiff(const QList<core::sync::SyncDiffEntry> &diff);

    core::sync::SyncEngine           *m_engine;
    QList<core::sync::SyncDiffEntry>  m_diff;

    QStackedWidget *m_stack;

    // Страница 0: настройки
    QLineEdit   *m_localPath;
    QLineEdit   *m_remotePath;
    QRadioButton *m_dirLocal;
    QRadioButton *m_dirRemote;
    QRadioButton *m_dirBoth;
    QRadioButton *m_cmMtime;
    QRadioButton *m_cmChecksum;
    QLineEdit   *m_excludeEdit;
    QCheckBox   *m_excludeHidden;

    // Страница 1: preview
    QTreeWidget *m_diffTree;
    QLabel      *m_diffSummary;

    // Страница 2: прогресс
    QProgressBar *m_progressBar;
    QLabel       *m_progressLabel;

    QPushButton *m_previewBtn;
    QPushButton *m_applyBtn;
    QPushButton *m_backBtn;
};

} // namespace linscp::ui::dialogs
