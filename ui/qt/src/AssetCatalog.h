#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

namespace AtenaUi {

QString resolveAsset(const QString &resourcePath);
QIcon assetIcon(const QString &resourcePath);
QPixmap assetPixmap(const QString &resourcePath);

} // namespace AtenaUi
