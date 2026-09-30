#include "ChatComposer.h"
#include "../AssetCatalog.h"
#include "../theme/UiMetrics.h"

#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSize>
#include <QTextDocument>

#include <algorithm>
#include <cmath>

namespace AtenaUi {

ChatComposer::ChatComposer(QWidget *parent)
    : QFrame(parent),
      m_editor(new QPlainTextEdit(this)),
      m_attach(new QPushButton(this)),
      m_send(new QPushButton(QStringLiteral("Enviar"), this)),
      m_stop(new QPushButton(QStringLiteral("Parar"), this))
{
    setObjectName(QStringLiteral("Composer"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(8);

    m_editor->setPlaceholderText(QStringLiteral("Pergunte à Atena…"));
    m_editor->setTabChangesFocus(true);
    m_editor->setAccessibleName(QStringLiteral("Mensagem para Atena"));
    m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_editor->installEventFilter(this);

    m_attach->setIcon(assetIcon(QStringLiteral(":/icons/navigation/files.png")));
    m_attach->setIconSize(QSize(20, 20));
    m_attach->setToolTip(QStringLiteral("Anexar documento"));
    m_attach->setAccessibleName(QStringLiteral("Anexar documento"));
    m_attach->setFixedWidth(42);

    m_send->setIcon(assetIcon(QStringLiteral(":/icons/system/send.svg")));
    m_send->setObjectName(QStringLiteral("PrimaryButton"));
    m_send->setAccessibleName(QStringLiteral("Enviar mensagem"));

    m_stop->setIcon(assetIcon(QStringLiteral(":/icons/system/stop.svg")));
    m_stop->setObjectName(QStringLiteral("DangerButton"));
    m_stop->setAccessibleName(QStringLiteral("Parar geração"));
    m_stop->hide();

    layout->addWidget(m_attach);
    layout->addWidget(m_editor, 1);
    layout->addWidget(m_stop);
    layout->addWidget(m_send);

    connect(m_editor->document(), &QTextDocument::contentsChanged,
            this, &ChatComposer::updateEditorHeight);

    connect(m_send, &QPushButton::clicked, this, [this] {
        const QString value = m_editor->toPlainText().trimmed();
        if (!value.isEmpty()) Q_EMIT sendRequested(value);
    });
    connect(m_stop, &QPushButton::clicked, this, &ChatComposer::stopRequested);
    connect(m_attach, &QPushButton::clicked, this, &ChatComposer::attachRequested);

    updateEditorHeight();
}

QString ChatComposer::text() const { return m_editor->toPlainText(); }

void ChatComposer::clear()
{
    m_editor->clear();
    updateEditorHeight();
}

void ChatComposer::setGenerating(bool generating)
{
    m_send->setVisible(!generating);
    m_stop->setVisible(generating);
    m_editor->setEnabled(!generating);
}

void ChatComposer::updateEditorHeight()
{
    const int docHeight = static_cast<int>(std::ceil(m_editor->document()->size().height()));
    const int target = std::clamp(docHeight + 18,
                                  Metrics::ComposerMinHeight,
                                  Metrics::ComposerMaxHeight);
    m_editor->setFixedHeight(target);
}

bool ChatComposer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_editor && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return && (key->modifiers() & Qt::ControlModifier)) {
            const QString value = m_editor->toPlainText().trimmed();
            if (!value.isEmpty()) Q_EMIT sendRequested(value);
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}

} // namespace AtenaUi
