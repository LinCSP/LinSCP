#pragma once
#include <QDialog>

class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QGroupBox;
class QPushButton;
class QLabel;

namespace linscp::ui::dialogs {

/// Диалог настроек — аналог WinSCP TPreferencesDialog.
///
///   ┌──────────────┬──────────────────────────────────────────────┐
///   │ Интерфейс    │  [заголовок страницы]                        │
///   │ Интеграция   │                                              │
///   │  └ Терминал  │  [содержимое страницы]                       │
///   │ Передача     │                                              │
///   ├──────────────┴──────────────────────────────────────────────┤
///   │               [OK]  [Отмена]  [Применить]                  │
///   └─────────────────────────────────────────────────────────────┘
class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget *parent = nullptr);

private slots:
    void onTreeSelectionChanged();
    void onTerminalModeChanged(int index);
    void onBrowseCustomTerminal();
    void onApply();

private:
    void setupUi();
    void buildTree();
    void buildPageInterface();
    void buildPageTerminal();
    void buildPageTransfer();

    void loadSettings();
    void saveSettings();

    // ── Навигация ──────────────────────────────────────────────────────────────
    QTreeWidget    *m_tree   = nullptr;
    QStackedWidget *m_stack  = nullptr;

    // ── Страница: Интерфейс ───────────────────────────────────────────────────
    QComboBox *m_langCombo   = nullptr;
    QComboBox *m_themeCombo  = nullptr;

    // ── Страница: Интеграция / Терминал ──────────────────────────────────────
    QComboBox   *m_termMode      = nullptr;
    QGroupBox   *m_customGroup   = nullptr;
    QLineEdit   *m_customPath    = nullptr;
    QPushButton *m_browsePath    = nullptr;
    QLineEdit   *m_customArgs    = nullptr;
    QLabel      *m_argsHintLbl   = nullptr;
    QCheckBox   *m_autoOpen      = nullptr;
    QLabel      *m_detectedLbl   = nullptr;

    // ── Страница: Передача данных ─────────────────────────────────────────────
    QSpinBox    *m_maxConcurrent = nullptr;
};

} // namespace linscp::ui::dialogs
