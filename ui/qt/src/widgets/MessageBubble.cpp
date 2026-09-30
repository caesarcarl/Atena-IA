#include "MessageBubble.h"
#include "../theme/UiMetrics.h"

#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace AtenaUi {

MessageBubble::MessageBubble(Role role, const QString &text, QWidget *parent)
    : QFrame(parent),
      m_role(role),
      m_text(text),
      m_roleLabel(new QLabel(this)),
      m_content(new QTextBrowser(this)),
      m_expand(new QPushButton(this))
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

    m_content->setObjectName(QStringLiteral("MessageContent"));
    m_content->setFrameShape(QFrame::NoFrame);
    m_content->setOpenExternalLinks(false);
    m_content->setOpenLinks(false);
    m_content->setReadOnly(true);
    m_content->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    m_content->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_content->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_content->document()->setDocumentMargin(0.0);

    m_expand->setObjectName(QStringLiteral("LinkButton"));
    m_expand->setText(QStringLiteral("Ver mais"));
    m_expand->setCursor(Qt::PointingHandCursor);
    m_expand->setAccessibleName(QStringLiteral("Expandir ou recolher mensagem"));
    m_expand->hide();

    layout->addWidget(m_roleLabel);
    layout->addWidget(m_content);
    layout->addWidget(m_expand, 0, Qt::AlignRight);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    setAccessibleName(QStringLiteral("Mensagem de %1").arg(m_roleLabel->text()));

    connect(m_expand, &QPushButton::clicked, this, [this] {
        m_expanded = !m_expanded;
        m_expand->setText(m_expanded ? QStringLiteral("Recolher") : QStringLiteral("Ver mais"));
        updateContentGeometry();
    });

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

int MessageBubble::preferredBubbleWidth(int availableWidth) const
{
    const int safeAvailable = std::max(220, availableWidth);
    const double fraction = m_role == Role::User ? 0.72 : 0.86;
    const int preferred = static_cast<int>(std::lround(static_cast<double>(safeAvailable) * fraction));
    const int upper = std::max(220, std::min(Metrics::ChatBubbleMaxWidth, safeAvailable - 12));
    const int lowerTarget = m_role == Role::User ? Metrics::ChatUserMinWidth : Metrics::ChatAssistantMinWidth;
    const int lower = std::min(lowerTarget, upper);
    return std::clamp(preferred, lower, upper);
}

void MessageBubble::setAvailableWidth(int width)
{
    m_availableWidth = std::max(220, width);
    const int bubbleWidth = preferredBubbleWidth(m_availableWidth);
    setMaximumWidth(bubbleWidth);
    setMinimumWidth(std::min(bubbleWidth, m_role == Role::User ? 150 : 240));
    updateContentGeometry();
}

void MessageBubble::setStreaming(bool streaming)
{
    m_streaming = streaming;
    if (streaming) m_expanded = true;
    updateContentGeometry();
}

void MessageBubble::render()
{
    m_content->setMarkdown(sanitizeMarkdown(m_text));
    updateContentGeometry();
}

void MessageBubble::updateContentGeometry()
{
    const int frameMargins = 28;
    const int bubbleWidth = preferredBubbleWidth(m_availableWidth);
    const int documentWidth = std::max(160, bubbleWidth - frameMargins);

    setMaximumWidth(bubbleWidth);
    m_content->document()->setTextWidth(documentWidth);

    const int naturalHeight = std::max(32,
        static_cast<int>(std::ceil(m_content->document()->size().height())) + 8);
    const bool longMessage = naturalHeight > Metrics::ChatCollapsedHeight;
    const bool showFull = m_streaming || m_expanded || !longMessage;
    const int visibleHeight = showFull ? naturalHeight
                                       : std::min(naturalHeight, Metrics::ChatCollapsedHeight);

    m_content->setFixedHeight(visibleHeight);
    m_expand->setVisible(longMessage && !m_streaming);
    m_expand->setText(m_expanded ? QStringLiteral("Recolher") : QStringLiteral("Ver mais"));

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

void MessageBubble::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    updateContentGeometry();
}

} // namespace AtenaUi
