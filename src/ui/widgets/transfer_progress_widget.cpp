#include "transfer_progress_widget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>

namespace linscp::ui::widgets {

TransferProgressWidget::TransferProgressWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(8);

    m_dirLabel   = new QLabel(this);
    m_dirLabel->setFixedWidth(18);
    m_dirLabel->setAlignment(Qt::AlignCenter);

    m_nameLabel  = new QLabel(this);
    m_nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_bar = new QProgressBar(this);
    m_bar->setRange(0, 100);
    m_bar->setFixedWidth(120);
    m_bar->setTextVisible(true);
    m_bar->setFormat(QStringLiteral("%p%"));

    m_speedLabel = new QLabel(this);
    m_speedLabel->setFixedWidth(80);
    m_speedLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_etaLabel = new QLabel(this);
    m_etaLabel->setFixedWidth(60);
    m_etaLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    layout->addWidget(m_dirLabel);
    layout->addWidget(m_nameLabel, 1);
    layout->addWidget(m_bar);
    layout->addWidget(m_speedLabel);
    layout->addWidget(m_etaLabel);
}

void TransferProgressWidget::setFileName(const QString &name)
{
    m_nameLabel->setText(name);
    m_nameLabel->setToolTip(name);
}

void TransferProgressWidget::setProgress(int percent)
{
    m_bar->setValue(percent);
}

void TransferProgressWidget::setDirection(bool upload)
{
    m_dirLabel->setText(upload ? QStringLiteral("↑") : QStringLiteral("↓"));
}

void TransferProgressWidget::setSpeed(double bytesPerSec)
{
    QString s;
    if (bytesPerSec < 1024)
        s = QStringLiteral("%1 B/s").arg(bytesPerSec, 0, 'f', 0);
    else if (bytesPerSec < 1024 * 1024)
        s = QStringLiteral("%1 KB/s").arg(bytesPerSec / 1024, 0, 'f', 1);
    else
        s = QStringLiteral("%1 MB/s").arg(bytesPerSec / (1024 * 1024), 0, 'f', 2);
    m_speedLabel->setText(s);
}

void TransferProgressWidget::setEta(int seconds)
{
    if (seconds < 0) { m_etaLabel->clear(); return; }
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    m_etaLabel->setText(h > 0
                            ? QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'))
                            : QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QChar('0')));
}

} // namespace linscp::ui::widgets
