#include "ToolsPage.h"
#include "../AssetCatalog.h"
#include "../client/AtenaClientFacade.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace AtenaUi {

ToolsPage::ToolsPage(AtenaClientFacade *client, QWidget *parent)
    : QWidget(parent), m_client(client), m_list(nullptr), m_state(new QLabel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Ferramentas"), this);
    title->setObjectName(QStringLiteral("PageTitle"));
    auto *desc = new QLabel(
        QStringLiteral("A interface mostra somente ferramentas anunciadas pelo Atena Core. A UI não executa comandos de shell diretamente."),
        this);
    desc->setWordWrap(true);
    desc->setObjectName(QStringLiteral("Muted"));

    auto *refresh = new QPushButton(assetIcon(QStringLiteral(":/icons/navigation/tools.png")),
                                    QStringLiteral("Atualizar ferramentas"), this);
    refresh->setEnabled(false);

    m_state->setText(QStringLiteral("Aguardando conexão com o Core."));
    m_state->setObjectName(QStringLiteral("Muted"));
    m_state->setWordWrap(true);

    layout->addWidget(title);
    layout->addWidget(desc);
    layout->addWidget(refresh, 0, Qt::AlignLeft);
    layout->addWidget(m_state);

    auto *host = new QWidget(this);
    m_list = new QVBoxLayout(host);
    m_list->setContentsMargins(0, 0, 0, 0);
    m_list->setSpacing(10);
    m_list->addStretch();
    layout->addWidget(host, 1);

    connect(refresh, &QPushButton::clicked, m_client, &AtenaClientFacade::requestTools);
    connect(m_client, &AtenaClientFacade::toolsReady, this, &ToolsPage::rebuild);
    connect(m_client, &AtenaClientFacade::connectionStateChanged, this,
            [this, refresh](AtenaClientFacade::ConnectionState state, const QString &detail) {
                const bool connected = state == AtenaClientFacade::ConnectionState::Connected;
                refresh->setEnabled(connected);
                if (connected) {
                    m_state->setText(QStringLiteral("Core conectado · consultando capabilities reais…"));
                    m_client->requestTools();
                } else if (state == AtenaClientFacade::ConnectionState::Connecting) {
                    m_state->setText(QStringLiteral("Conectando ao Core…"));
                } else {
                    rebuild({});
                    m_state->setText(QStringLiteral("Ferramentas indisponíveis enquanto o Core estiver offline. %1").arg(detail));
                }
            });
}

void ToolsPage::rebuild(const QVariantList &tools)
{
    while (m_list->count() > 1) {
        auto *item = m_list->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (tools.isEmpty()) {
        m_state->setText(QStringLiteral("Nenhuma ferramenta foi anunciada pelo Core."));
        return;
    }

    m_state->setText(QStringLiteral("%1 ferramenta(s) disponível(is).").arg(tools.size()));
    for (const QVariant &entry : tools) {
        const QVariantMap tool = entry.toMap();
        const QString name = tool.value(QStringLiteral("name")).toString();
        const QString risk = tool.value(QStringLiteral("risk")).toString();

        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("Card"));
        card->setMinimumHeight(68);
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(16, 12, 16, 12);

        auto *icon = new QLabel(card);
        icon->setPixmap(assetPixmap(QStringLiteral(":/icons/navigation/tools.png"))
                            .scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        auto *text = new QVBoxLayout();
        auto *nameLabel = new QLabel(name, card);
        nameLabel->setObjectName(QStringLiteral("SectionTitle"));
        auto *riskLabel = new QLabel(
            risk == QStringLiteral("read") ? QStringLiteral("Leitura · risco baixo")
                                           : QStringLiteral("Risco informado pelo Core: %1").arg(risk),
            card);
        riskLabel->setObjectName(QStringLiteral("Muted"));
        text->addWidget(nameLabel);
        text->addWidget(riskLabel);

        row->addWidget(icon);
        row->addLayout(text, 1);
        m_list->insertWidget(m_list->count() - 1, card);
    }
}

} // namespace AtenaUi
