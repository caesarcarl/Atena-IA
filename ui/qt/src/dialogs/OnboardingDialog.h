#pragma once
#include <QDialog>
class QStackedWidget;
class QPushButton;
class QLabel;
namespace AtenaUi { class AtenaClientFacade;
class OnboardingDialog final : public QDialog {
    Q_OBJECT

public: explicit OnboardingDialog(AtenaClientFacade *client,QWidget *parent=nullptr); int currentStep() const;
private: void next(); AtenaClientFacade *m_client; QStackedWidget *m_pages; QPushButton *m_back; QPushButton *m_next; QLabel *m_progress; };
}
