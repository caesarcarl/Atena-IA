#include "ThemeManager.h"
#include "ThemePalette.h"

#include <QApplication>
#include <QEvent>
#include <QPalette>
#include <QSettings>

namespace AtenaUi {

ThemeManager::ThemeManager(QApplication *app, QObject *parent)
    : QObject(parent), m_app(app)
{
    QSettings settings(QStringLiteral("AthenasOS"), QStringLiteral("Atena"));
    const QString saved = settings.value(QStringLiteral("ui/theme"), QStringLiteral("system")).toString();
    if (saved == QStringLiteral("light")) m_theme = Theme::Light;
    else if (saved == QStringLiteral("dark")) m_theme = Theme::Dark;
    apply();

    m_app->installEventFilter(this);
}

ThemeManager::Theme ThemeManager::currentTheme() const { return m_theme; }

bool ThemeManager::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_app && event->type() == QEvent::ApplicationPaletteChange && m_theme == Theme::System)
        apply();
    return QObject::eventFilter(watched, event);
}

QString ThemeManager::themeName(Theme theme)
{
    switch (theme) {
        case Theme::Light: return QStringLiteral("Claro");
        case Theme::Dark: return QStringLiteral("Escuro");
        case Theme::System: return QStringLiteral("Sistema");
    }
    return QStringLiteral("Sistema");
}

void ThemeManager::setTheme(Theme theme)
{
    if (m_theme == theme) return;
    m_theme = theme;
    QSettings settings(QStringLiteral("AthenasOS"), QStringLiteral("Atena"));
    settings.setValue(QStringLiteral("ui/theme"), theme == Theme::Light ? QStringLiteral("light") :
        theme == Theme::Dark ? QStringLiteral("dark") : QStringLiteral("system"));
    apply();
    Q_EMIT themeChanged(m_theme);
}

bool ThemeManager::isDarkEffective() const
{
    if (m_theme == Theme::Dark) return true;
    if (m_theme == Theme::Light) return false;
    return m_app->palette().color(QPalette::Window).lightness() < 128;
}

void ThemeManager::apply()
{
    m_app->setStyleSheet(buildStyleSheet(isDarkEffective()));
}

