#pragma once
#include <QMainWindow>
#include "client/AtenaClientFacade.h"
class QStackedWidget;
class QCloseEvent;
namespace AtenaUi { class ThemeManager; class StatusChip; class ChatPage;
class MainWindow final : public QMainWindow {
    Q_OBJECT

public: MainWindow(AtenaClientFacade *client,ThemeManager *theme,QWidget *parent=nullptr);
protected: void closeEvent(QCloseEvent *event) override;
private: void showPage(int index); void showOnboardingIfNeeded(); AtenaClientFacade *m_client; ThemeManager *m_theme; QStackedWidget *m_stack; StatusChip *m_status; ChatPage *m_chat; };
}
