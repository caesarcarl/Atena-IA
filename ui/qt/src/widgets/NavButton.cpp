#include "NavButton.h"
#include "../AssetCatalog.h"
#include <QIcon>
#include <QSize>

namespace AtenaUi {
NavButton::NavButton(const QString &text, const QString &iconResource, QWidget *parent)
    : QPushButton(assetIcon(iconResource), text, parent) {
    setObjectName(QStringLiteral("NavButton"));
    setCheckable(true);
    setAutoExclusive(true);
    setIconSize(QSize(20, 20));
    setMinimumHeight(42);
    setAccessibleName(text);
}
}
