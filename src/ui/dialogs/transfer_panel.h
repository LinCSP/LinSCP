#pragma once
#include <QWidget>

class QTableView;
class QPushButton;
class QLabel;

namespace linscp::core::transfer { class TransferQueue; }
namespace linscp::models { class TransferQueueModel; }

namespace linscp::ui::dialogs {

/// Нижняя панель очереди передач.
/// Показывает таблицу заданий + кнопки Pause/Resume/Cancel/Clear.
class TransferPanel : public QWidget {
    Q_OBJECT
public:
    explicit TransferPanel(core::transfer::TransferQueue *queue, QWidget *parent = nullptr);

private slots:
    void onPause();
    void onResume();
    void onCancel();
    void onClearCompleted();
    void onSelectionChanged();

private:
    void setupUi();

    core::transfer::TransferQueue  *m_queue;
    models::TransferQueueModel     *m_model;

    QTableView  *m_table;
    QPushButton *m_pauseBtn;
    QPushButton *m_resumeBtn;
    QPushButton *m_cancelBtn;
    QPushButton *m_clearBtn;
    QLabel      *m_summaryLabel;
};

} // namespace linscp::ui::dialogs
