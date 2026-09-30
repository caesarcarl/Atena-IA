#include "ProvidersPage.h"

#include "../AssetCatalog.h"
#include "../client/AtenaClientFacade.h"
#include "../dialogs/ProviderSetupDialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

namespace AtenaUi {

ProvidersPage::ProvidersPage(AtenaClientFacade *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
    auto *page = new QVBoxLayout(this);
    page->setContentsMargins(0, 0, 0, 0);
    page->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *host = new QWidget;
    auto *outer = new QVBoxLayout(host);
    outer->setContentsMargins(28, 24, 28, 24);
    outer->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("Serviços de IA"), host);
    title->setObjectName(QStringLiteral("PageTitle"));
    auto *description = new QLabel(
        QStringLiteral("Configure os serviços que a Atena pode usar. O processamento local continua disponível pelo Ollama; serviços online só são usados quando configurados."),
        host);
    description->setObjectName(QStringLiteral("Muted"));
    description->setWordWrap(true);

    auto *actions = new QHBoxLayout;
    auto *refresh = new QPushButton(assetIcon(QStringLiteral(":/icons/navigation/providers.png")),
                                    QStringLiteral("Atualizar"), host);
    refresh->setAccessibleName(QStringLiteral("Atualizar serviços de IA"));
    actions->addWidget(refresh);
    actions->addStretch();

    outer->addWidget(title);
    outer->addWidget(description);
    outer->addLayout(actions);

    QList<QPushButton *> coreButtons;

