#pragma once
#include <QWidget>
namespace AtenaUi { class ThemeManager;
class SettingsPage final : public QWidget {
    Q_OBJECT
public: explicit SettingsPage(ThemeManager *theme,QWidget *parent=nullptr); }; }
