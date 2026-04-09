#pragma once
#include <QDialog>

class QListWidget;
class QLabel;
class QPushButton;
class QStackedWidget;

namespace linscp::core::keys { class KeyManager; class KeyGenerator; }

namespace linscp::ui::dialogs {

/// Менеджер SSH-ключей:
/// - список ключей из ~/.ssh + добавленных вручную
/// - генерация новой пары
/// - конвертация .ppk ↔ OpenSSH
class KeyDialog : public QDialog {
    Q_OBJECT
public:
    explicit KeyDialog(core::keys::KeyManager   *keyMgr,
                       core::keys::KeyGenerator *keyGen,
                       QWidget *parent = nullptr);

private slots:
    void onSelectionChanged();
    void onAddKey();
    void onGenerateKey();
    void onConvertKey();
    void onRemoveKey();
    void refreshList();

private:
    void setupUi();

    core::keys::KeyManager   *m_keyMgr;
    core::keys::KeyGenerator *m_keyGen;

    QListWidget *m_list;
    QLabel      *m_fingerprintLabel;
    QLabel      *m_typeLabel;
    QPushButton *m_addBtn;
    QPushButton *m_genBtn;
    QPushButton *m_convertBtn;
    QPushButton *m_removeBtn;
};

} // namespace linscp::ui::dialogs
