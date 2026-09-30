#include "MainWindow.h"
#include "AssetCatalog.h"

#include "dialogs/OnboardingDialog.h"
#include "dialogs/ToolConfirmationDialog.h"
#include "pages/ChatPage.h"
#include "pages/DiagnosticsPage.h"
#include "pages/HomePage.h"
#include "pages/KnowledgePage.h"
#include "pages/ModelsPage.h"
#include "pages/PrivacyPage.h"
#include "pages/ProvidersPage.h"
#include "pages/SettingsPage.h"
#include "pages/ToolsPage.h"
#include "theme/ThemeManager.h"
#include "widgets/NavButton.h"
#include "widgets/StatusChip.h"

#include <QCloseEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include <iterator>

namespace AtenaUi {
namespace {

struct NavigationItem {
    const char *label;
    const char *icon;
};

constexpr NavigationItem kNavigation[] = {
    {"Início", ":/branding/app-icon/atena-64x64.png"},
    {"Conversa", ":/icons/navigation/new-conversation.png"},
    {"Modelos", ":/icons/navigation/models.png"},
    {"Serviços de IA", ":/icons/navigation/providers.png"},
    {"Conhecimento", ":/icons/navigation/library.png"},
    {"Ferramentas", ":/icons/navigation/tools.png"},
    {"Privacidade", ":/icons/navigation/privacy.png"},
    {"Diagnóstico", ":/icons/navigation/diagnostics.png"},
    {"Configurações", ":/icons/navigation/settings.png"},
};

QWidget *createBrand(QWidget *parent)
{
    auto *host = new QWidget(parent);
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(4);

    auto *logo = new QLabel(host);
    logo->setPixmap(assetPixmap(QStringLiteral(":/branding/atena-ia-logo-horizontal.png"))
                        .scaled(208, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    logo->setAccessibleName(QStringLiteral("Logo Atena IA"));

    auto *subtitle = new QLabel(QStringLiteral("IA local e híbrida"), host);
    subtitle->setObjectName(QStringLiteral("Muted"));
    subtitle->setAlignment(Qt::AlignCenter);

    layout->addWidget(logo);
    layout->addWidget(subtitle);
    return host;
}

} // namespace

MainWindow::MainWindow(AtenaClientFacade *client, ThemeManager *theme, QWidget *parent)
    : QMainWindow(parent),
      m_client(client),
      m_theme(theme),
      m_stack(new QStackedWidget(this)),
      m_status(new StatusChip(this)),
      m_chat(nullptr)
{
    setWindowTitle(QStringLiteral("Atena"));
    setWindowIcon(assetIcon(QStringLiteral(":/branding/atena-app-icon.png")));
    resize(1280, 760);
    setMinimumSize(960, 600);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("AppRoot"));
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *sidebar = new QFrame(central);
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(252);

    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(16, 18, 16, 18);
    sideLayout->setSpacing(5);
    sideLayout->addWidget(createBrand(sidebar));
    sideLayout->addSpacing(14);

    auto *newChat = new QPushButton(assetIcon(QStringLiteral(":/icons/navigation/new-conversation.png")),
                                    QStringLiteral("Nova conversa"), sidebar);
    newChat->setObjectName(QStringLiteral("PrimaryButton"));
    newChat->setAccessibleName(QStringLiteral("Nova conversa"));
    newChat->setMinimumHeight(42);
    sideLayout->addWidget(newChat);
    sideLayout->addSpacing(10);

    QList<NavButton *> navButtons;
    for (int index = 0; index < static_cast<int>(std::size(kNavigation)); ++index) {
        const auto &item = kNavigation[index];
        auto *button = new NavButton(QString::fromUtf8(item.label),
                                     QString::fromLatin1(item.icon), sidebar);
        navButtons.append(button);
        sideLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, index] { showPage(index); });
    }
    navButtons.first()->setChecked(true);
    sideLayout->addStretch();

    auto *version = new QLabel(QStringLiteral("Atena 0.5.0-base · integrada"), sidebar);
    version->setObjectName(QStringLiteral("Muted"));
    version->setAlignment(Qt::AlignCenter);
    sideLayout->addWidget(version);

