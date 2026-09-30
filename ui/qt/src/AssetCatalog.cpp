#include "AssetCatalog.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>

namespace AtenaUi {

QString resolveAsset(const QString &resourcePath)
{
    if (resourcePath.isEmpty()) return resourcePath;
    if (QFileInfo::exists(resourcePath)) return resourcePath;

    QString relative = resourcePath;
    if (relative.startsWith(QStringLiteral(":/"))) relative.remove(0, 2);
    else if (relative.startsWith(QLatin1Char('/'))) relative.remove(0, 1);

    const QString installed = QDir(QStringLiteral("/usr/share/atena/assets")).filePath(relative);
    if (QFileInfo::exists(installed)) return installed;

    const QString besideExecutable = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("assets/") + relative);
    if (QFileInfo::exists(besideExecutable)) return besideExecutable;

    return resourcePath;
}

QIcon assetIcon(const QString &resourcePath)
{
    return QIcon(resolveAsset(resourcePath));
}

QPixmap assetPixmap(const QString &resourcePath)
{
    return QPixmap(resolveAsset(resourcePath));
}

} // namespace AtenaUi
