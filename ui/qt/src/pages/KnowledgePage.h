#pragma once
#include <QWidget>
#include <QVariant>
class QVBoxLayout;
class QLabel;
class QPushButton;

namespace AtenaUi {
class AtenaClientFacade;
class KnowledgePage final : public QWidget {
    Q_OBJECT
public:
    explicit KnowledgePage(AtenaClientFacade *client, QWidget *parent = nullptr);
private:
    void rebuild(const QVariantList &documents);
    AtenaClientFacade *m_client;
    QVBoxLayout *m_list;
    QLabel *m_state;
    QPushButton *m_add;
};
}
