#include "ChatPage.h"
#include "../client/AtenaClientFacade.h"
#include "../widgets/ChatComposer.h"
#include "../widgets/MessageBubble.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace AtenaUi {

ChatPage::ChatPage(AtenaClientFacade *client, QWidget *parent)
    : QWidget(parent),
      m_client(client),
      m_scroll(new QScrollArea(this)),
      m_messageHost(new QWidget),
      m_messages(new QVBoxLayout(m_messageHost)),
      m_composer(new ChatComposer(this)),
      m_engine(new QLabel(this))
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(22, 18, 22, 18);
    outer->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Conversa"), this);
    title->setObjectName(QStringLiteral("PageTitle"));

    auto *banner = new QFrame(this);
    banner->setObjectName(QStringLiteral("ConnectionBanner"));
    auto *bannerLayout = new QHBoxLayout(banner);
    bannerLayout->setContentsMargins(12, 8, 12, 8);
    m_engine->setText(QStringLiteral("Conectando ao Atena Core…"));
    m_engine->setObjectName(QStringLiteral("Muted"));
    m_engine->setWordWrap(true);
    auto *reconnect = new QPushButton(QStringLiteral("Reconectar"), banner);
    reconnect->setAccessibleName(QStringLiteral("Reconectar ao Atena Core"));
    bannerLayout->addWidget(m_engine, 1);
    bannerLayout->addWidget(reconnect);

    outer->addWidget(title);
    outer->addWidget(banner);

    m_messages->setContentsMargins(10, 10, 10, 10);
    m_messages->setSpacing(12);
    m_messages->addStretch();

    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setWidget(m_messageHost);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(m_scroll, 1);
    outer->addWidget(m_composer);

    auto *bar = m_scroll->verticalScrollBar();
    connect(bar, &QScrollBar::valueChanged, this, [this, bar](int value) {
        const int distance = bar->maximum() - value;
        m_followTail = distance <= 32;
    });
    connect(bar, &QScrollBar::rangeChanged, this, [this](int, int) {
        updateBubbleWidths();
        if (m_followTail) scrollToBottom();
    });

    m_composer->setEnabled(false);
    addMessage(MessageBubble::Role::System,
               QStringLiteral("A conversa será habilitada quando a interface concluir a conexão com o Atena Core."));

    connect(reconnect, &QPushButton::clicked, m_client, &AtenaClientFacade::connectToCore);
    connect(m_composer, &ChatComposer::sendRequested, this, &ChatPage::handleSend);
    connect(m_composer, &ChatComposer::stopRequested, this, [this] {
        if (!m_activeOperation.isEmpty()) m_client->cancelOperation(m_activeOperation);
    });
    connect(m_composer, &ChatComposer::attachRequested, this, &ChatPage::attachDocumentRequested);

    connect(m_client, &AtenaClientFacade::connectionStateChanged, this,
            [this, reconnect](AtenaClientFacade::ConnectionState state, const QString &detail) {
                m_connected = state == AtenaClientFacade::ConnectionState::Connected;
                m_composer->setEnabled(m_connected);
                reconnect->setVisible(!m_connected && state != AtenaClientFacade::ConnectionState::Connecting);
                if (m_connected) {
                    m_lastError.clear();
                    m_engine->setText(QStringLiteral("Core conectado · provider local Ollama disponível para consulta"));
                } else if (state == AtenaClientFacade::ConnectionState::Connecting) {
                    m_engine->setText(QStringLiteral("Conectando ao Atena Core…"));
                } else {
                    m_engine->setText(friendlyError(QStringLiteral("CORE_CONNECTION"), detail));
                }
            });

    connect(m_client, &AtenaClientFacade::chatAccepted, this,
            [this](const QString &op, const QString &session) {
                m_activeOperation = op;
                m_sessionId = session;
                m_streamingBubble = nullptr;
                m_composer->setGenerating(true);
                m_followTail = true;
            });

    connect(m_client, &AtenaClientFacade::chatDelta, this,
            [this](const QString &op, const QString &delta) {
                if (op != m_activeOperation) return;
                if (!m_streamingBubble) {
                    m_streamingBubble = new MessageBubble(MessageBubble::Role::Assistant, QString(), m_messageHost);
                    m_streamingBubble->setStreaming(true);
                    m_streamingBubble->setAvailableWidth(m_scroll->viewport()->width() - 24);
                    m_messages->insertWidget(m_messages->count() - 1, m_streamingBubble, 0, Qt::AlignLeft);
                }
                m_streamingBubble->appendDelta(delta);
                if (m_followTail) scrollToBottom();
            }, Qt::QueuedConnection);

    connect(m_client, &AtenaClientFacade::operationFinished, this,
            [this](const QString &op, const QString &status, const QString &message) {
                if (!m_activeOperation.isEmpty() && op != m_activeOperation) return;
                if (status != QStringLiteral("completed") && !message.isEmpty()) {
                    const QString friendly = friendlyError(QStringLiteral("CHAT_ERROR"), message);
                    if (friendly != m_lastError) {
                        addMessage(MessageBubble::Role::System, friendly);
                        m_lastError = friendly;
                    }
                }
                if (m_streamingBubble) m_streamingBubble->setStreaming(false);
                m_activeOperation.clear();
                m_streamingBubble = nullptr;
                m_composer->setGenerating(false);
                updateBubbleWidths();
            }, Qt::QueuedConnection);

    connect(m_client, &AtenaClientFacade::clientError, this,
            [this](const QString &code, const QString &message) {
                const QString friendly = friendlyError(code, message);
                if (code == QStringLiteral("NOT_CONFIGURED") ||
                    code == QStringLiteral("CORE_CONNECTION") ||
                    message == QStringLiteral("network_error")) {
                    m_engine->setText(friendly);
                } else if (friendly != m_lastError) {
                    addMessage(MessageBubble::Role::System, friendly);
                    m_lastError = friendly;
                }
                if (m_streamingBubble) m_streamingBubble->setStreaming(false);
                m_activeOperation.clear();
                m_streamingBubble = nullptr;
                m_composer->setGenerating(false);
            });
}

