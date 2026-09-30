#pragma once

#include <QPixmap>
#include <QVariant>
#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QResizeEvent;

namespace AtenaUi {

class AtenaClientFacade;

class KnowledgePage final : public QWidget {
    Q_OBJECT
public:
    explicit KnowledgePage(AtenaClientFacade *client, QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuild(const QVariantList &documents);
    void updateResponsiveLayout();
    void updateIllustrationGeometry();

    AtenaClientFacade *m_client;
    QVBoxLayout *m_list;
    QLabel *m_state;
    QPushButton *m_add;
    QScrollArea *m_listScroll;
    QFrame *m_illustrationCard;
    QLabel *m_illustration;
    QPixmap m_illustrationSource;
};

} // namespace AtenaUi
