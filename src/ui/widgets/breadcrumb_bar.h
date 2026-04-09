#pragma once
#include <QWidget>
#include <QStringList>

class QHBoxLayout;
class QLineEdit;
class QToolButton;
class QStackedWidget;

namespace linscp::ui::widgets {

/// Виджет навигации по пути: «/home / user / documents» с кликабельными сегментами.
/// По двойному клику переходит в режим ручного ввода пути.
class BreadcrumbBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)

public:
    explicit BreadcrumbBar(QWidget *parent = nullptr);

    QString path() const { return m_path; }
    void setPath(const QString &path);

    void setEditable(bool editable);

signals:
    void pathChanged(const QString &path);
    void pathRequested(const QString &path);  ///< пользователь нажал Enter или кнопку

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onEditingFinished();
    void onSegmentClicked();

private:
    void rebuildCrumbs();
    void enterEditMode();
    void leaveEditMode();

    QString          m_path;
    QStackedWidget  *m_stack;       ///< 0=crumbs, 1=lineedit
    QWidget         *m_crumbWidget;
    QHBoxLayout     *m_crumbLayout;
    QLineEdit       *m_lineEdit;
    bool             m_editMode = false;
};

} // namespace linscp::ui::widgets
