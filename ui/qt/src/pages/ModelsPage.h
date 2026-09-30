#pragma once
#include <QWidget>
#include <QVariant>
class QVBoxLayout;
class QLabel;
namespace AtenaUi { class AtenaClientFacade;
class ModelsPage final : public QWidget {
    Q_OBJECT
public:
    explicit ModelsPage(AtenaClientFacade *client, QWidget *parent=nullptr);
private:
    void rebuild(const QVariantList &models);
    AtenaClientFacade *m_client;
    QVBoxLayout *m_list;
    QLabel *m_state;
    bool m_connected{false};
}; }
