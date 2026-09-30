#pragma once
#include <QWidget>
#include "../widgets/MessageBubble.h"
class QScrollArea;
class QVBoxLayout;
class QLabel;

namespace AtenaUi {
class AtenaClientFacade;
class ChatComposer;
class MessageBubble;

class ChatPage final : public QWidget {
    Q_OBJECT
public:
    explicit ChatPage(AtenaClientFacade *client, QWidget *parent = nullptr);
    void setEngineLabel(const QString &label);
    void newConversation();
    int messageCount() const;
Q_SIGNALS:
    void attachDocumentRequested();
private:
    void addMessage(MessageBubble::Role role, const QString &text);
    void handleSend(const QString &text);
    void scrollToBottom();
    void clearMessages();
    QString friendlyError(const QString &code, const QString &message) const;
    AtenaClientFacade *m_client;
    QScrollArea *m_scroll;
    QWidget *m_messageHost;
    QVBoxLayout *m_messages;
    ChatComposer *m_composer;
    QLabel *m_engine;
    MessageBubble *m_streamingBubble{nullptr};
    QString m_activeOperation;
    QString m_sessionId;
    QString m_lastError;
    bool m_connected{false};
};
}
