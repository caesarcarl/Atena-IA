#include "ToolConfirmationDialog.h"
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
namespace AtenaUi {
ToolConfirmationDialog::ToolConfirmationDialog(const QVariantMap &request,QWidget *parent):QDialog(parent){ setWindowTitle(QStringLiteral("Confirmar ação da Atena")); setModal(true); setMinimumWidth(500); auto *layout=new QVBoxLayout(this); auto *title=new QLabel(QStringLiteral("Atena deseja executar uma ação")); title->setStyleSheet(QStringLiteral("font-size:18px;font-weight:700;")); auto *summary=new QLabel(request.value(QStringLiteral("summary")).toString()); summary->setWordWrap(true); auto *risk=new QLabel(QStringLiteral("Risco: %1").arg(request.value(QStringLiteral("risk")).toString())); risk->setStyleSheet(QStringLiteral("font-weight:600;color:#B87518;")); auto *details=new QLabel(QString::fromUtf8(QJsonDocument::fromVariant(request.value(QStringLiteral("arguments"))).toJson(QJsonDocument::Indented))); details->setTextInteractionFlags(Qt::TextSelectableByMouse); auto *buttons=new QDialogButtonBox(QDialogButtonBox::Cancel); auto *allow=buttons->addButton(QStringLiteral("Permitir uma vez"),QDialogButtonBox::AcceptRole); allow->setObjectName(QStringLiteral("PrimaryButton")); allow->setAccessibleName(QStringLiteral("Permitir esta ação uma vez")); layout->addWidget(title); layout->addWidget(summary); layout->addWidget(risk); layout->addWidget(details); layout->addWidget(buttons); connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject); connect(allow,&QPushButton::clicked,this,[this]{m_allowed=true;accept();}); }
bool ToolConfirmationDialog::allowedOnce() const{return m_allowed;}
}
