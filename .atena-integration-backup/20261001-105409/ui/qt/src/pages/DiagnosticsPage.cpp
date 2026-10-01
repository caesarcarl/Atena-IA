#include "DiagnosticsPage.h"
#include "../AssetCatalog.h"
#include "../client/AtenaClientFacade.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSysInfo>
#include <QStyle>
#include <QVBoxLayout>

#ifndef ATENA_PACKAGE_REVISION
#define ATENA_PACKAGE_REVISION "development"
#endif

namespace AtenaUi {

DiagnosticsPage::DiagnosticsPage(AtenaClientFacade *client, QWidget *parent)
    : QWidget(parent),
      m_client(client),
      m_output(new QPlainTextEdit(this)),
      m_state(new QLabel(QStringLiteral("Aguardando consulta ao Core"), this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("Diagnóstico"), this);
    title->setObjectName(QStringLiteral("PageTitle"));

    auto *desc = new QLabel(
        QStringLiteral("Verifica a interface, o executável do Core, permissões, endpoint IPC e o log de inicialização."),
        this);
    desc->setObjectName(QStringLiteral("Muted"));
    desc->setWordWrap(true);

    auto *runButton = new QPushButton(assetIcon(QStringLiteral(":/icons/navigation/diagnostics.png")),
                                      QStringLiteral("Executar teste"), this);
    runButton->setObjectName(QStringLiteral("PrimaryButton"));

    auto *reconnectButton = new QPushButton(QStringLiteral("Reiniciar conexão com o Core"), this);
    auto *copyButton = new QPushButton(QStringLiteral("Copiar diagnóstico"), this);

    m_state->setObjectName(QStringLiteral("Muted"));
    m_output->setReadOnly(true);
    m_output->setAccessibleName(QStringLiteral("Resultado do diagnóstico"));

    layout->addWidget(title);
    layout->addWidget(desc);
    layout->addWidget(m_state);

    auto *actionsLayout = new QHBoxLayout();
    actionsLayout->addWidget(runButton);
    actionsLayout->addWidget(reconnectButton);
    actionsLayout->addWidget(copyButton);
    actionsLayout->addStretch();
    layout->addLayout(actionsLayout);
    layout->addWidget(m_output, 1);

    const QVariantMap localInfo{
        {QStringLiteral("ui_version"), QStringLiteral("0.5.0-base")},
        {QStringLiteral("package_revision"), QStringLiteral(ATENA_PACKAGE_REVISION)},
        {QStringLiteral("qt_version"), QString::fromLatin1(qVersion())},
        {QStringLiteral("os"), QSysInfo::prettyProductName()},
        {QStringLiteral("architecture"), QSysInfo::currentCpuArchitecture()},
        {QStringLiteral("core"), QStringLiteral("aguardando teste")}
    };
    display(localInfo);

    connect(runButton, &QPushButton::clicked, m_client, &AtenaClientFacade::requestDoctor);
    connect(reconnectButton, &QPushButton::clicked, m_client, &AtenaClientFacade::connectToCore);
    connect(m_client, &AtenaClientFacade::doctorReady, this, &DiagnosticsPage::display);

    connect(m_client, &AtenaClientFacade::connectionStateChanged, this,
            [this](AtenaClientFacade::ConnectionState state, const QString &detail) {
                if (state == AtenaClientFacade::ConnectionState::Connected) {
                    m_state->setText(QStringLiteral("Core conectado. O teste pode consultar o backend."));
                    m_state->setObjectName(QStringLiteral("SuccessText"));
                } else if (state == AtenaClientFacade::ConnectionState::Connecting) {
                    m_state->setText(QStringLiteral("Tentando conectar ao Core…"));
                    m_state->setObjectName(QStringLiteral("Muted"));
                } else {
                    m_state->setText(QStringLiteral("Core offline · %1").arg(detail));
                    m_state->setObjectName(QStringLiteral("WarningText"));
                }
                m_state->style()->unpolish(m_state);
                m_state->style()->polish(m_state);
            });

    connect(copyButton, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_output->toPlainText());
    });
}

void DiagnosticsPage::display(const QVariantMap &payload)
{
    QVariantMap merged = payload;
    if (!merged.contains(QStringLiteral("ui_version")))
        merged.insert(QStringLiteral("ui_version"), QStringLiteral("0.5.0-base"));
    if (!merged.contains(QStringLiteral("package_revision")))
        merged.insert(QStringLiteral("package_revision"), QStringLiteral(ATENA_PACKAGE_REVISION));
    if (!merged.contains(QStringLiteral("qt_version")))
        merged.insert(QStringLiteral("qt_version"), QString::fromLatin1(qVersion()));
    if (!merged.contains(QStringLiteral("os")))
        merged.insert(QStringLiteral("os"), QSysInfo::prettyProductName());
    if (!merged.contains(QStringLiteral("architecture")))
        merged.insert(QStringLiteral("architecture"), QSysInfo::currentCpuArchitecture());

    const QJsonDocument document = QJsonDocument::fromVariant(merged);
    m_output->setPlainText(QString::fromUtf8(document.toJson(QJsonDocument::Indented)));
}

} // namespace AtenaUi
