#pragma once
#include <QWidget>
#include <QVariant>
class QVBoxLayout;
class QLabel;

namespace AtenaUi {
class AtenaClientFacade;
class ToolsPage final : public QWidget {
    Q_OBJECT
public:
    explicit ToolsPage(AtenaClientFacade *client, QWidget *parent = nullptr);
private:
    void rebuild(const QVariantList &tools);
    AtenaClientFacade *m_client;
    QVBoxLayout *m_list;
    QLabel *m_state;
};
}
