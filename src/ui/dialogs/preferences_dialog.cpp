#include "preferences_dialog.h"
#include "core/app_settings.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QStackedWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QHeaderView>

namespace linscp::ui::dialogs {

using namespace core;

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(720, 520);
    setupUi();
    loadSettings();
}

// ── Построение UI ─────────────────────────────────────────────────────────────

void PreferencesDialog::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(8);

    // Горизонтальный сплиттер: дерево | страница
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);

    // Дерево
    m_tree = new QTreeWidget(splitter);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setFixedWidth(180);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setStyleSheet("QTreeWidget { border-right: 1px solid palette(mid); }");

    // Стек страниц
    m_stack = new QStackedWidget(splitter);
    splitter->addWidget(m_tree);
    splitter->addWidget(m_stack);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    buildPageInterface();
    buildPageTerminal();
    buildPageTransfer();
    buildPageLogging();
    buildTree();

    root->addWidget(splitter, 1);

    // Кнопки
    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
        this);
    root->addWidget(btns);

    connect(btns, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings(); accept();
    });
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(btns->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &PreferencesDialog::onApply);

    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, [this]() { onTreeSelectionChanged(); });
}

void PreferencesDialog::buildTree()
{
    auto *iface = new QTreeWidgetItem(m_tree, { tr("Interface") });
    iface->setData(0, Qt::UserRole, 0);   // index в m_stack
    iface->setExpanded(true);

    auto *integ = new QTreeWidgetItem(m_tree, { tr("Integration") });
    integ->setFlags(integ->flags() & ~Qt::ItemIsSelectable);

    auto *term  = new QTreeWidgetItem(integ, { tr("Terminal") });
    term->setData(0, Qt::UserRole, 1);
    integ->setExpanded(true);

    auto *xfer  = new QTreeWidgetItem(m_tree, { tr("Transfer") });
    xfer->setData(0, Qt::UserRole, 2);

    auto *log   = new QTreeWidgetItem(m_tree, { tr("Logging") });
    log->setData(0, Qt::UserRole, 3);

    m_tree->setCurrentItem(iface);
}

// ── Страница: Логирование ─────────────────────────────────────────────────────

void PreferencesDialog::buildPageLogging()
{
    auto *page = new QWidget;
    auto *vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(12);

    vbox->addWidget(new QLabel(tr("<b>Logging</b>")));

    auto *grp  = new QGroupBox(tr("Session log"), page);
    auto *form = new QFormLayout(grp);

    m_logEnabled = new QCheckBox(tr("Enable session log"), grp);
    form->addRow(m_logEnabled);

    auto *dirRow = new QHBoxLayout;
    m_logDir = new QLineEdit(grp);
    m_logDir->setPlaceholderText(tr("Default: ~/.local/share/linscp/logs"));
    m_logBrowse = new QPushButton(tr("Browse…"), grp);
    m_logBrowse->setFixedWidth(80);
    dirRow->addWidget(m_logDir);
    dirRow->addWidget(m_logBrowse);
    form->addRow(tr("Directory:"), dirRow);

    m_logHint = new QLabel(
        tr("<i>Each session creates a separate file: YYYYMMDD_HHmmss_user@host.log</i>"),
        grp);
    m_logHint->setTextFormat(Qt::RichText);
    m_logHint->setWordWrap(true);
    m_logHint->setStyleSheet("font-size: 11px; color: palette(mid);");
    form->addRow(m_logHint);

    vbox->addWidget(grp);
    vbox->addStretch(1);

    m_stack->addWidget(page);   // index 3

    connect(m_logBrowse, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select log directory"),
            m_logDir->text().isEmpty()
                ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                : m_logDir->text());
        if (!dir.isEmpty())
            m_logDir->setText(dir);
    });
}

// ── Страница: Интерфейс ───────────────────────────────────────────────────────

void PreferencesDialog::buildPageInterface()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(10);

    auto *title = new QLabel(tr("<b>Interface</b>"));
    form->addRow(title);
    form->addRow(new QLabel);   // spacer

    m_langCombo = new QComboBox(page);
    m_langCombo->addItem("English", "en");
    m_langCombo->addItem("Русский", "ru");
    form->addRow(tr("Language:"), m_langCombo);

    auto *restartNote = new QLabel(
        tr("<i>Language change takes effect after restarting LinSCP.</i>"), page);
    restartNote->setWordWrap(true);
    form->addRow(restartNote);

    form->addRow(new QLabel);   // spacer

    m_themeCombo = new QComboBox(page);
    m_themeCombo->addItem(tr("System (follow OS)"),  int(AppSettings::ThemeMode::System));
    m_themeCombo->addItem(tr("Light"),               int(AppSettings::ThemeMode::Light));
    m_themeCombo->addItem(tr("Dark"),                int(AppSettings::ThemeMode::Dark));
    form->addRow(tr("Theme:"), m_themeCombo);

    m_stack->addWidget(page);   // index 0
}

