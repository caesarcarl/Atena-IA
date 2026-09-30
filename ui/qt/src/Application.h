#pragma once
#include <QObject>
#include <memory>
class QApplication;
namespace AtenaUi { class AtenaClientFacade; class ThemeManager; class MainWindow;
class Application final : public QObject {
    Q_OBJECT

public: explicit Application(QApplication *app); ~Application() override; int run();
private: QApplication *m_app; std::unique_ptr<AtenaClientFacade> m_client; std::unique_ptr<ThemeManager> m_theme; std::unique_ptr<MainWindow> m_window; }; }
