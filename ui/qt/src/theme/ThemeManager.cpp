#include "ThemeManager.h"

#include <QApplication>
#include <QEvent>
#include <QPalette>
#include <QSettings>

namespace AtenaUi {

ThemeManager::ThemeManager(QApplication *app, QObject *parent)
    : QObject(parent), m_app(app) {
    QSettings settings(QStringLiteral("AthenasOS"), QStringLiteral("Atena"));
    const QString saved = settings.value(QStringLiteral("ui/theme"), QStringLiteral("system")).toString();
    if (saved == QStringLiteral("light")) m_theme = Theme::Light;
    else if (saved == QStringLiteral("dark")) m_theme = Theme::Dark;
    apply();

    m_app->installEventFilter(this);
}

ThemeManager::Theme ThemeManager::currentTheme() const { return m_theme; }

bool ThemeManager::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_app && event->type() == QEvent::ApplicationPaletteChange && m_theme == Theme::System) apply();
    return QObject::eventFilter(watched, event);
}

QString ThemeManager::themeName(Theme theme) {
    switch (theme) {
        case Theme::Light: return QStringLiteral("Claro");
        case Theme::Dark: return QStringLiteral("Escuro");
        case Theme::System: return QStringLiteral("Sistema");
    }
    return QStringLiteral("Sistema");
}

void ThemeManager::setTheme(Theme theme) {
    if (m_theme == theme) return;
    m_theme = theme;
    QSettings settings(QStringLiteral("AthenasOS"), QStringLiteral("Atena"));
    settings.setValue(QStringLiteral("ui/theme"), theme == Theme::Light ? QStringLiteral("light") :
        theme == Theme::Dark ? QStringLiteral("dark") : QStringLiteral("system"));
    apply();
    Q_EMIT themeChanged(m_theme);
}

void ThemeManager::apply() {
    bool dark = false;
    if (m_theme == Theme::Dark) dark = true;
    else if (m_theme == Theme::System)
        dark = m_app->palette().color(QPalette::Window).lightness() < 128;
    m_app->setStyleSheet(buildStyleSheet(dark));
}

QString ThemeManager::buildStyleSheet(bool dark) const {
    const QString bg = dark ? QStringLiteral("#15131B") : QStringLiteral("#FAF8F5");
    const QString surface = dark ? QStringLiteral("#201D2A") : QStringLiteral("#FFFFFF");
    const QString surfaceAlt = dark ? QStringLiteral("#292538") : QStringLiteral("#F4F0FA");
    const QString text = dark ? QStringLiteral("#F7F3FF") : QStringLiteral("#21155C");
    const QString muted = dark ? QStringLiteral("#B6AEC7") : QStringLiteral("#6B647A");
    const QString border = dark ? QStringLiteral("#3A344A") : QStringLiteral("#E7E0EA");
    return QStringLiteral(R"QSS(
        QWidget { color: %4; font-family: sans-serif; font-size: 14px; background: transparent; }
        QMainWindow, QDialog, QWidget#AppRoot, QFrame#ContentSurface { background: %1; }
        QStackedWidget { background: %1; }
        QFrame#Sidebar { background: %2; border-right: 1px solid %6; }
        QFrame#TopBar { background: %1; border-bottom: 1px solid %6; }
        QLabel { background: transparent; }
        QLabel#BrandTitle { font-size: 20px; font-weight: 700; color: %4; }
        QLabel#PageTitle { font-size: 24px; font-weight: 700; }
        QLabel#SectionTitle { font-size: 16px; font-weight: 700; }
        QLabel#Muted { color: %5; }
        QLabel#SuccessText { color: #48A47C; }
        QLabel#WarningText { color: #D29A42; }
        QLabel#ErrorText { color: #D86A7D; }
        QPushButton { border: 1px solid %6; border-radius: 10px; padding: 9px 13px; background: %2; }
        QPushButton:hover { background: %3; }
        QPushButton:disabled { color: %5; background: %1; }
        QPushButton:focus { border: 2px solid #7B3FF2; }
        QPushButton#PrimaryButton { color: white; background: #6928D9; border: none; font-weight: 600; }
        QPushButton#PrimaryButton:hover { background: #5720B8; }
        QPushButton#DangerButton { color: white; background: #B83D59; border: none; }
        QPushButton#NavButton { border: none; text-align: left; padding: 10px 12px; background: transparent; }
        QPushButton#NavButton:checked { background: %3; color: #9A67FF; font-weight: 600; }
        QLineEdit, QTextEdit, QPlainTextEdit, QComboBox { background: %2; border: 1px solid %6; border-radius: 10px; padding: 8px; selection-background-color: #6928D9; }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus { border: 2px solid #7B3FF2; }
        QAbstractScrollArea { background: transparent; border: none; }
        QAbstractScrollArea QWidget { background: transparent; }
        QFrame#Card { background: %2; border: 1px solid %6; border-radius: 14px; }
        QFrame#SubtleCard { background: %3; border: 1px solid %6; border-radius: 12px; }
        QFrame#AssistantBubble { background: %3; border: 1px solid %6; border-radius: 14px; }
        QFrame#SystemBubble { background: %2; border: 1px solid %6; border-radius: 14px; }
        QFrame#UserBubble { background: #6928D9; border-radius: 14px; }
        QFrame#UserBubble QLabel { color: white; }
        QFrame#Composer { background: %2; border: 1px solid %6; border-radius: 14px; }
        QFrame#ConnectionBanner { background: %3; border: 1px solid %6; border-radius: 10px; }
        QProgressBar { border: 1px solid %6; border-radius: 6px; text-align: center; background: %3; }
        QProgressBar::chunk { background: #6928D9; border-radius: 5px; }
        QToolTip { background: %2; color: %4; border: 1px solid %6; }
    )QSS").arg(bg, surface, surfaceAlt, text, muted, border);
}

} // namespace AtenaUi
