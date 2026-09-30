#pragma once
#include <QPushButton>

namespace AtenaUi {
class NavButton final : public QPushButton {
    Q_OBJECT
public:
    explicit NavButton(const QString &text, const QString &iconResource, QWidget *parent = nullptr);
};
}
