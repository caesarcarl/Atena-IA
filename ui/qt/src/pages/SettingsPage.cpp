#include "SettingsPage.h"
#include "../theme/ThemeManager.h"
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
namespace AtenaUi {
SettingsPage::SettingsPage(ThemeManager *theme,QWidget *parent):QWidget(parent){ auto *layout=new QVBoxLayout(this); layout->setContentsMargins(28,24,28,24); auto *title=new QLabel(QStringLiteral("Configurações")); title->setObjectName(QStringLiteral("PageTitle")); auto *themeLabel=new QLabel(QStringLiteral("Tema")); auto *combo=new QComboBox; combo->addItem(QStringLiteral("Sistema"),0); combo->addItem(QStringLiteral("Claro"),1); combo->addItem(QStringLiteral("Escuro"),2); combo->setAccessibleName(QStringLiteral("Tema da interface")); const int current=theme->currentTheme()==ThemeManager::Theme::Light?1:theme->currentTheme()==ThemeManager::Theme::Dark?2:0; combo->setCurrentIndex(current); layout->addWidget(title); layout->addWidget(themeLabel); layout->addWidget(combo,0,Qt::AlignLeft); auto *note=new QLabel(QStringLiteral("QSettings é usado apenas para preferências visuais locais e geometria da janela. Credenciais não são persistidas pela UI.")); note->setWordWrap(true); note->setObjectName(QStringLiteral("Muted")); layout->addWidget(note); layout->addStretch(); connect(combo,&QComboBox::currentIndexChanged,this,[theme](int i){theme->setTheme(i==1?ThemeManager::Theme::Light:i==2?ThemeManager::Theme::Dark:ThemeManager::Theme::System);}); }
}
