#pragma once
#include <QFrame>
class QLabel;

namespace AtenaUi {
class MessageBubble final : public QFrame {
    Q_OBJECT
public:
    enum class Role { User, Assistant, System };
    explicit MessageBubble(Role role, const QString &text, QWidget *parent = nullptr);
    void appendDelta(const QString &text);
    QString plainText() const;
    static QString sanitizeMarkdown(const QString &input);
private:
    void render();
    Role m_role;
    QString m_text;
    QLabel *m_roleLabel;
    QLabel *m_content;
};
}