    auto addProviderCard = [this, host, outer, &coreButtons](const QString &id,
                                                             const QString &label,
                                                             const QString &descriptionText,
                                                             const QString &iconResource,
                                                             bool primary) {
        auto *card = new QFrame(host);
        card->setObjectName(primary ? QStringLiteral("Card") : QStringLiteral("SubtleCard"));
        card->setMinimumHeight(96);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(18, 14, 18, 14);
        row->setSpacing(14);

        auto *icon = new QLabel(card);
        icon->setPixmap(assetPixmap(iconResource).scaled(42, 42, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        icon->setFixedSize(48, 48);
        icon->setAlignment(Qt::AlignCenter);

        auto *text = new QVBoxLayout;
        auto *providerTitle = new QLabel(label, card);
        providerTitle->setObjectName(QStringLiteral("SectionTitle"));
        auto *providerDescription = new QLabel(descriptionText, card);
        providerDescription->setObjectName(QStringLiteral("Muted"));
        providerDescription->setWordWrap(true);
        auto *status = new QLabel(QStringLiteral("Aguardando conexão com o Core"), card);
        status->setObjectName(QStringLiteral("Muted"));
        status->setWordWrap(true);
        m_statusLabels.insert(id, status);
        text->addWidget(providerTitle);
        text->addWidget(providerDescription);
        text->addWidget(status);

        auto *buttons = new QVBoxLayout;
        buttons->setSpacing(6);
        auto *test = new QPushButton(QStringLiteral("Testar"), card);
        auto *configure = new QPushButton(assetIcon(QStringLiteral(":/icons/navigation/settings.png")),
                                          QStringLiteral("Configurar"), card);
        if (primary) configure->setObjectName(QStringLiteral("PrimaryButton"));
        test->setEnabled(false);
        configure->setEnabled(false);
        test->setMinimumWidth(108);
        configure->setMinimumWidth(108);
        coreButtons << test << configure;
        buttons->addWidget(test);
        buttons->addWidget(configure);

        row->addWidget(icon, 0, Qt::AlignTop);
        row->addLayout(text, 1);
        row->addLayout(buttons);
        outer->addWidget(card);

        connect(configure, &QPushButton::clicked, this, [this, id, label] { openSetup(id, label); });
        connect(test, &QPushButton::clicked, this, [this, id] {
            if (m_statusLabels.contains(id)) m_statusLabels.value(id)->setText(QStringLiteral("Testando conexão…"));
            m_client->testProvider(id);
        });
    };

    addProviderCard(QStringLiteral("ollama"), QStringLiteral("Ollama"),
                    QStringLiteral("IA local ou em outro computador da sua rede"),
                    QStringLiteral(":/icons/quick-actions/mode-local.png"), true);

    auto *cloudTitle = new QLabel(QStringLiteral("APIs e gateways compatíveis"), host);
    cloudTitle->setObjectName(QStringLiteral("SectionTitle"));
    outer->addWidget(cloudTitle);

    addProviderCard(QStringLiteral("openai"), QStringLiteral("OpenAI"),
                    QStringLiteral("Serviço online configurado por API"),
                    QStringLiteral(":/icons/quick-actions/mode-cloud.png"), false);
    addProviderCard(QStringLiteral("groq"), QStringLiteral("Groq"),
                    QStringLiteral("Inferência online compatível com o protocolo OpenAI"),
                    QStringLiteral(":/icons/quick-actions/mode-cloud.png"), false);
    addProviderCard(QStringLiteral("deepseek"), QStringLiteral("DeepSeek"),
                    QStringLiteral("Serviço online compatível com o protocolo OpenAI"),
                    QStringLiteral(":/icons/quick-actions/mode-cloud.png"), false);
    addProviderCard(QStringLiteral("xai"), QStringLiteral("xAI"),
                    QStringLiteral("Serviço online compatível com o protocolo OpenAI"),
                    QStringLiteral(":/icons/quick-actions/mode-cloud.png"), false);
    addProviderCard(QStringLiteral("openai_compatible"), QStringLiteral("OpenAI-compatible"),
                    QStringLiteral("Servidor, proxy ou gateway personalizado"),
                    QStringLiteral(":/icons/quick-actions/mode-hybrid.png"), false);

    auto *note = new QLabel(
        QStringLiteral("Outros providers com contratos próprios serão adicionados como integrações nativas em etapas posteriores."),
        host);
    note->setObjectName(QStringLiteral("Muted"));
    note->setWordWrap(true);
    outer->addWidget(note);
    outer->addStretch();

    scroll->setWidget(host);
    page->addWidget(scroll);

    connect(refresh, &QPushButton::clicked, m_client, &AtenaClientFacade::requestProviders);
    connect(m_client, &AtenaClientFacade::providersReady, this, &ProvidersPage::rebuild);
    connect(m_client, &AtenaClientFacade::providerTestFinished, this,
            [this](const QString &providerId, bool ok, const QString &message) {
                if (!m_statusLabels.contains(providerId)) return;
                auto *label = m_statusLabels.value(providerId);
                label->setObjectName(ok ? QStringLiteral("SuccessText") : QStringLiteral("WarningText"));
                label->setText(ok ? QStringLiteral("Conectado · %1").arg(message)
                                  : QStringLiteral("Não foi possível conectar · %1").arg(message));
                label->style()->unpolish(label);
                label->style()->polish(label);
            });

    connect(m_client, &AtenaClientFacade::connectionStateChanged, this,
            [this, refresh, coreButtons](AtenaClientFacade::ConnectionState state, const QString &detail) {
                const bool connected = state == AtenaClientFacade::ConnectionState::Connected;
                refresh->setEnabled(connected);
                for (auto *button : coreButtons) button->setEnabled(connected);
                for (auto it = m_statusLabels.begin(); it != m_statusLabels.end(); ++it) {
                    auto *label = it.value();
                    if (connected) {
                        label->setObjectName(QStringLiteral("Muted"));
                        label->setText(QStringLiteral("Core conectado · aguardando consulta"));
                    } else if (state == AtenaClientFacade::ConnectionState::Connecting) {
                        label->setObjectName(QStringLiteral("Muted"));
                        label->setText(QStringLiteral("Conectando ao Core…"));
                    } else {
                        label->setObjectName(QStringLiteral("WarningText"));
                        label->setText(QStringLiteral("Core offline · %1").arg(detail));
                    }
                    label->style()->unpolish(label);
                    label->style()->polish(label);
                }
                if (connected) m_client->requestProviders();
            });
}

void ProvidersPage::rebuild(const QVariantList &providers)
{
    for (auto it = m_statusLabels.begin(); it != m_statusLabels.end(); ++it) {
        it.value()->setObjectName(QStringLiteral("Muted"));
        it.value()->setText(QStringLiteral("Não configurado nesta sessão"));
    }

    for (const QVariant &entry : providers) {
        const QVariantMap provider = entry.toMap();
        const QString id = provider.value(QStringLiteral("id"),
                           provider.value(QStringLiteral("preset_id"),
                           provider.value(QStringLiteral("type")))).toString();
        if (!m_statusLabels.contains(id)) continue;
        const bool configured = provider.value(QStringLiteral("configured"),
                                provider.value(QStringLiteral("enabled"))).toBool();
        const QString model = provider.value(QStringLiteral("model")).toString();
        auto *status = m_statusLabels.value(id);
        status->setText(configured
            ? (model.isEmpty() ? QStringLiteral("Configurado · selecione um modelo")
                               : QStringLiteral("Configurado · modelo %1").arg(model))
            : QStringLiteral("Requer configuração"));
        status->style()->unpolish(status);
        status->style()->polish(status);
    }
}

void ProvidersPage::openSetup(const QString &presetId, const QString &label)
{
    ProviderSetupDialog dialog(presetId, label, this);
    if (dialog.exec() != QDialog::Accepted) return;
    QVariantMap values = dialog.takeConfiguration();
    if (m_statusLabels.contains(presetId))
        m_statusLabels.value(presetId)->setText(QStringLiteral("Salvando e testando configuração…"));
    m_client->configureProvider(presetId, values);
    values.remove(QStringLiteral("secret_value"));
}

} // namespace AtenaUi
