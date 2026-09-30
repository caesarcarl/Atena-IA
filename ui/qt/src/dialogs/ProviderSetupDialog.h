#pragma once
#include <QDialog>
#include <QVariant>
class QLineEdit;
namespace AtenaUi {
class ProviderSetupDialog final : public QDialog {
    Q_OBJECT

public: ProviderSetupDialog(const QString &presetId,const QString &displayName,QWidget *parent=nullptr); QVariantMap takeConfiguration();
private: QString m_presetId; QLineEdit *m_endpoint; QLineEdit *m_apiKey; QLineEdit *m_model; QVariantMap m_configuration; };
}
