#include "OnboardingDialog.h"
#include "../client/AtenaClientFacade.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace AtenaUi {
namespace {

QWidget *welcomePage()
{
    auto *page = new QWidget();
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(24, 18, 24, 18);
    layout->setSpacing(26);

    auto *illustration = new QLabel(page);
    illustration->setPixmap(QPixmap(QStringLiteral(":/illustrations/athena-about-onboarding.png"))
                                .scaled(230, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    illustration->setAlignment(Qt::AlignCenter);
    illustration->setAccessibleName(QStringLiteral("Atena com lança e escudo"));

    auto *content = new QVBoxLayout();
    content->setSpacing(12);

    auto *logo = new QLabel(page);
    logo->setPixmap(QPixmap(QStringLiteral(":/branding/atena-ia-logo-horizontal.png"))
                        .scaled(390, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    logo->setAccessibleName(QStringLiteral("Logo Atena IA"));

    auto *title = new QLabel(QStringLiteral("Bem-vindo à Atena"), page);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size:28px;font-weight:700;"));

    auto *description = new QLabel(
        QStringLiteral("Uma assistente local, híbrida e extensível. Configure o Ollama, "
                       "seus modelos e a forma de processamento sem separar a interface do Atena Core."), page);
    description->setWordWrap(true);
    description->setAlignment(Qt::AlignCenter);

    content->addStretch();
    content->addWidget(logo);
    content->addWidget(title);
    content->addWidget(description);
    content->addStretch();

    layout->addWidget(illustration, 1);
    layout->addLayout(content, 2);
    return page;
}

QWidget *modePage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Como deseja usar IA?"), page);
    title->setStyleSheet(QStringLiteral("font-size:24px;font-weight:700;"));
    layout->addWidget(title);

    auto *group = new QButtonGroup(page);
    const struct { const char *label; const char *icon; } modes[] = {
        {"Modelo local", ":/icons/quick-actions/mode-local.png"},
        {"API", ":/icons/quick-actions/mode-cloud.png"},
        {"Ambos", ":/icons/quick-actions/mode-hybrid.png"},
        {"Configurar depois", ":/icons/navigation/settings.png"},
    };

    for (const auto &mode : modes) {
        auto *row = new QHBoxLayout();
        auto *icon = new QLabel(page);
        icon->setPixmap(QPixmap(QString::fromLatin1(mode.icon))
                            .scaled(38, 38, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        auto *radio = new QRadioButton(QString::fromUtf8(mode.label), page);
        group->addButton(radio);
        row->addWidget(icon);
        row->addWidget(radio, 1);
        layout->addLayout(row);
    }

    group->buttons().last()->setChecked(true);
    layout->addStretch();
    return page;
}

QWidget *privacyPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);

    auto *title = new QLabel(QStringLiteral("Privacidade clara por padrão"), page);
    title->setStyleSheet(QStringLiteral("font-size:24px;font-weight:700;"));

    auto *description = new QLabel(
        QStringLiteral("Local não envia dados para providers externos. Cloud e Híbrido devem indicar "
                       "a fronteira antes de transmitir conteúdo local. A UI nunca deve salvar chaves "
                       "em texto simples."), page);
    description->setWordWrap(true);

    auto *icon = new QLabel(page);
    icon->setPixmap(QPixmap(QStringLiteral(":/icons/navigation/privacy.png"))
                        .scaled(110, 110, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon->setAlignment(Qt::AlignCenter);

    layout->addWidget(title);
    layout->addWidget(description);
    layout->addSpacing(18);
    layout->addWidget(icon);
    layout->addStretch();
    return page;
}

} // namespace

OnboardingDialog::OnboardingDialog(AtenaClientFacade *client, QWidget *parent)
    : QDialog(parent),
      m_client(client),
      m_pages(new QStackedWidget(this)),
      m_back(new QPushButton(QStringLiteral("Voltar"), this)),
      m_next(new QPushButton(QStringLiteral("Continuar"), this)),
      m_progress(new QLabel(this))
{
    Q_UNUSED(m_client);
    setWindowTitle(QStringLiteral("Configurar Atena"));
    setWindowIcon(QIcon(QStringLiteral(":/branding/app-icon/atena-64x64.png")));
    setMinimumSize(800, 560);

    auto *outer = new QVBoxLayout(this);
    m_pages->addWidget(welcomePage());
    m_pages->addWidget(modePage());
    m_pages->addWidget(privacyPage());

    outer->addWidget(m_progress);
    outer->addWidget(m_pages, 1);

    auto *actions = new QHBoxLayout();
    actions->addWidget(m_back);
    actions->addStretch();
    m_next->setObjectName(QStringLiteral("PrimaryButton"));
    actions->addWidget(m_next);
    outer->addLayout(actions);

    m_back->setEnabled(false);
    m_progress->setText(QStringLiteral("1 de 3"));

    connect(m_back, &QPushButton::clicked, this, [this] {
        if (m_pages->currentIndex() > 0) {
            m_pages->setCurrentIndex(m_pages->currentIndex() - 1);
        }
        m_back->setEnabled(m_pages->currentIndex() > 0);
        m_next->setText(QStringLiteral("Continuar"));
        m_progress->setText(QStringLiteral("%1 de 3").arg(m_pages->currentIndex() + 1));
    });
    connect(m_next, &QPushButton::clicked, this, &OnboardingDialog::next);
}

void OnboardingDialog::next()
{
    if (m_pages->currentIndex() == m_pages->count() - 1) {
        accept();
        return;
    }
    m_pages->setCurrentIndex(m_pages->currentIndex() + 1);
    m_back->setEnabled(true);
    if (m_pages->currentIndex() == m_pages->count() - 1) {
        m_next->setText(QStringLiteral("Concluir"));
    }
    m_progress->setText(QStringLiteral("%1 de 3").arg(m_pages->currentIndex() + 1));
}

int OnboardingDialog::currentStep() const
{
    return m_pages->currentIndex();
}

} // namespace AtenaUi
