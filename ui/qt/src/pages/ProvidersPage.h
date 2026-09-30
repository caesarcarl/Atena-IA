#pragma once
#include <QWidget>
#include <QVariant>
#include <QHash>
class QLabel;

namespace AtenaUi {
class AtenaClientFacade;
class ProvidersPage final : public QWidget {
    Q_OBJECT
public:
    explicit ProvidersPage(AtenaClientFacade *client, QWidget *parent = nullptr);
private:
    void rebuild(const QVariantList &providers);
    void openSetup(const QString &presetId, const QString &label);
    AtenaClientFacade *m_client;
    QHash<QString, QLabel *> m_statusLabels;
};
}
