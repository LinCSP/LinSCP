#pragma once
#include <QObject>
#include <QString>

namespace linscp::core::keys {

enum class GenerateKeyType { RSA2048, RSA4096, ED25519, ECDSA256 };

class KeyGenerator : public QObject {
    Q_OBJECT
public:
    explicit KeyGenerator(QObject *parent = nullptr);

    /// Генерирует пару ключей и сохраняет в файлы privateKeyPath / privateKeyPath.pub
    bool generate(GenerateKeyType type,
                  const QString &privateKeyPath,
                  const QString &passphrase = {},
                  const QString &comment    = {});

    QString lastError() const { return m_lastError; }

signals:
    void generated(const QString &privateKeyPath, const QString &publicKeyPath);
    void errorOccurred(const QString &message);

private:
    QString m_lastError;
};

} // namespace linscp::core::keys