// ── Страница: Интеграция / Терминал ──────────────────────────────────────────

void PreferencesDialog::buildPageTerminal()
{
    auto *page  = new QWidget;
    auto *vbox  = new QVBoxLayout(page);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(12);

    vbox->addWidget(new QLabel(tr("<b>Integration — Terminal emulator</b>")));

    // Выбор терминала
    auto *modeGroup = new QGroupBox(tr("Terminal program"), page);
    auto *modeForm  = new QFormLayout(modeGroup);

    m_termMode = new QComboBox(modeGroup);
    for (const auto &e : AppSettings::terminalEntries())
        m_termMode->addItem(e.displayName, int(e.mode));
    modeForm->addRow(tr("Emulator:"), m_termMode);

    m_detectedLbl = new QLabel(modeGroup);
    m_detectedLbl->setStyleSheet("color: palette(mid); font-size: 11px;");
    modeForm->addRow(tr("Detected:"), m_detectedLbl);

    vbox->addWidget(modeGroup);

    // Кастомный путь + аргументы
    m_customGroup = new QGroupBox(tr("Custom terminal"), page);
    auto *custForm = new QFormLayout(m_customGroup);

    auto *pathRow = new QHBoxLayout;
    m_customPath  = new QLineEdit(m_customGroup);
    m_customPath->setPlaceholderText("/usr/bin/xterm");
    m_browsePath  = new QPushButton(tr("Browse…"), m_customGroup);
    m_browsePath->setFixedWidth(80);
    pathRow->addWidget(m_customPath);
    pathRow->addWidget(m_browsePath);
    custForm->addRow(tr("Path:"), pathRow);

    m_customArgs = new QLineEdit(m_customGroup);
    m_customArgs->setPlaceholderText("ssh -p %port% %user%@%host%");
    custForm->addRow(tr("Arguments:"), m_customArgs);

    m_argsHintLbl = new QLabel(
        tr("Placeholders: <b>%host%</b>  <b>%port%</b>  <b>%user%</b>  <b>%key%</b>"),
        m_customGroup);
    m_argsHintLbl->setTextFormat(Qt::RichText);
    m_argsHintLbl->setStyleSheet("font-size: 11px; color: palette(mid);");
    custForm->addRow(m_argsHintLbl);

    vbox->addWidget(m_customGroup);

    // Опции
    auto *optGroup = new QGroupBox(tr("Options"), page);
    auto *optForm  = new QFormLayout(optGroup);
    m_autoOpen = new QCheckBox(
        tr("Automatically open terminal when connecting"), optGroup);
    optForm->addRow(m_autoOpen);
    vbox->addWidget(optGroup);

    vbox->addStretch(1);

    m_stack->addWidget(page);   // index 1

    connect(m_termMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::onTerminalModeChanged);
    connect(m_browsePath, &QPushButton::clicked,
            this, &PreferencesDialog::onBrowseCustomTerminal);
}

// ── Страница: Передача данных ─────────────────────────────────────────────────

void PreferencesDialog::buildPageTransfer()
{
    auto *page = new QWidget;
    auto *vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(12);

    vbox->addWidget(new QLabel(tr("<b>Transfer</b>")));

    auto *grp  = new QGroupBox(tr("Parallel transfers"), page);
    auto *form = new QFormLayout(grp);
    m_maxConcurrent = new QSpinBox(grp);
    m_maxConcurrent->setRange(1, 10);
    m_maxConcurrent->setSuffix(tr(" files"));
    form->addRow(tr("Max simultaneous:"), m_maxConcurrent);
    vbox->addWidget(grp);
    vbox->addStretch(1);

    m_stack->addWidget(page);   // index 2
}

// ── Слоты ─────────────────────────────────────────────────────────────────────

