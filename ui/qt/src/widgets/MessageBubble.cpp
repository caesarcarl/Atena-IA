#include "MessageBubble.h"

#include <QLabel>
#include <QRegularExpression>
#include <QTextDocument>
#include <QVBoxLayout>

namespace AtenaUi {

MessageBubble::MessageBubble(Role role, const QString &text, QWidget *parent)
    : QFrame(parent),
      m_role(role),
      m_text(text),
      m_roleLabel(new QLabel(this)),
      m_content(new QLabel(this))
{
    setObjectName(role == Role::User ? QStringLiteral("UserBubble")
                                    : role == Role::System ? QStringLiteral("SystemBubble")
                                                           : QStringLiteral("AssistantBubble"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 11, 14, 11);
    layout->setSpacing(6);

    m_roleLabel->setText(role == Role::User ? QStringLiteral("Você")
                                            : role == Role::Assistant ? QStringLiteral("Atena")
                                                                       : QStringLiteral("Sistema"));
    m_roleLabel->setStyleSheet(QStringLiteral("font-weight:700;"));

    m_content->setWordWrap(true);
    m_content->setTextFormat(Qt::RichText);
    m_content->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    m_content->setOpenExternalLinks(false);
    m_content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    layout->addWidget(m_roleLabel);
    layout->addWidget(m_content);
    setMaximumWidth(760);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);
    setAccessibleName(QStringLiteral("Mensagem de %1").arg(m_roleLabel->text()));
    render();
}

QString MessageBubble::sanitizeMarkdown(const QString &input)
{
    QString safe = input;
    safe.remove(QRegularExpression(
        QStringLiteral(R"(<\s*(img|iframe|object|embed|script|style)[^>]*>)"),
        QRegularExpression::CaseInsensitiveOption));
    safe.replace(QRegularExpression(QStringLiteral(R"(!\[([^\]]*)\]\([^\)]*\))")),
                 QStringLiteral("[imagem removida]"));
    safe.replace(QRegularExpression(
                     QStringLiteral(R"(\[([^\]]+)\]\((?:https?|file|ftp):[^\)]*\))"),
                     QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("\\1"));
    return safe;
}

void MessageBubble::render()
{
    QTextDocument document;
    document.setMarkdown(sanitizeMarkdown(m_text));
    m_content->setText(document.toHtml());
    m_content->adjustSize();
    updateGeometry();
}

void MessageBubble::appendDelta(const QString &text)
{
    m_text += text;
    render();
}

QString MessageBubble::plainText() const
{
    return m_text;
}

} // namespace AtenaUi