void ChatPage::setEngineLabel(const QString &label)
{
    if (!label.isEmpty()) m_engine->setText(label);
}

void ChatPage::newConversation()
{
    if (!m_activeOperation.isEmpty()) m_client->cancelOperation(m_activeOperation);
    m_activeOperation.clear();
    m_sessionId.clear();
    m_streamingBubble = nullptr;
    m_lastError.clear();
    m_followTail = true;
    m_composer->setGenerating(false);
    clearMessages();
    addMessage(MessageBubble::Role::System,
               m_connected ? QStringLiteral("Nova conversa iniciada.")
                           : QStringLiteral("Nova conversa criada. Conecte o Core para enviar mensagens."));
}

int ChatPage::messageCount() const
{
    return m_messages->count() - 1;
}

void ChatPage::addMessage(MessageBubble::Role role, const QString &text)
{
    auto *bubble = new MessageBubble(role, text, m_messageHost);
    bubble->setAvailableWidth(m_scroll->viewport()->width() - 24);
    m_messages->insertWidget(m_messages->count() - 1, bubble, 0,
                             role == MessageBubble::Role::User ? Qt::AlignRight : Qt::AlignLeft);
    m_followTail = true;
    scrollToBottom(true);
}

void ChatPage::handleSend(const QString &text)
{
    if (!m_connected) {
        m_engine->setText(QStringLiteral("Core desconectado. Use ‘Reconectar’ antes de enviar a mensagem."));
        return;
    }
    addMessage(MessageBubble::Role::User, text);
    m_composer->clear();
    m_client->startChat(text, m_sessionId);
}

void ChatPage::clearMessages()
{
    while (m_messages->count() > 1) {
        auto *item = m_messages->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

QString ChatPage::friendlyError(const QString &code, const QString &message) const
{
    const QString technical = message.trimmed();
    if (code == QStringLiteral("NOT_CONFIGURED") || technical == QStringLiteral("network_error"))
        return QStringLiteral("Não foi possível conectar ao Atena Core. Abra Diagnóstico para ver o executável, socket e log de inicialização.");
    if (technical == QStringLiteral("provider_unavailable"))
        return QStringLiteral("O serviço de IA selecionado não está disponível. Verifique o Ollama em Serviços de IA.");
    if (technical == QStringLiteral("timeout"))
        return QStringLiteral("A operação excedeu o tempo de espera. O modelo pode estar carregando ou o serviço pode estar lento.");
    if (technical == QStringLiteral("cancelled"))
        return QStringLiteral("A geração foi cancelada.");
    if (technical.isEmpty())
        return QStringLiteral("A operação não pôde ser concluída.");
    return QStringLiteral("Não foi possível concluir a operação. Detalhe técnico: %1").arg(technical);
}

void ChatPage::scrollToBottom(bool force)
{
    if (!force && !m_followTail) return;
    QTimer::singleShot(0, this, [this, force] {
        if (!force && !m_followTail) return;
        auto *bar = m_scroll->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
}

void ChatPage::updateBubbleWidths()
{
    const int width = qMax(220, m_scroll->viewport()->width() - 24);
    for (int i = 0; i < m_messages->count() - 1; ++i) {
        if (auto *bubble = qobject_cast<MessageBubble *>(m_messages->itemAt(i)->widget()))
            bubble->setAvailableWidth(width);
    }
}

void ChatPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateBubbleWidths();
}

} // namespace AtenaUi
