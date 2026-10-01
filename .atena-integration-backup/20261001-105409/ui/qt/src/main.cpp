#include "Application.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QResource>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    // atena.qrc is compiled into a static UI library. Explicit initialization
    // guarantees the linker keeps and registers the resource object.
    Q_INIT_RESOURCE(atena);

    QCoreApplication::setOrganizationName(QStringLiteral("AthenasOS"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("athenas.local"));
    QCoreApplication::setApplicationName(QStringLiteral("Atena"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.5.0-base"));

    QApplication::setApplicationDisplayName(QStringLiteral("Atena"));
    QApplication::setDesktopFileName(QStringLiteral("atena"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/branding/atena-app-icon.png")));

    AtenaUi::Application atena(&app);
    return atena.run();
}
