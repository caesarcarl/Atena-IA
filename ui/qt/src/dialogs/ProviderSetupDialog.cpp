#include "ProviderSetupDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace AtenaUi {

ProviderSetupDialog::ProviderSetupDialog(const QString &presetId,
                                         const QString &displayName,
                                         QWidget *parent)
    : QDialog(parent),
      m_presetId(presetId),
      m_endpoint(new QLineEdit(this)),
      m_apiKey(new QLineEdit(this)),
      m_model(new QLineEdit(this))
{
    setWindowTitle(QStringLiteral("Configurar %1").arg(displayName));
    setMinimumWidth(500);

    auto *layout = new QVBoxLayout(this);

    auto *info = new QLabel(
        QStringLiteral("A interface entrega a configuração ao Atena Core. "
                       "No Ollama local nenhuma chave é necessária. Chaves de API são armazenadas no cofre seguro do sistema (Windows Credential Manager ou keyring/libsecret no Linux) e nunca no SQLite/QSettings."), this);
    info->setWordWrap(true);
    info->setObjectName(QStringLiteral("Muted"));
    layout->addWidget(info);

    auto *form = new QFormLayout();

    m_endpoint->setAccessibleName(QStringLiteral("Endpoint do provider"));
    m_apiKey->setAccessibleName(QStringLiteral("Chave ou token de API"));
    m_apiKey->setEchoMode(QLineEdit::Password);
    m_model->setAccessibleName(QStringLiteral("Modelo padrão"));

    if (presetId == QStringLiteral("ollama")) {
        m_endpoint->setText(QStringLiteral("http://127.0.0.1:11434"));
        m_endpoint->setPlaceholderText(QStringLiteral("http://127.0.0.1:11434"));
        m_apiKey->setPlaceholderText(QStringLiteral("Opcional para Ollama remoto/proxy"));
        m_model->setPlaceholderText(QStringLiteral("Ex.: qwen3:0.6b"));
    } else if (presetId == QStringLiteral("openai")) {
        m_endpoint->setText(QStringLiteral("https://api.openai.com/v1"));
        m_apiKey->setPlaceholderText(QStringLiteral("Chave da OpenAI"));
        m_model->setPlaceholderText(QStringLiteral("Modelo disponível na sua conta"));
    } else if (presetId == QStringLiteral("groq")) {
        m_endpoint->setText(QStringLiteral("https://api.groq.com/openai/v1"));
        m_apiKey->setPlaceholderText(QStringLiteral("Chave da Groq"));
        m_model->setPlaceholderText(QStringLiteral("Modelo disponível na sua conta"));
    } else if (presetId == QStringLiteral("deepseek")) {
        m_endpoint->setText(QStringLiteral("https://api.deepseek.com/v1"));
        m_apiKey->setPlaceholderText(QStringLiteral("Chave da DeepSeek"));
        m_model->setPlaceholderText(QStringLiteral("Modelo disponível na sua conta"));
    } else if (presetId == QStringLiteral("xai")) {
        m_endpoint->setText(QStringLiteral("https://api.x.ai/v1"));
        m_apiKey->setPlaceholderText(QStringLiteral("Chave da xAI"));
        m_model->setPlaceholderText(QStringLiteral("Modelo disponível na sua conta"));
    } else if (presetId == QStringLiteral("openai_compatible")) {
        m_endpoint->setPlaceholderText(QStringLiteral("https://servidor/v1"));
        m_apiKey->setPlaceholderText(QStringLiteral("Chave ou token; opcional em gateways locais"));
        m_model->setPlaceholderText(QStringLiteral("Modelo exposto pelo servidor"));
    } else {
        m_endpoint->setPlaceholderText(QStringLiteral("Endpoint do provider"));
        m_apiKey->setPlaceholderText(QStringLiteral("Chave de API"));
    }

    form->addRow(QStringLiteral("Endpoint"), m_endpoint);
    form->addRow(QStringLiteral("Chave / token"), m_apiKey);
    form->addRow(QStringLiteral("Modelo padrão"), m_model);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        m_configuration.insert(QStringLiteral("preset_id"), m_presetId);

        const QString endpoint = m_endpoint->text().trimmed();
        const QString secret = m_apiKey->text();
        const QString model = m_model->text().trimmed();

        if (!endpoint.isEmpty()) {
            m_configuration.insert(QStringLiteral("endpoint"), endpoint);
        }
        if (!secret.isEmpty()) {
            m_configuration.insert(QStringLiteral("secret_value"), secret);
        }
        if (!model.isEmpty()) {
            m_configuration.insert(QStringLiteral("model"), model);
        }

        m_apiKey->clear();
        accept();
    });
}

QVariantMap ProviderSetupDialog::takeConfiguration()
{
    QVariantMap result = m_configuration;
    m_configuration.clear();
    return result;
}

} // namespace AtenaUi
