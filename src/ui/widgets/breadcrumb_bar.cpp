#include "breadcrumb_bar.h"
#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QStackedWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QStyle>

namespace linscp::ui::widgets {

BreadcrumbBar::BreadcrumbBar(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(28);

    auto *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_stack = new QStackedWidget(this);

    // Страница 0: breadcrumbs
    m_crumbWidget = new QWidget(m_stack);
    m_crumbWidget->setObjectName("BreadcrumbBar_crumbs");
    m_crumbLayout = new QHBoxLayout(m_crumbWidget);
    m_crumbLayout->setContentsMargins(4, 0, 4, 0);
    m_crumbLayout->setSpacing(0);
    m_crumbLayout->addStretch();

    // Страница 1: ввод пути
    m_lineEdit = new QLineEdit(m_stack);
    m_lineEdit->setClearButtonEnabled(true);
    connect(m_lineEdit, &QLineEdit::editingFinished,
            this, &BreadcrumbBar::onEditingFinished);
    connect(m_lineEdit, &QLineEdit::returnPressed,
            this, &BreadcrumbBar::onEditingFinished);

    m_stack->addWidget(m_crumbWidget);
    m_stack->addWidget(m_lineEdit);
    m_stack->setCurrentIndex(0);

    outerLayout->addWidget(m_stack);
}

void BreadcrumbBar::setPath(const QString &path)
{
    if (m_path == path) return;
    m_path = path;
    if (!m_editMode) rebuildCrumbs();
    emit pathChanged(path);
}

void BreadcrumbBar::rebuildCrumbs()
{
    // Удалить старые кнопки (кроме stretch в конце)
    while (m_crumbLayout->count() > 1) {
        QLayoutItem *item = m_crumbLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    // Разбить путь на сегменты
    const QStringList parts = m_path.split('/', Qt::SkipEmptyParts);
    QString accumulated = "/";

    // Корневой сегмент
    auto *rootBtn = new QToolButton(m_crumbWidget);
    rootBtn->setText("/");
    rootBtn->setProperty("crumbPath", accumulated);
    rootBtn->setAutoRaise(true);
    connect(rootBtn, &QToolButton::clicked, this, &BreadcrumbBar::onSegmentClicked);
    m_crumbLayout->insertWidget(m_crumbLayout->count() - 1, rootBtn);

    for (const QString &part : parts) {
        accumulated += (accumulated.endsWith('/') ? part : '/' + part);

        auto *sep = new QLabel(QStringLiteral(" › "), m_crumbWidget);
        sep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_crumbLayout->insertWidget(m_crumbLayout->count() - 1, sep);

        auto *btn = new QToolButton(m_crumbWidget);
        btn->setText(part);
        btn->setProperty("crumbPath", accumulated);
        btn->setAutoRaise(true);
        connect(btn, &QToolButton::clicked, this, &BreadcrumbBar::onSegmentClicked);
        m_crumbLayout->insertWidget(m_crumbLayout->count() - 1, btn);
    }
}

void BreadcrumbBar::onSegmentClicked()
{
    auto *btn = qobject_cast<QToolButton *>(sender());
    if (!btn) return;
    const QString path = btn->property("crumbPath").toString();
    emit pathRequested(path);
}

void BreadcrumbBar::enterEditMode()
{
    m_editMode = true;
    m_lineEdit->setText(m_path);
    m_stack->setCurrentIndex(1);
    m_lineEdit->selectAll();
    m_lineEdit->setFocus();
}

void BreadcrumbBar::leaveEditMode()
{
    m_editMode = false;
    rebuildCrumbs();
    m_stack->setCurrentIndex(0);
}

void BreadcrumbBar::onEditingFinished()
{
    const QString newPath = m_lineEdit->text().trimmed();
    leaveEditMode();
    if (!newPath.isEmpty()) {
        emit pathRequested(newPath);
    }
}

void BreadcrumbBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_editMode)
        enterEditMode();
    QWidget::mouseDoubleClickEvent(event);
}

void BreadcrumbBar::setEditable(bool editable)
{
    if (!editable && m_editMode) leaveEditMode();
}

} // namespace linscp::ui::widgets
