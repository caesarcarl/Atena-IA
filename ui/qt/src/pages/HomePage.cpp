#include "HomePage.h"
#include "../AssetCatalog.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace AtenaUi {
namespace {

QFrame *makeModeCard(const QString &title,
                     const QString &description,
                     const QString &icon,
                     const QString &mode,
                     HomePage *owner)
{
    auto *card = new QFrame(owner);
    card->setObjectName(QStringLiteral("Card"));

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(8);

    auto *iconLabel = new QLabel(card);
    iconLabel->setPixmap(assetPixmap(icon).scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAccessibleName(QStringLiteral("Ícone %1").arg(title));

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(QStringLiteral("font-weight:700;font-size:16px;"));

    auto *desc = new QLabel(description, card);
    desc->setObjectName(QStringLiteral("Muted"));
    desc->setWordWrap(true);

    auto *button = new QPushButton(QStringLiteral("Abrir"), card);
    button->setAccessibleName(QStringLiteral("Abrir modo %1").arg(title));

    layout->addWidget(iconLabel);
    layout->addWidget(titleLabel);
    layout->addWidget(desc);
    layout->addStretch();
    layout->addWidget(button);

    QObject::connect(button, &QPushButton::clicked, owner,
                     [owner, mode] { Q_EMIT owner->modeSelected(mode); });
    return card;
}

} // namespace

HomePage::HomePage(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(30, 26, 30, 26);
    outer->setSpacing(26);

    auto *left = new QVBoxLayout();
    left->setSpacing(12);

    auto *eyebrow = new QLabel(QStringLiteral("Inteligência moldada pelo conhecimento"), this);
    eyebrow->setObjectName(QStringLiteral("Muted"));

    auto *title = new QLabel(this);
    title->setPixmap(assetPixmap(QStringLiteral(":/branding/atena-ia-logo-horizontal.png"))
                         .scaled(390, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    title->setAccessibleName(QStringLiteral("Atena IA"));

    auto *subtitle = new QLabel(QStringLiteral("Como posso ajudá-lo a pensar hoje?"), this);
    subtitle->setStyleSheet(QStringLiteral("font-size:20px;"));

    left->addWidget(eyebrow);
    left->addWidget(title);
    left->addWidget(subtitle);
    left->addSpacing(10);

    auto *grid = new QGridLayout();
    grid->setSpacing(14);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->addWidget(makeModeCard(QStringLiteral("Pesquisar"),
                                 QStringLiteral("Investigue um assunto com contexto e fontes disponíveis."),
                                 QStringLiteral(":/icons/quick-actions/quick-search.png"),
                                 QStringLiteral("research"), this), 0, 0);
    grid->addWidget(makeModeCard(QStringLiteral("Criar"),
                                 QStringLiteral("Desenvolva ideias, textos e estruturas com a Atena."),
                                 QStringLiteral(":/icons/quick-actions/quick-create.png"),
                                 QStringLiteral("create"), this), 0, 1);
    grid->addWidget(makeModeCard(QStringLiteral("Estudar"),
                                 QStringLiteral("Aprenda em profundidade com uma abordagem pedagógica."),
                                 QStringLiteral(":/icons/quick-actions/quick-study.png"),
                                 QStringLiteral("study"), this), 1, 0);
    grid->addWidget(makeModeCard(QStringLiteral("Analisar"),
                                 QStringLiteral("Examine conteúdo, evidências e pontos importantes."),
                                 QStringLiteral(":/icons/quick-actions/quick-analyze.png"),
                                 QStringLiteral("analyze"), this), 1, 1);
    left->addLayout(grid);
    left->addStretch();

    auto *heroCard = new QFrame(this);
    heroCard->setMinimumWidth(360);
    heroCard->setObjectName(QStringLiteral("Card"));
    auto *heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(8, 8, 8, 8);

    auto *hero = new QLabel(heroCard);
    hero->setPixmap(assetPixmap(QStringLiteral(":/illustrations/home-hero.png"))
                        .scaled(560, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    hero->setAlignment(Qt::AlignCenter);
    hero->setAccessibleName(QStringLiteral("Ilustração principal da Atena IA"));
    heroLayout->addWidget(hero, 1);

    outer->addLayout(left, 3);
    outer->addWidget(heroCard, 2);
}

} // namespace AtenaUi
