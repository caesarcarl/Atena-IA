#include "KnowledgePage.h"
#include "../AssetCatalog.h"
#include "../client/AtenaClientFacade.h"

#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace AtenaUi {

KnowledgePage::KnowledgePage(AtenaClientFacade *client, QWidget *parent)
    : QWidget(parent),
      m_client(client),
      m_list(nullptr),
      m_state(new QLabel(this)),
      m_add(new QPushButton(assetIcon(QStringLiteral(":/icons/quick-actions/add-knowledge.png")),
                            QStringLiteral("Adicionar documento"), this))
{
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(28, 24, 28, 24);
    outer->setSpacing(24);

    auto *content = new QVBoxLayout();
    content->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Conhecimento"), this);
    title->setObjectName(QStringLiteral("PageTitle"));

    auto *desc = new QLabel(
        QStringLiteral("Adicione TXT ou Markdown ao índice RAG local. O conteúdo é entregue ao Atena Core e indexado no SQLite/FTS5. PDF ainda exige um parser próprio."),
        this);
    desc->setWordWrap(true);
    desc->setObjectName(QStringLiteral("Muted"));

    m_add->setObjectName(QStringLiteral("PrimaryButton"));
    m_add->setAccessibleName(QStringLiteral("Adicionar documento ao conhecimento"));
    m_add->setEnabled(false);

    m_state->setText(QStringLiteral("Aguardando conexão com o Core."));
    m_state->setObjectName(QStringLiteral("Muted"));
    m_state->setWordWrap(true);

    content->addWidget(title);
    content->addWidget(desc);
    content->addWidget(m_add, 0, Qt::AlignLeft);
    content->addWidget(m_state);

    auto *host = new QWidget(this);
    m_list = new QVBoxLayout(host);
    m_list->setContentsMargins(0, 4, 0, 0);
    m_list->setSpacing(10);
    m_list->addStretch();
    content->addWidget(host, 1);

    auto *illustration = new QLabel(this);
    illustration->setPixmap(assetPixmap(QStringLiteral(":/illustrations/library-knowledge.png"))
                                .scaled(500, 310, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    illustration->setAlignment(Qt::AlignCenter);
    illustration->setAccessibleName(QStringLiteral("Ilustração da biblioteca de conhecimento da Atena"));

    outer->addLayout(content, 3);
    outer->addWidget(illustration, 2);

    connect(m_add, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Selecionar documento"),
            QString(),
            QStringLiteral("Documentos de texto (*.txt *.md *.markdown);;Todos os arquivos (*)"));
        if (!path.isEmpty()) m_client->ingestDocument(path);
    });

    connect(m_client, &AtenaClientFacade::documentsReady, this, &KnowledgePage::rebuild);
    connect(m_client, &AtenaClientFacade::documentProgress, this,
            [this](const QString &, const QString &label, qint64, qint64) {
                m_state->setText(label);
            });
    connect(m_client, &AtenaClientFacade::connectionStateChanged, this,
            [this](AtenaClientFacade::ConnectionState state, const QString &detail) {
                const bool connected = state == AtenaClientFacade::ConnectionState::Connected;
                m_add->setEnabled(connected);
                if (connected) {
                    m_state->setText(QStringLiteral("Core conectado · carregando documentos indexados…"));
                    m_client->requestDocuments();
                } else if (state == AtenaClientFacade::ConnectionState::Connecting) {
                    m_state->setText(QStringLiteral("Conectando ao Core…"));
                } else {
                    rebuild({});
                    m_state->setText(QStringLiteral("Conhecimento indisponível enquanto o Core estiver offline. %1").arg(detail));
                }
            });
}

void KnowledgePage::rebuild(const QVariantList &documents)
{
    while (m_list->count() > 1) {
        auto *item = m_list->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (documents.isEmpty()) {
        m_state->setText(QStringLiteral("Nenhum documento indexado. Use ‘Adicionar documento’ para começar."));
        return;
    }

    m_state->setText(QStringLiteral("%1 documento(s) indexado(s) no Core.").arg(documents.size()));
    for (const QVariant &entry : documents) {
        const QVariantMap document = entry.toMap();
        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("Card"));
        card->setMinimumHeight(70);
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(14, 12, 14, 12);

        auto *icon = new QLabel(card);
        icon->setPixmap(assetPixmap(QStringLiteral(":/icons/navigation/library.png"))
                            .scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        auto *text = new QVBoxLayout();
        auto *name = new QLabel(document.value(QStringLiteral("title")).toString(), card);
        name->setObjectName(QStringLiteral("SectionTitle"));
        auto *details = new QLabel(
            QStringLiteral("%1 fragmento(s) · %2")
                .arg(document.value(QStringLiteral("chunks")).toInt())
                .arg(document.value(QStringLiteral("created_at")).toString()),
            card);
        details->setObjectName(QStringLiteral("Muted"));
        text->addWidget(name);
        text->addWidget(details);

        row->addWidget(icon);
        row->addLayout(text, 1);
        m_list->insertWidget(m_list->count() - 1, card);
    }
}

} // namespace AtenaUi
