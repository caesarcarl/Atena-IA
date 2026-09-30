#include "PrivacyPage.h"
#include <QButtonGroup>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>
namespace AtenaUi {
PrivacyPage::PrivacyPage(QWidget *parent):QWidget(parent){ auto *layout=new QVBoxLayout(this); layout->setContentsMargins(28,24,28,24); auto *title=new QLabel(QStringLiteral("Privacidade")); title->setObjectName(QStringLiteral("PageTitle")); auto *desc=new QLabel(QStringLiteral("O modo efetivo é decidido e aplicado pelo Core. Esta tela explica a fronteira de processamento e será ligada a config.get/config.update pelo SDK.")); desc->setWordWrap(true); desc->setObjectName(QStringLiteral("Muted")); layout->addWidget(title); layout->addWidget(desc); auto *group=new QButtonGroup(this); const QList<QPair<QString,QString>> modes={{"Local","Processamento local. Nenhum fallback remoto implícito."},{"Cloud","Dados necessários podem sair da máquina para o provider escolhido."},{"Híbrido","Combina capacidades com autorização explícita para cruzar a fronteira."}}; int i=0; for(const auto&m:modes){ auto *r=new QRadioButton(QStringLiteral("%1\n%2").arg(m.first,m.second)); r->setEnabled(false); group->addButton(r,i++); layout->addWidget(r);} layout->addStretch(); }
}
