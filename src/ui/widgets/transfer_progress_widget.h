#pragma once
#include <QWidget>

class QLabel;
class QProgressBar;

namespace linscp::ui::widgets {

/// Компактный виджет прогресса одной передачи:
/// [↑ filename.txt] [=====>    42%] [1.2 MB/s] [ETA 0:32]
class TransferProgressWidget : public QWidget {
    Q_OBJECT
public:
    explicit TransferProgressWidget(QWidget *parent = nullptr);

    void setFileName(const QString &name);
    void setProgress(int percent);
    void setSpeed(double bytesPerSec);
    void setEta(int seconds);
    void setDirection(bool upload); ///< true = upload

private:
    QLabel       *m_dirLabel;
    QLabel       *m_nameLabel;
    QProgressBar *m_bar;
    QLabel       *m_speedLabel;
    QLabel       *m_etaLabel;
};

} // namespace linscp::ui::widgets
