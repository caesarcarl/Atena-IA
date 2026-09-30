#pragma once
#include <QWidget>
namespace AtenaUi {
class HomePage final : public QWidget {
    Q_OBJECT
public:
    explicit HomePage(QWidget *parent = nullptr);
Q_SIGNALS:
    void modeSelected(const QString &mode);
};
}
