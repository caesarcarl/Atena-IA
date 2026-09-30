#pragma once
#include <QWidget>
#include <QVariant>
class QLabel;
class QPlainTextEdit;

namespace AtenaUi {
class AtenaClientFacade;
class DiagnosticsPage final : public QWidget {
    Q_OBJECT
public:
    explicit DiagnosticsPage(AtenaClientFacade *client, QWidget *parent = nullptr);
private:
    void display(const QVariantMap &payload);
    AtenaClientFacade *m_client;
    QPlainTextEdit *m_output;
    QLabel *m_state;
};
}
