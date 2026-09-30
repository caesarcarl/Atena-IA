#pragma once

#include <QString>

namespace AtenaUi {

struct ThemePalette {
    QString background;
    QString surface;
    QString surfaceRaised;
    QString surfaceInteractive;
    QString textPrimary;
    QString textSecondary;
    QString border;
    QString borderStrong;
    QString primary;
    QString primaryHover;
    QString primaryPressed;
    QString success;
    QString warning;
    QString error;
    QString gold;
    QString scrollTrack;
    QString scrollThumb;
    QString scrollThumbHover;
};

inline ThemePalette lightPalette()
{
    return {
        QStringLiteral("#FAF8F5"),
        QStringLiteral("#FFFFFF"),
        QStringLiteral("#F5F1F8"),
        QStringLiteral("#EFEAF4"),
        QStringLiteral("#21155C"),
        QStringLiteral("#6B647A"),
        QStringLiteral("#B9B0C3"),
        QStringLiteral("#8C8298"),
        QStringLiteral("#6928D9"),
        QStringLiteral("#7A3CE4"),
        QStringLiteral("#5620B5"),
        QStringLiteral("#2F8B68"),
        QStringLiteral("#A66E13"),
        QStringLiteral("#B83D59"),
        QStringLiteral("#B7892E"),
        QStringLiteral("#EEEAF1"),
        QStringLiteral("#746A81"),
        QStringLiteral("#5B5068")
    };
}

inline ThemePalette darkPalette()
{
    return {
        QStringLiteral("#15131B"),
        QStringLiteral("#201D2A"),
        QStringLiteral("#292538"),
        QStringLiteral("#302A40"),
        QStringLiteral("#F7F3FF"),
        QStringLiteral("#B6AEC7"),
        QStringLiteral("#514864"),
        QStringLiteral("#6B5E80"),
        QStringLiteral("#7B3FF2"),
        QStringLiteral("#8E55FF"),
        QStringLiteral("#642BCB"),
        QStringLiteral("#48A47C"),
        QStringLiteral("#D29A42"),
        QStringLiteral("#D86A7D"),
        QStringLiteral("#D4A94F"),
        QStringLiteral("#1C1924"),
        QStringLiteral("#655A77"),
        QStringLiteral("#817292")
    };
}

} // namespace AtenaUi
