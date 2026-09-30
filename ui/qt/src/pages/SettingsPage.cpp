#include "SettingsPage.h"
#include "../theme/ThemeManager.h"

#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

namespace AtenaUi {

SettingsPage::SettingsPage(ThemeManager *theme, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Configurações"), this);
    title->setObjectName(QStringLiteral("PageTitle"));

    auto *appearance = new QFrame(this);
    appearance->setObjectName(QStringLiteral("Card"));
    auto *cardLayout = new QVBoxLayout(appearance);
    cardLayout->setContentsMargins(18, 16, 18, 16);
    cardLayout->setSpacing(10);

    auto *section = new QLabel(QStringLiteral("Aparência"), appearance);
    section->setObjectName(QStringLiteral("SectionTitle"));

    auto *themeLabel = new QLabel(QStringLiteral("Tema da interface"), appearance);
    auto *combo = new QComboBox(appearance);
    combo->addItem(QStringLiteral("Sistema"), 0);
    combo->addItem(QStringLiteral("Claro"), 1);
    combo->addItem(QStringLiteral("Escuro"), 2);
    combo->setAccessibleName(QStringLiteral("Tema da interface"));
    combo->setMinimumWidth(190);

    const int current = theme->currentTheme() == ThemeManager::Theme::Light ? 1
                      : theme->currentTheme() == ThemeManager::Theme::Dark ? 2 : 0;
    combo->setCurrentIndex(current);

    auto *note = new QLabel(
        QStringLiteral("A aparência é salva somente neste computador. Credenciais de serviços de IA não ficam nas preferências visuais."),
        appearance);
    note->setWordWrap(true);
    note->setObjectName(QStringLiteral("Muted"));

    cardLayout->addWidget(section);
    cardLayout->addWidget(themeLabel);
    cardLayout->addWidget(combo, 0, Qt::AlignLeft);
    cardLayout->addWidget(note);

    layout->addWidget(title);
    layout->addWidget(appearance);
    layout->addStretch();

    connect(combo, &QComboBox::currentIndexChanged, this, [theme](int i) {
        theme->setTheme(i == 1 ? ThemeManager::Theme::Light
                               : i == 2 ? ThemeManager::Theme::Dark
                                        : ThemeManager::Theme::System);
    });
}

} // namespace AtenaUi
