#include "Application.h"
#include "MainWindow.h"
#include "client/RealAtenaClient.h"
#include "theme/ThemeManager.h"

#include <QApplication>

namespace AtenaUi {

Application::Application(QApplication *app)
    : QObject(app),
      m_app(app),
      m_client(std::make_unique<RealAtenaClient>()),
      m_theme(std::make_unique<ThemeManager>(app)),
      m_window(std::make_unique<MainWindow>(m_client.get(), m_theme.get()))
{
    m_client->connectToCore();
}

Application::~Application() = default;

int Application::run()
{
    m_window->show();
    return m_app->exec();
}

} // namespace AtenaUi
