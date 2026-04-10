#pragma once
#include <QDialog>
#include <QUuid>
#include "core/transfer/transfer_item.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

namespace linscp::core::transfer {
class TransferManager;
class TransferQueue;
}

namespace linscp::ui::dialogs {

/// Диалог прогресса передачи файлов — аналог WinSCP TProgressForm.
///
///   ┌──────────────────────────────────────────────────────────────┐
///   │  ↓ Downloading                                               │
///   │  Имя файла: large_archive.tar.gz                             │
///   │  Назначение: /home/user/downloads/large_archive.tar.gz       │
///   │                                                              │
///   │  Текущий файл:   [=============>         56%]  45.2 MB/s     │
///   │                  1.23 GB / 2.20 GB                           │
///   │                                                              │
///   │  Всего:          [======>                31%]                │
///   │                  3 из 9 файлов           ETA: 0:42           │
///   │                                                              │
///   │            [ В фон ]          [ Отмена ]                     │
///   └──────────────────────────────────────────────────────────────┘
class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgressDialog(core::transfer::TransferManager *manager,
                            core::transfer::TransferQueue   *queue,
                            QWidget *parent = nullptr);

    /// Показать диалог и начать слежение за указанным заданием.
    /// Можно вызывать несколько раз — следить всегда за активным.
    void trackTransfer(const QUuid &id);

signals:
    /// Пользователь нажал «В фон» — диалог скрыт, передача продолжается
    void movedToBackground();

private slots:
    void onTransferStarted(const QUuid &id);
    void onTransferFinished(const QUuid &id, bool success);
    void onProgressPoll();
    void onCancelClicked();
    void onBackgroundClicked();

private:
    void setupUi();
    void refreshFromItem(const core::transfer::TransferItem &item);
    QString formatBytes(qint64 bytes) const;
    QString formatEta(int seconds) const;

    core::transfer::TransferManager *m_manager;
    core::transfer::TransferQueue   *m_queue;

    QUuid   m_trackedId;        ///< задание, за которым следим
    int     m_totalFiles  = 0;  ///< сколько файлов в текущей сессии
    int     m_doneFiles   = 0;

    // ── Виджеты ──────────────────────────────────────────────────────────────
    QLabel       *m_dirLabel;       ///< "↓ Downloading" / "↑ Uploading"
    QLabel       *m_fileNameLabel;
    QLabel       *m_destLabel;

    QProgressBar *m_fileBar;
    QLabel       *m_fileSizeLabel;
    QLabel       *m_speedLabel;

    QProgressBar *m_totalBar;
    QLabel       *m_totalCountLabel;
    QLabel       *m_etaLabel;

    QPushButton  *m_bgBtn;
    QPushButton  *m_cancelBtn;

    QTimer       *m_pollTimer;      ///< 200 мс — обновление прогресса
};

} // namespace linscp::ui::dialogs
