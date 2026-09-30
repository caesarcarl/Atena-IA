#include "KnowledgePage.h"
#include "../AssetCatalog.h"
#include "../client/AtenaClientFacade.h"

#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

namespace AtenaUi {

KnowledgePage::KnowledgePage(AtenaClientFacade *client, QWidget *parent)
    : QWidget(parent),
      m_client(client),
      m_list(nullptr),
      m_state(new QLabel(this)),
      m_add(new QPushButton(assetIcon(QStringLiteral(":/icons/quick-actions/add-knowledge.png")),
                            QStringLiteral("Adicionar documento"), this)),
      m_listScroll(new QScrollArea(this)),
      m_illustrationCard(new QFrame(this)),
      m_illustration(new QLabel(m_illustrationCard)),
      m_illustrationSource(assetPixmap(QStringLiteral(":/illustrations/library-knowledge.png")))
{
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(28, 24, 28, 24);
    outer->setSpacing(24);

    auto *contentHost = new QWidget(this);
    auto *content = new QVBoxLayout(contentHost);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Conhecimento"), this);
    title->setObjectName(QStringLiteral("PageTitle"));

    auto *desc = new QLabel(
        QStringLiteral("Adicione documentos ao conhecimento local da Atena. O conteúdo indexado pode ser usado como contexto nas respostas."),
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

    auto *host = new QWidget;
    m_list = new QVBoxLayout(host);
    m_list->setContentsMargins(0, 4, 6, 4);
    m_list->setSpacing(10);
    m_list->addStretch();

    m_listScroll->setFrameShape(QFrame::NoFrame);
    m_listScroll->setWidgetResizable(true);
    m_listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listScroll->setWidget(host);
    content->addWidget(m_listScroll, 1);

    m_illustrationCard->setObjectName(QStringLiteral("KnowledgeIllustrationCard"));
    m_illustrationCard->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *illustrationLayout = new QVBoxLayout(m_illustrationCard);
    illustrationLayout->setContentsMargins(12, 12, 12, 12);
    illustrationLayout->setSpacing(0);

    m_illustration->setObjectName(QStringLiteral("KnowledgeIllustration"));
    m_illustration->setAlignment(Qt::AlignCenter);
    m_illustration->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_illustration->setAccessibleName(QStringLiteral("Ilustração da biblioteca de conhecimento da Atena"));
    illustrationLayout->addWidget(m_illustration, 0, Qt::AlignCenter);

    // A área útil é elástica; o painel visual mantém proporção própria e não
    // deve crescer verticalmente só porque a lista de documentos é longa.
    outer->addWidget(contentHost, 1);
    outer->addWidget(m_illustrationCard, 0, Qt::AlignTop);

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
            [this](const QString &, const QString &label, qint64 current, qint64 total) {
                if (total > 0)
                    m_state->setText(QStringLiteral("%1 · %2/%3").arg(label).arg(current).arg(total));
                else
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

    updateResponsiveLayout();
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
        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("Card"));
        card->setMinimumHeight(74);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(14, 12, 14, 12);
        row->setSpacing(12);

        auto *icon = new QLabel(card);
        icon->setPixmap(assetPixmap(QStringLiteral(":/icons/navigation/library.png"))
                            .scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        icon->setFixedSize(38, 38);

        auto *text = new QVBoxLayout;
        auto *name = new QLabel(document.value(QStringLiteral("title")).toString(), card);
        name->setObjectName(QStringLiteral("SectionTitle"));
        name->setWordWrap(true);

        auto *details = new QLabel(
            QStringLiteral("%1 fragmento(s) · %2")
                .arg(document.value(QStringLiteral("chunks")).toInt())
                .arg(document.value(QStringLiteral("created_at")).toString()),
            card);
        details->setObjectName(QStringLiteral("Muted"));
        details->setWordWrap(true);

        text->addWidget(name);
        text->addWidget(details);

        row->addWidget(icon, 0, Qt::AlignTop);
        row->addLayout(text, 1);
        m_list->insertWidget(m_list->count() - 1, card);
    }
}

void KnowledgePage::updateResponsiveLayout()
{
    // O painel é complementar. Em janelas compactas a biblioteca recebe toda
    // a largura; em desktop ele volta como um card lateral de proporção fixa.
    const bool showIllustration = width() >= 1040 && !m_illustrationSource.isNull();
    m_illustrationCard->setVisible(showIllustration);
    if (showIllustration) updateIllustrationGeometry();
}

void KnowledgePage::updateIllustrationGeometry()
{
    if (m_illustrationSource.isNull()) return;

    // Aproximadamente um terço da página, limitado para não competir com a
    // lista RAG. O conteúdo preserva a proporção original do asset.
    const int cardWidth = qBound(320, qRound(width() * 0.31), 440);
    const int innerWidth = qMax(1, cardWidth - 24);
    const qreal ratio = qreal(m_illustrationSource.height()) / qreal(m_illustrationSource.width());
    const int innerHeight = qMax(1, qRound(innerWidth * ratio));

    const QPixmap scaled = m_illustrationSource.scaled(
        innerWidth,
        innerHeight,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);

    m_illustration->setPixmap(scaled);
    m_illustration->setFixedSize(scaled.size());
    m_illustrationCard->setFixedSize(scaled.width() + 24, scaled.height() + 24);
}

void KnowledgePage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateResponsiveLayout();
}

} // namespace AtenaUi
