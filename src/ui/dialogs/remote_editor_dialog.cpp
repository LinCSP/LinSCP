#include "remote_editor_dialog.h"
#include "core/i_remote_file_system.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTextStream>
#include <QFont>
#include <QFontMetrics>
#include <QFontDatabase>
#include <QProgressDialog>
#include <QApplication>
#include <QCloseEvent>

namespace linscp::ui::dialogs {

static constexpr qint64 kMaxFileSize = 4 * 1024 * 1024; // 4 MB

RemoteEditorDialog::RemoteEditorDialog(core::IRemoteFileSystem *fs,
                                       const QString &remotePath,
                                       QWidget *parent)
    : QDialog(parent)
    , m_fs(fs)
    , m_remotePath(remotePath)
{
    setWindowTitle(tr("Edit: %1").arg(QFileInfo(remotePath).fileName()));
    resize(800, 600);
    setMinimumSize(400, 300);

    // ── Editor ────────────────────────────────────────────────────────────────
    m_editor = new QPlainTextEdit(this);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);

    // Моноширинный шрифт
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(10);
    m_editor->setFont(font);

    // Подсветить текущую строку
    auto highlightCurrentLine = [this]() {
        QList<QTextEdit::ExtraSelection> sels;
        if (!m_editor->isReadOnly()) {
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(
                m_editor->palette().color(QPalette::AlternateBase));
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            sel.cursor = m_editor->textCursor();
            sel.cursor.clearSelection();
            sels << sel;
        }
        m_editor->setExtraSelections(sels);
    };
    connect(m_editor, &QPlainTextEdit::cursorPositionChanged,
            this, highlightCurrentLine);

    connect(m_editor, &QPlainTextEdit::modificationChanged, this, [this](bool mod) {
        m_modified = mod;
        m_saveBtn->setEnabled(mod);
    });

    // ── Status bar ────────────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("Loading…"), this);
    m_statusLabel->setStyleSheet("color: gray; font-size: 11px;");

    // ── Buttons ───────────────────────────────────────────────────────────────
    m_saveBtn = new QPushButton(tr("Save"), this);
    m_saveBtn->setEnabled(false);
    m_saveBtn->setShortcut(QKeySequence::Save);
    connect(m_saveBtn, &QPushButton::clicked, this, &RemoteEditorDialog::onSave);

    auto *saveCloseBtn = new QPushButton(tr("Save && Close"), this);
    connect(saveCloseBtn, &QPushButton::clicked, this, &RemoteEditorDialog::onSaveAndClose);

    auto *closeBtn = new QPushButton(tr("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(m_statusLabel, 1);
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addWidget(saveCloseBtn);
    btnLayout->addWidget(closeBtn);

    // ── Main layout ───────────────────────────────────────────────────────────
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->addWidget(m_editor, 1);
    mainLayout->addLayout(btnLayout);

    // Загружаем файл после отображения диалога
    QMetaObject::invokeMethod(this, &RemoteEditorDialog::loadFile, Qt::QueuedConnection);
}

// ── Загрузка ──────────────────────────────────────────────────────────────────

void RemoteEditorDialog::loadFile()
{
    // Проверяем размер файла через stat
    const auto info = m_fs->stat(m_remotePath);
    if (info.size > kMaxFileSize) {
        m_statusLabel->setText(
            tr("File too large (%1 MB) — max 4 MB")
                .arg(info.size / 1024 / 1024));
        m_editor->setReadOnly(true);
        m_editor->setPlaceholderText(tr("File is too large to edit in the built-in editor."));
        return;
    }

    // Скачать во временный файл
    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        m_statusLabel->setText(tr("Cannot create temp file"));
        return;
    }
    const QString tmpPath = tmp.fileName();
    tmp.close();

    m_statusLabel->setText(tr("Downloading…"));
    qApp->processEvents();

    if (!m_fs->download(m_remotePath, tmpPath)) {
        m_statusLabel->setText(tr("Download failed: %1").arg(m_fs->lastError()));
        return;
    }

    // Читаем содержимое
    QFile f(tmpPath);
    if (!f.open(QIODevice::ReadOnly)) {
        m_statusLabel->setText(tr("Cannot read temp file"));
        QFile::remove(tmpPath);
        return;
    }

    const QByteArray raw = f.readAll();
    f.close();
    QFile::remove(tmpPath);

    // Определяем кодировку (UTF-8 с BOM или без; fallback Latin-1)
    QTextStream ts(raw);
    const QString text = ts.readAll();

    m_editor->setPlainText(text);
    m_editor->document()->setModified(false);
    m_modified = false;
    m_saveBtn->setEnabled(false);

    const QString fname = QFileInfo(m_remotePath).fileName();
    m_statusLabel->setText(
        tr("%1  |  %2 lines  |  %3 bytes")
            .arg(fname)
            .arg(m_editor->document()->blockCount())
            .arg(raw.size()));
}

// ── Сохранение ────────────────────────────────────────────────────────────────

void RemoteEditorDialog::onSave()
{
    if (!m_modified) return;

    // Записываем во временный файл
    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        QMessageBox::critical(this, tr("Save"), tr("Cannot create temp file."));
        return;
    }
    const QString tmpPath = tmp.fileName();

    QTextStream ts(&tmp);
    ts << m_editor->toPlainText();
    tmp.close();

    m_statusLabel->setText(tr("Uploading…"));
    qApp->processEvents();

    if (!m_fs->upload(tmpPath, m_remotePath)) {
        m_statusLabel->setText(tr("Upload failed: %1").arg(m_fs->lastError()));
        QFile::remove(tmpPath);
        return;
    }

    QFile::remove(tmpPath);

    m_editor->document()->setModified(false);
    m_modified = false;
    m_saveBtn->setEnabled(false);
    m_statusLabel->setText(tr("Saved."));
}

void RemoteEditorDialog::onSaveAndClose()
{
    onSave();
    if (!m_modified)
        accept();
}

} // namespace linscp::ui::dialogs