    auto *content = new QFrame(central);
    content->setObjectName(QStringLiteral("ContentSurface"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    auto *topBar = new QFrame(content);
    topBar->setObjectName(QStringLiteral("TopBar"));
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(22, 10, 22, 10);

    auto *privacy = new QLabel(QStringLiteral("Processamento controlado pelo Atena Core"), topBar);
    privacy->setObjectName(QStringLiteral("Muted"));
    auto *reconnect = new QPushButton(QStringLiteral("Reconectar"), topBar);
    reconnect->setAccessibleName(QStringLiteral("Reconectar ao Atena Core"));
    reconnect->hide();

    topLayout->addWidget(privacy);
    topLayout->addStretch();
    topLayout->addWidget(reconnect);
    topLayout->addWidget(m_status);

    contentLayout->addWidget(topBar);
    contentLayout->addWidget(m_stack, 1);

    auto *home = new HomePage(m_stack);
    m_chat = new ChatPage(m_client, m_stack);

    m_stack->addWidget(home);
    m_stack->addWidget(m_chat);
    m_stack->addWidget(new ModelsPage(m_client, m_stack));
    m_stack->addWidget(new ProvidersPage(m_client, m_stack));
    m_stack->addWidget(new KnowledgePage(m_client, m_stack));
    m_stack->addWidget(new ToolsPage(m_client, m_stack));
    m_stack->addWidget(new PrivacyPage(m_stack));
    m_stack->addWidget(new DiagnosticsPage(m_client, m_stack));
    m_stack->addWidget(new SettingsPage(m_theme, m_stack));

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(content, 1);
    setCentralWidget(central);

    connect(newChat, &QPushButton::clicked, this, [this] { m_chat->newConversation(); showPage(1); });
    connect(reconnect, &QPushButton::clicked, m_client, &AtenaClientFacade::connectToCore);
    connect(home, &HomePage::modeSelected, this, [this](const QString &) { showPage(1); });

    connect(m_client, &AtenaClientFacade::connectionStateChanged, this,
            [this, reconnect](AtenaClientFacade::ConnectionState state, const QString &detail) {
                const bool connected = state == AtenaClientFacade::ConnectionState::Connected;
                const QString status = connected
                    ? QStringLiteral("Core conectado")
                    : state == AtenaClientFacade::ConnectionState::Connecting
                        ? QStringLiteral("Conectando…")
                        : QStringLiteral("Core indisponível");

                m_status->setStatus(status, connected);
                reconnect->setVisible(state != AtenaClientFacade::ConnectionState::Connecting && !connected);
                m_chat->setEngineLabel(connected
                    ? QStringLiteral("Core conectado · Ollama pronto para ser consultado")
                    : detail);
            });

    connect(m_client, &AtenaClientFacade::toolConfirmationRequested, this,
            [this](const QVariantMap &request) {
                ToolConfirmationDialog dialog(request, this);
                dialog.exec();
                m_client->confirmTool(
                    request.value(QStringLiteral("confirmation_id")).toString(),
                    dialog.allowedOnce());
            });

    connect(m_client, &AtenaClientFacade::clientError, this,
            [this](const QString &code, const QString &message) {
                statusBar()->showMessage(QStringLiteral("%1 · %2").arg(code, message), 8000);
            });

    QSettings settings(QStringLiteral("AthenasOS"), QStringLiteral("Atena"));
    if (settings.contains(QStringLiteral("ui/geometry"))) {
        restoreGeometry(settings.value(QStringLiteral("ui/geometry")).toByteArray());
    }

    QTimer::singleShot(0, this, &MainWindow::showOnboardingIfNeeded);
}

void MainWindow::showPage(int index)
{
    if (index >= 0 && index < m_stack->count()) {
        m_stack->setCurrentIndex(index);
    }
}

void MainWindow::showOnboardingIfNeeded()
{
    QSettings settings(QStringLiteral("AthenasOS"), QStringLiteral("Atena"));
    if (settings.value(QStringLiteral("ui/onboarding_complete"), false).toBool()) {
        return;
    }

    OnboardingDialog dialog(m_client, this);
    if (dialog.exec() == QDialog::Accepted) {
        settings.setValue(QStringLiteral("ui/onboarding_complete"), true);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings(QStringLiteral("AthenasOS"), QStringLiteral("Atena"));
    settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    QMainWindow::closeEvent(event);
}

} // namespace AtenaUi