void PreferencesDialog::onTreeSelectionChanged()
{
    auto *item = m_tree->currentItem();
    if (!item) return;
    const int idx = item->data(0, Qt::UserRole).toInt();
    m_stack->setCurrentIndex(idx);
}

void PreferencesDialog::onTerminalModeChanged(int /*comboIndex*/)
{
    const auto mode = static_cast<AppSettings::TerminalMode>(
        m_termMode->currentData().toInt());
    const bool isCustom = (mode == AppSettings::TerminalMode::Custom);
    m_customGroup->setEnabled(isCustom);

    // Показать, что сейчас найдено системой
    if (mode == AppSettings::TerminalMode::AutoDetect) {
        // Проверяем по порядку
        const QStringList candidates = {
            "putty","kitty","xterm","konsole",
            "gnome-terminal","xfce4-terminal","alacritty"
        };
        QString found;
        for (const auto &c : candidates) {
            if (!QStandardPaths::findExecutable(c).isEmpty()) { found = c; break; }
        }
        m_detectedLbl->setText(found.isEmpty()
            ? tr("none found — install a terminal emulator")
            : found + "  (" + QStandardPaths::findExecutable(found) + ")");
    } else if (!isCustom) {
        const QString bin = AppSettings::terminalBinaryForMode(mode);
        const QString path = QStandardPaths::findExecutable(bin);
        m_detectedLbl->setText(path.isEmpty()
            ? tr("%1 not found in PATH").arg(bin)
            : path);
    } else {
        m_detectedLbl->setText(tr("—"));
    }
}

void PreferencesDialog::onBrowseCustomTerminal()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select terminal executable"),
        "/usr/bin", tr("Executables (*)"));
    if (!path.isEmpty())
        m_customPath->setText(path);
}

void PreferencesDialog::onApply()
{
    saveSettings();
}

// ── Load / Save ───────────────────────────────────────────────────────────────

void PreferencesDialog::loadSettings()
{
    // Interface
    const QString lang = AppSettings::language();
    for (int i = 0; i < m_langCombo->count(); ++i) {
        if (m_langCombo->itemData(i).toString() == lang ||
            (lang.isEmpty() && m_langCombo->itemData(i).toString() == "en")) {
            m_langCombo->setCurrentIndex(i);
            break;
        }
    }

    const int themeInt = int(AppSettings::theme());
    for (int i = 0; i < m_themeCombo->count(); ++i) {
        if (m_themeCombo->itemData(i).toInt() == themeInt) {
            m_themeCombo->setCurrentIndex(i);
            break;
        }
    }

    // Terminal
    const int modeInt = int(AppSettings::terminalMode());
    for (int i = 0; i < m_termMode->count(); ++i) {
        if (m_termMode->itemData(i).toInt() == modeInt) {
            m_termMode->setCurrentIndex(i);
            break;
        }
    }
    m_customPath->setText(AppSettings::terminalCustomPath());
    m_customArgs->setText(AppSettings::terminalCustomArgs());
    m_autoOpen->setChecked(AppSettings::terminalAutoOpen());
    onTerminalModeChanged(m_termMode->currentIndex());

    // Transfer
    m_maxConcurrent->setValue(AppSettings::maxConcurrentTransfers());

    // Logging
    m_logEnabled->setChecked(AppSettings::sessionLogEnabled());
    m_logDir->setText(AppSettings::sessionLogDir());
}

void PreferencesDialog::saveSettings()
{
    // Interface
    AppSettings::setLanguage(m_langCombo->currentData().toString());
    AppSettings::setTheme(
        static_cast<AppSettings::ThemeMode>(m_themeCombo->currentData().toInt()));
    AppSettings::applyTheme();

    // Terminal
    AppSettings::setTerminalMode(
        static_cast<AppSettings::TerminalMode>(m_termMode->currentData().toInt()));
    AppSettings::setTerminalCustomPath(m_customPath->text());
    AppSettings::setTerminalCustomArgs(m_customArgs->text().trimmed().isEmpty()
        ? "ssh -p %port% %user%@%host%"
        : m_customArgs->text());
    AppSettings::setTerminalAutoOpen(m_autoOpen->isChecked());

    // Transfer
    AppSettings::setMaxConcurrentTransfers(m_maxConcurrent->value());

    // Logging
    AppSettings::setSessionLogEnabled(m_logEnabled->isChecked());
    AppSettings::setSessionLogDir(m_logDir->text().trimmed());
}

} // namespace linscp::ui::dialogs
