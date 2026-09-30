#pragma once

#include <QMainWindow>
#include "client/AtenaClientFacade.h"

class QCloseEvent;
class QFrame;
class QResizeEvent;
class QStackedWidget;

namespace AtenaUi {

class ThemeManager;
class StatusChip;
class ChatPage;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(AtenaClientFacade *client, ThemeManager *theme, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void showPage(int index);
    void showOnboardingIfNeeded();
    void updateResponsiveShell();

    AtenaClientFacade *m_client;
    ThemeManager *m_theme;
    QStackedWidget *m_stack;
    StatusChip *m_status;
    ChatPage *m_chat;
    QFrame *m_sidebar{nullptr};
};

} // namespace AtenaUi
