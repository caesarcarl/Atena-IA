#include "ModelsPage.h"
#include "../AssetCatalog.h"
#include "../client/AtenaClientFacade.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace AtenaUi {

ModelsPage::ModelsPage(AtenaClientFacade *client, QWidget *parent)
    : QWidget(parent), m_client(client), m_state(new QLabel(this))
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(28, 24, 28, 24);
    outer->setSpacing(12);

    auto *header = new QHBoxLayout();
    header->setSpacing(18);

    auto *headerText = new QVBoxLayout();
    auto *title = new QLabel(QStringLiteral("Modelos"), this);
    title->setObjectName(QStringLiteral("PageTitle"));

    auto *desc = new QLabel(
        QStringLiteral("Liste, selecione, baixe e remova modelos do Ollama pelo Atena Core."), this);
    desc->setObjectName(QStringLiteral("Muted"));
    desc->setWordWrap(true);

    auto *actions = new QHBoxLayout();
    auto *refresh = new QPushButton(
        assetIcon(QStringLiteral(":/icons/navigation/models.png")),
        QStringLiteral("Atualizar"), this);
    refresh->setAccessibleName(QStringLiteral("Atualizar lista de modelos"));

    auto *pull = new QPushButton(
        assetIcon(QStringLiteral(":/icons/quick-actions/mode-local.png")),
        QStringLiteral("Baixar modelo"), this);
    pull->setObjectName(QStringLiteral("PrimaryButton"));
    pull->setAccessibleName(QStringLiteral("Baixar modelo do Ollama"));

    refresh->setEnabled(false);
    pull->setEnabled(false);
    actions->addWidget(refresh);
    actions->addWidget(pull);
    actions->addStretch();

    headerText->addWidget(title);
    headerText->addWidget(desc);
    headerText->addLayout(actions);
    headerText->addStretch();

    auto *illustration = new QLabel(this);
    illustration->setPixmap(assetPixmap(QStringLiteral(":/illustrations/models-providers.png"))
                                .scaled(360, 205, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    illustration->setAlignment(Qt::AlignCenter);
    illustration->setAccessibleName(QStringLiteral("Ilustração de modelos e serviços da Atena"));

    header->addLayout(headerText, 1);
    header->addWidget(illustration);
    outer->addLayout(header);

    m_state->setObjectName(QStringLiteral("Muted"));
    m_state->setText(QStringLiteral("Aguardando conexão com o Core."));
    outer->addWidget(m_state);

    auto *host = new QWidget(this);
    m_list = new QVBoxLayout(host);
    m_list->setSpacing(10);
    m_list->addStretch();
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(host);
    outer->addWidget(scroll, 1);

    connect(refresh, &QPushButton::clicked, m_client, &AtenaClientFacade::requestModels);
    connect(pull, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const QString model = QInputDialog::getText(
            this,
            QStringLiteral("Baixar modelo"),
            QStringLiteral("Nome do modelo no Ollama (ex.: qwen3:0.6b):"),
            QLineEdit::Normal,
            QString(),
            &ok).trimmed();
        if (ok && !model.isEmpty()) {
            m_client->pullModel(QStringLiteral("ollama"), model);
        }
    });
    connect(m_client, &AtenaClientFacade::modelsReady, this, &ModelsPage::rebuild);
    connect(m_client, &AtenaClientFacade::connectionStateChanged, this,
            [this, refresh, pull](AtenaClientFacade::ConnectionState state, const QString &detail) {
                m_connected = state == AtenaClientFacade::ConnectionState::Connected;
                refresh->setEnabled(m_connected);
                pull->setEnabled(m_connected);
                if (m_connected) {
                    m_state->setText(QStringLiteral("Core conectado · consultando modelos do Ollama…"));
                    m_client->requestModels();
                } else if (state == AtenaClientFacade::ConnectionState::Connecting) {
                    m_state->setText(QStringLiteral("Conectando ao Core…"));
                } else {
                    rebuild({});
                    m_state->setText(QStringLiteral("Modelos indisponíveis enquanto o Core estiver offline. %1").arg(detail));
                }
            });

    rebuild({});
}

void ModelsPage::rebuild(const QVariantList &models)
{
    while (m_list->count() > 1) {
        auto *item = m_list->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (models.isEmpty()) {
        auto *empty = new QLabel(
            m_connected ? QStringLiteral("Nenhum modelo encontrado no Ollama. Use ‘Baixar modelo’ para instalar um.")
                        : QStringLiteral("Conecte o Core para consultar os modelos."),
            this);
        empty->setWordWrap(true);
        empty->setObjectName(QStringLiteral("Muted"));
        m_list->insertWidget(0, empty);
        return;
    }
    m_state->setText(QStringLiteral("%1 modelo(s) encontrado(s) no Ollama.").arg(models.size()));

    for (const QVariant &entry : models) {
        const QVariantMap model = entry.toMap();
        const bool active = model.value(QStringLiteral("active")).toBool();

        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("Card"));
        card->setMinimumHeight(76);
        auto *row = new QHBoxLayout(card);

        auto *modelIcon = new QLabel(card);
        modelIcon->setPixmap(assetPixmap(QStringLiteral(":/icons/navigation/models.png"))
                                 .scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        const QString suffix = active ? QStringLiteral(" · <b>em uso</b>") : QString();
        auto *text = new QLabel(
            QStringLiteral("<b>%1</b><br>%2 · %3%4")
                .arg(model.value(QStringLiteral("name")).toString(),
                     model.value(QStringLiteral("runtime")).toString(),
                     model.value(QStringLiteral("size")).toString(),
                     suffix), card);

        const QString modelId = model.value(
            QStringLiteral("id"), model.value(QStringLiteral("name"))).toString();
        const QString providerId = model.value(
            QStringLiteral("provider_id"), QStringLiteral("ollama")).toString();

        auto *use = new QPushButton(active ? QStringLiteral("Em uso") : QStringLiteral("Usar"), card);
        use->setObjectName(QStringLiteral("PrimaryButton"));
        use->setEnabled(!active);
        use->setAccessibleName(QStringLiteral("Usar modelo %1").arg(modelId));

        auto *remove = new QPushButton(QStringLiteral("Remover"), card);
        remove->setEnabled(!active);
        remove->setAccessibleName(QStringLiteral("Remover modelo %1").arg(modelId));

        row->addWidget(modelIcon);
        row->addWidget(text, 1);
        row->addWidget(remove);
        row->addWidget(use);
        m_list->insertWidget(m_list->count() - 1, card);

        connect(use, &QPushButton::clicked, this,
                [this, providerId, modelId] { m_client->selectModel(providerId, modelId); });
        connect(remove, &QPushButton::clicked, this, [this, providerId, modelId] {
            if (QMessageBox::question(
                    this,
                    QStringLiteral("Remover modelo"),
                    QStringLiteral("Remover %1 do Ollama?").arg(modelId)) == QMessageBox::Yes) {
                m_client->removeModel(providerId, modelId);
            }
        });
    }
}

} // namespace AtenaUi
