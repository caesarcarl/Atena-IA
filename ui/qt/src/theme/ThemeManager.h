#pragma once
#include <QObject>
#include <QString>

class QApplication;

namespace AtenaUi {

class ThemeManager final : public QObject {
    Q_OBJECT
public:
    enum class Theme { System, Light, Dark };
    Q_ENUM(Theme)

    explicit ThemeManager(QApplication *app, QObject *parent = nullptr);
    Theme currentTheme() const;
    void setTheme(Theme theme);
    static QString themeName(Theme theme);
    bool isDarkEffective() const;

Q_SIGNALS:
    void themeChanged(AtenaUi::ThemeManager::Theme theme);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void apply();
    QString buildStyleSheet(bool dark) const;
    QApplication *m_app;
    Theme m_theme{Theme::System};
};

} // namespace AtenaUi
