#pragma once
#include <QFrame>
class QPlainTextEdit;
class QPushButton;

namespace AtenaUi {
class ChatComposer final : public QFrame {
    Q_OBJECT
public:
    explicit ChatComposer(QWidget *parent = nullptr);
    QString text() const;
    void clear();
    void setGenerating(bool generating);
Q_SIGNALS:
    void sendRequested(const QString &text);
    void stopRequested();
    void attachRequested();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    QPlainTextEdit *m_editor;
    QPushButton *m_attach;
    QPushButton *m_send;
    QPushButton *m_stop;
};
}
