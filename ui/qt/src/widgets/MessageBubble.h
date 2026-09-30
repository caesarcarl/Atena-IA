#pragma once

#include <QFrame>

class QLabel;
class QPushButton;
class QTextBrowser;
class QResizeEvent;

namespace AtenaUi {

class MessageBubble final : public QFrame {
    Q_OBJECT
public:
    enum class Role { User, Assistant, System };

    explicit MessageBubble(Role role, const QString &text, QWidget *parent = nullptr);

    void appendDelta(const QString &text);
    void setAvailableWidth(int width);
    void setStreaming(bool streaming);
    QString plainText() const;

    static QString sanitizeMarkdown(const QString &input);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void render();
    void updateContentGeometry();
    int preferredBubbleWidth(int availableWidth) const;

    Role m_role;
    QString m_text;
    QLabel *m_roleLabel;
    QTextBrowser *m_content;
    QPushButton *m_expand;
    int m_availableWidth{640};
    bool m_expanded{false};
    bool m_streaming{false};
};

} // namespace AtenaUi