QString ThemeManager::buildStyleSheet(bool dark) const
{
    const ThemePalette p = dark ? darkPalette() : lightPalette();

    QString style = QStringLiteral(R"QSS(
        QWidget {
            color: @TEXT@;
            font-family: sans-serif;
            font-size: 14px;
            background: transparent;
        }

        QMainWindow, QDialog, QWidget#AppRoot, QFrame#ContentSurface, QStackedWidget {
            background: @BG@;
        }

        QFrame#Sidebar {
            background: @SURFACE@;
            border-right: 1px solid @BORDER@;
        }

        QFrame#TopBar {
            background: @BG@;
            border-bottom: 1px solid @BORDER@;
        }

        QLabel { background: transparent; }
        QLabel#BrandTitle { font-size: 20px; font-weight: 700; color: @TEXT@; }
        QLabel#PageTitle { font-size: 24px; font-weight: 700; }
        QLabel#SectionTitle { font-size: 16px; font-weight: 700; }
        QLabel#Muted { color: @MUTED@; }
        QLabel#SuccessText { color: @SUCCESS@; }
        QLabel#WarningText { color: @WARNING@; }
        QLabel#ErrorText { color: @ERROR@; }

        QPushButton {
            border: 1px solid @BORDER@;
            border-radius: 10px;
            padding: 9px 13px;
            background: @SURFACE@;
        }
        QPushButton:hover { background: @INTERACTIVE@; border-color: @BORDER_STRONG@; }
        QPushButton:pressed { background: @RAISED@; }
        QPushButton:disabled { color: @MUTED@; background: @BG@; border-color: @BORDER@; }
        QPushButton:focus { border: 2px solid @PRIMARY@; }

        QPushButton#PrimaryButton {
            color: white;
            background: @PRIMARY@;
            border: 1px solid @PRIMARY@;
            font-weight: 600;
        }
        QPushButton#PrimaryButton:hover { background: @PRIMARY_HOVER@; border-color: @PRIMARY_HOVER@; }
        QPushButton#PrimaryButton:pressed { background: @PRIMARY_PRESSED@; border-color: @PRIMARY_PRESSED@; }

        QPushButton#DangerButton {
            color: white;
            background: @ERROR@;
            border: 1px solid @ERROR@;
        }

        QPushButton#LinkButton {
            color: @PRIMARY_HOVER@;
            background: transparent;
            border: none;
            padding: 4px 6px;
            font-weight: 600;
        }
        QPushButton#LinkButton:hover { color: @PRIMARY@; text-decoration: underline; background: transparent; }

        QPushButton#NavButton {
            border: 1px solid transparent;
            text-align: left;
            padding: 10px 12px;
            background: transparent;
        }
        QPushButton#NavButton:hover { background: @RAISED@; border-color: @BORDER@; }
        QPushButton#NavButton:checked {
            background: @RAISED@;
            color: @PRIMARY_HOVER@;
            border-color: @BORDER@;
            font-weight: 600;
        }

        QLineEdit, QTextEdit, QPlainTextEdit, QTextBrowser, QComboBox {
            background: @SURFACE@;
            color: @TEXT@;
            border: 1px solid @BORDER@;
            border-radius: 10px;
            padding: 8px;
            selection-background-color: @PRIMARY@;
            selection-color: white;
        }

        QTextBrowser#MessageContent {
            background: transparent;
            border: none;
            border-radius: 0;
            padding: 0;
        }

        QLineEdit:hover, QTextEdit:hover, QPlainTextEdit:hover, QComboBox:hover {
            border-color: @BORDER_STRONG@;
        }

        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus {
            border: 2px solid @PRIMARY@;
        }

        QComboBox { padding-right: 28px; min-height: 20px; }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border-left: 1px solid @BORDER@;
            background: @RAISED@;
            border-top-right-radius: 9px;
            border-bottom-right-radius: 9px;
        }
        QComboBox QAbstractItemView {
            background: @SURFACE@;
            color: @TEXT@;
            border: 1px solid @BORDER_STRONG@;
            border-radius: 8px;
            padding: 4px;
            outline: 0;
            selection-background-color: @PRIMARY@;
            selection-color: white;
        }
        QComboBox QAbstractItemView::item {
            min-height: 30px;
            padding: 5px 8px;
            border-radius: 6px;
        }
        QComboBox QAbstractItemView::item:hover { background: @RAISED@; }
        QComboBox QAbstractItemView::item:selected { background: @PRIMARY@; color: white; }

        QMenu {
            background: @SURFACE@;
            color: @TEXT@;
            border: 1px solid @BORDER_STRONG@;
            border-radius: 8px;
            padding: 5px;
        }
        QMenu::item { padding: 7px 24px 7px 10px; border-radius: 6px; }
        QMenu::item:selected { background: @PRIMARY@; color: white; }

        QAbstractScrollArea { background: transparent; border: none; }
        QAbstractScrollArea QWidget { background: transparent; }

        QScrollBar:vertical {
            background: @SCROLL_TRACK@;
            width: 10px;
            margin: 2px;
            border: none;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical {
            background: @SCROLL_THUMB@;
            min-height: 34px;
            border: 1px solid @BORDER_STRONG@;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover { background: @SCROLL_HOVER@; }
        QScrollBar::handle:vertical:pressed { background: @PRIMARY@; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

        QScrollBar:horizontal {
            background: @SCROLL_TRACK@;
            height: 10px;
            margin: 2px;
            border: none;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal {
            background: @SCROLL_THUMB@;
            min-width: 34px;
            border: 1px solid @BORDER_STRONG@;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover { background: @SCROLL_HOVER@; }
        QScrollBar::handle:horizontal:pressed { background: @PRIMARY@; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

        QFrame#Card {
            background: @SURFACE@;
            border: 1px solid @BORDER@;
            border-radius: 14px;
        }
        QFrame#KnowledgeIllustrationCard {
            background: @SURFACE@;
            border: 1px solid @BORDER_STRONG@;
            border-radius: 16px;
        }
        QLabel#KnowledgeIllustration {
            background: @BG@;
            border: none;
            padding: 0;
        }
        QFrame#SubtleCard {
            background: @RAISED@;
            border: 1px solid @BORDER@;
            border-radius: 12px;
        }
        QFrame#AssistantBubble {
            background: @RAISED@;
            border: 1px solid @BORDER@;
            border-radius: 14px;
        }
        QFrame#SystemBubble {
            background: @SURFACE@;
            border: 1px solid @BORDER@;
            border-radius: 14px;
        }
        QFrame#UserBubble {
            background: @PRIMARY@;
            border: 1px solid @PRIMARY_HOVER@;
            border-radius: 14px;
        }
        QFrame#UserBubble QLabel, QFrame#UserBubble QTextBrowser { color: white; }

        QFrame#Composer {
            background: @SURFACE@;
            border: 1px solid @BORDER@;
            border-radius: 14px;
        }
        QFrame#Composer:hover { border-color: @BORDER_STRONG@; }

        QFrame#ConnectionBanner {
            background: @RAISED@;
            border: 1px solid @BORDER@;
            border-radius: 10px;
        }

        QProgressBar {
            border: 1px solid @BORDER@;
            border-radius: 6px;
            text-align: center;
            background: @RAISED@;
        }
        QProgressBar::chunk { background: @PRIMARY@; border-radius: 5px; }

        QToolTip {
            background: @SURFACE@;
            color: @TEXT@;
            border: 1px solid @BORDER_STRONG@;
            padding: 5px;
        }
    )QSS");

    const auto replace = [&style](const QString &token, const QString &value) {
        style.replace(token, value);
    };

    replace(QStringLiteral("@BG@"), p.background);
    replace(QStringLiteral("@SURFACE@"), p.surface);
    replace(QStringLiteral("@RAISED@"), p.surfaceRaised);
    replace(QStringLiteral("@INTERACTIVE@"), p.surfaceInteractive);
    replace(QStringLiteral("@TEXT@"), p.textPrimary);
    replace(QStringLiteral("@MUTED@"), p.textSecondary);
    replace(QStringLiteral("@BORDER@"), p.border);
    replace(QStringLiteral("@BORDER_STRONG@"), p.borderStrong);
    replace(QStringLiteral("@PRIMARY@"), p.primary);
    replace(QStringLiteral("@PRIMARY_HOVER@"), p.primaryHover);
    replace(QStringLiteral("@PRIMARY_PRESSED@"), p.primaryPressed);
    replace(QStringLiteral("@SUCCESS@"), p.success);
    replace(QStringLiteral("@WARNING@"), p.warning);
    replace(QStringLiteral("@ERROR@"), p.error);
    replace(QStringLiteral("@SCROLL_TRACK@"), p.scrollTrack);
    replace(QStringLiteral("@SCROLL_THUMB@"), p.scrollThumb);
    replace(QStringLiteral("@SCROLL_HOVER@"), p.scrollThumbHover);

    return style;
}

} // namespace AtenaUi
