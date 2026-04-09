#pragma once
#include <QDialog>
#include "core/session/session_profile.h"

class QTreeWidget;
class QStackedWidget;
class QTreeWidgetItem;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;

namespace linscp::ui::dialogs {

/// Диалог расширенных настроек сессии — аналог «Расширенные настройки соединения» в WinSCP.
/// Слева: дерево категорий. Справа: страница настроек выбранной категории.
///
/// Категории:
///   Среда
///     Каталоги
///     Корзина
///     SCP/Оболочка
///   Соединение
///     Прокси
///     Туннель
///   SSH
///     Аутентификация
///   Заметка
class AdvancedSessionDialog : public QDialog {
    Q_OBJECT
public:
    explicit AdvancedSessionDialog(const core::session::SessionProfile &profile,
                                   QWidget *parent = nullptr);

    /// Возвращает профиль с применёнными расширенными настройками
    core::session::SessionProfile applyTo(core::session::SessionProfile base) const;

private slots:
    void onCategoryChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
    void setupUi();
    void populate(const core::session::SessionProfile &p);

    QTreeWidget    *m_tree;
    QStackedWidget *m_stack;

    // ── Страница: Каталоги ────────────────────────────────────────────────────
    QLineEdit   *m_remoteDir;
    QLineEdit   *m_localDir;
    QCheckBox   *m_syncBrowsing;

    // ── Страница: Корзина ─────────────────────────────────────────────────────
    QCheckBox   *m_deleteToRecycleBin;
    QLineEdit   *m_recycleBinPath;

    // ── Страница: SCP/Оболочка ────────────────────────────────────────────────
    QLineEdit   *m_shell;
    QLineEdit   *m_listingCommand;
    QCheckBox   *m_clearAliases;
    QCheckBox   *m_ignoreLsWarnings;
    QComboBox   *m_eolType;

    // ── Страница: Среда (общая) ───────────────────────────────────────────────
    QComboBox   *m_dstMode;
    QCheckBox   *m_utf8FileNames;
    QSpinBox    *m_timezoneOffset;  ///< минуты

    // ── Страница: Прокси ─────────────────────────────────────────────────────
    QComboBox   *m_proxyMethod;
    QLineEdit   *m_proxyHost;
    QSpinBox    *m_proxyPort;
    QLineEdit   *m_proxyUser;
    QLineEdit   *m_proxyCommand;
    QCheckBox   *m_proxyDns;

    // ── Страница: Туннель ─────────────────────────────────────────────────────
    QCheckBox   *m_tunnelEnabled;
    QLineEdit   *m_tunnelHost;
    QSpinBox    *m_tunnelPort;
    QLineEdit   *m_tunnelUser;
    QComboBox   *m_tunnelAuth;
    QLineEdit   *m_tunnelKeyPath;
    QPushButton *m_tunnelBrowseKey;

    // ── Страница: SSH/Аутентификация ──────────────────────────────────────────
    QCheckBox   *m_compression;
    QSpinBox    *m_keepalive;   ///< секунды, 0 = отключён
    QSpinBox    *m_connectTimeout;

    // ── Страница: Заметка ─────────────────────────────────────────────────────
    // QPlainTextEdit используется напрямую через m_stack
};

} // namespace linscp::ui::dialogs
