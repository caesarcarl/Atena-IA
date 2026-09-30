#include "RealAtenaClient.h"

extern "C" {
#include "atena/client.h"
#include "atena/path.h"
#include "atena/status.h"
}

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QMetaObject>
#include <QThread>

#ifndef ATENA_PACKAGE_REVISION
#define ATENA_PACKAGE_REVISION "development"
#endif

namespace AtenaUi {
namespace {

QVariantMap jsonMap(const char *raw)
{
    return QJsonDocument::fromJson(QByteArray(raw ? raw : "{}")).object().toVariantMap();
}

QVariantList jsonList(const char *raw)
{
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(raw ? raw : "[]"));
    return document.isArray() ? document.array().toVariantList() : QVariantList{};
}

QByteArray modelListParams()
{
    return QJsonDocument(QJsonObject{{QStringLiteral("provider_id"), QStringLiteral("ollama")}})
        .toJson(QJsonDocument::Compact);
}

QString friendlyConnectionDetail(AtenaStatus status)
{
    switch (status) {
        case ATENA_OK:
            return QStringLiteral("Atena Core conectado");
        case ATENA_ERR_PATH:
#ifdef Q_OS_WIN
            return QStringLiteral("Core não encontrado. Verifique se atena-core.exe está instalado ao lado do atena-ui.exe.");
#else
            return QStringLiteral("Core não encontrado. Verifique a instalação em /usr/libexec/atena/atena-core.");
#endif
        case ATENA_ERR_PERMISSION:
            return QStringLiteral("Sem permissão para executar o Atena Core.");
        case ATENA_ERR_DB:
            return QStringLiteral("O Core não conseguiu abrir o banco local.");
        case ATENA_ERR_SCHEMA_TOO_NEW:
            return QStringLiteral("O banco local pertence a uma versão mais nova da Atena.");
        case ATENA_ERR_TIMEOUT:
            return QStringLiteral("O Core foi iniciado, mas não respondeu dentro do tempo esperado.");
        case ATENA_ERR_NETWORK:
#ifdef Q_OS_WIN
            return QStringLiteral("O canal IPC do Core (Named Pipe) não respondeu. Abra Diagnóstico para consultar o log de inicialização.");
#else
            return QStringLiteral("O socket IPC do Core não respondeu. Abra Diagnóstico para consultar o log de inicialização.");
#endif
        default:
            return QStringLiteral("Falha ao iniciar ou conectar ao Core: %1")
                .arg(QString::fromUtf8(atena_status_string(status)));
    }
}

} // namespace

RealAtenaClient::RealAtenaClient(QObject *parent)
    : AtenaClientFacade(parent)
{
}

RealAtenaClient::~RealAtenaClient()
{
    QMutexLocker lock(&m_mutex);
    atena_client_close(m_client);
    m_client = nullptr;
}

AtenaClient *RealAtenaClient::client()
{
    QMutexLocker lock(&m_mutex);
    return m_client;
}

void RealAtenaClient::rememberConnectionResult(int status, const QString &detail)
{
    QMutexLocker lock(&m_mutex);
    m_lastConnectionStatus = status;
    m_lastConnectionDetail = detail;
}

void RealAtenaClient::connectToCore()
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_connecting) return;
        m_connecting = true;
        if (m_client) {
            atena_client_close(m_client);
            m_client = nullptr;
        }
    }

    Q_EMIT connectionStateChanged(ConnectionState::Connecting,
                                  QStringLiteral("Iniciando e validando o Atena Core…"));

    auto *thread = QThread::create([this] {
        AtenaClientConfig config{};
        config.connect_timeout_ms = 2500;
        config.request_timeout_ms = 600000;

        AtenaClient *created = nullptr;
        const AtenaStatus status = atena_client_connect_or_start(&config, &created);
        const QString detail = friendlyConnectionDetail(status);

        {
            QMutexLocker lock(&m_mutex);
            m_connecting = false;
            m_lastConnectionStatus = static_cast<int>(status);
            m_lastConnectionDetail = detail;
            if (status == ATENA_OK) m_client = created;
        }

        if (status == ATENA_OK) {
            Q_EMIT connectionStateChanged(ConnectionState::Connected,
                                          QStringLiteral("Atena Core 0.5.0-base conectado"));
        } else {
            if (created) atena_client_close(created);
            Q_EMIT connectionStateChanged(ConnectionState::Failed, detail);
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::callMap(const char *method, const char *params,
                              void (RealAtenaClient::*signal)(const QVariantMap &))
{
    auto *thread = QThread::create([this,
                                    method = QByteArray(method),
                                    params = QByteArray(params),
                                    signal] {
        AtenaClient *c = client();
        if (!c) {
            Q_EMIT clientError(QStringLiteral("NOT_CONFIGURED"),
                               QStringLiteral("Core não conectado."));
            return;
        }

        char *raw = nullptr;
        const AtenaStatus status = atena_client_call(c, method.constData(), params.constData(), &raw);
        if (status == ATENA_OK) {
            const QVariantMap map = jsonMap(raw);
            atena_client_free_string(raw);
            Q_EMIT (this->*signal)(map);
        } else {
            if (raw) atena_client_free_string(raw);
            Q_EMIT clientError(QStringLiteral("CORE_ERROR"),
                               QString::fromUtf8(atena_status_string(status)));
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::requestStatus()
{
    callMap("system.status", "{}", &RealAtenaClient::statusReady);
}

QVariantMap RealAtenaClient::localDiagnostics() const
{
    QVariantMap result;
    result.insert(QStringLiteral("ui_version"), QStringLiteral("0.5.0-base"));
    result.insert(QStringLiteral("package_revision"), QStringLiteral(ATENA_PACKAGE_REVISION));

    {
        QMutexLocker lock(&m_mutex);
        result.insert(QStringLiteral("connection_status_code"), m_lastConnectionStatus);
        result.insert(QStringLiteral("connection_detail"), m_lastConnectionDetail);
        result.insert(QStringLiteral("connected"), m_client != nullptr);
    }

    AtenaPaths paths{};
    const AtenaStatus pathStatus = atena_paths_resolve(&paths);
    result.insert(QStringLiteral("paths_status"), QString::fromUtf8(atena_status_string(pathStatus)));
    if (pathStatus != ATENA_OK) return result;

    const QString corePath = QString::fromUtf8(paths.core_executable);
    const QString identityPath = QString::fromUtf8(paths.identity_dir);
    const QString endpoint = QString::fromUtf8(paths.endpoint);
    const QString dataDir = QString::fromUtf8(paths.data_dir);
    const QString logPath = dataDir + QStringLiteral("/core.log");

    const QFileInfo coreInfo(corePath);
    const QFileInfo identityInfo(identityPath);
    result.insert(QStringLiteral("core_executable"), corePath);
    result.insert(QStringLiteral("core_exists"), coreInfo.exists());
    result.insert(QStringLiteral("core_executable_permission"), coreInfo.isExecutable());
    result.insert(QStringLiteral("identity_dir"), identityPath);
    result.insert(QStringLiteral("identity_exists"), identityInfo.exists() && identityInfo.isDir());
    result.insert(QStringLiteral("ipc_endpoint"), endpoint);
    result.insert(QStringLiteral("core_log"), logPath);

    QFile log(logPath);
    if (log.open(QIODevice::ReadOnly)) {
        constexpr qint64 maxTail = 12 * 1024;
        if (log.size() > maxTail) log.seek(log.size() - maxTail);
        result.insert(QStringLiteral("core_log_tail"), QString::fromUtf8(log.readAll()));
    } else {
        result.insert(QStringLiteral("core_log_tail"), QStringLiteral("Log ainda não criado."));
    }

    return result;
}

void RealAtenaClient::requestDoctor()
{
    auto *thread = QThread::create([this] {
        QVariantMap diagnostic = localDiagnostics();
        AtenaClient *c = client();
        if (!c) {
            diagnostic.insert(QStringLiteral("core"), QStringLiteral("offline"));
            Q_EMIT doctorReady(diagnostic);
            return;
        }

        char *raw = nullptr;
        const AtenaStatus status = atena_client_doctor(c, &raw);
        if (status == ATENA_OK) {
            diagnostic.insert(QStringLiteral("core"), jsonMap(raw));
            atena_client_free_string(raw);
        } else {
            if (raw) atena_client_free_string(raw);
            diagnostic.insert(QStringLiteral("core"), QStringLiteral("falha na consulta"));
            diagnostic.insert(QStringLiteral("doctor_error"),
                              QString::fromUtf8(atena_status_string(status)));
        }
        Q_EMIT doctorReady(diagnostic);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

struct ChatContext {
    RealAtenaClient *self;
    QString session;
    QString operation;
};

static int chatEvent(const AtenaStreamEvent *event, void *userdata)
{
    auto *ctx = static_cast<ChatContext *>(userdata);
    const QString op = QString::fromUtf8(event->operation_id ? event->operation_id : "");
    if (ctx->operation.isEmpty() && !op.isEmpty()) {
        ctx->operation = op;
        Q_EMIT ctx->self->chatAccepted(op, ctx->session);
    }
    if (event->type == ATENA_EVENT_TEXT_DELTA && event->text)
        Q_EMIT ctx->self->chatDelta(op, QString::fromUtf8(event->text));
    return 0;
}

void RealAtenaClient::startChat(const QString &text, const QString &sessionId)
{
    auto *thread = QThread::create([this, text, sessionId] {
        AtenaClient *c = client();
        if (!c) {
            Q_EMIT clientError(QStringLiteral("NOT_CONFIGURED"), QStringLiteral("Core não conectado."));
            return;
        }

        char sid[37] = {0};
        QString effective = sessionId;
        if (effective.isEmpty()) {
            const AtenaStatus sessionStatus = atena_client_session_create(c, "Conversa Qt", sid);
            if (sessionStatus != ATENA_OK) {
                Q_EMIT clientError(QStringLiteral("SESSION_ERROR"),
                                   QString::fromUtf8(atena_status_string(sessionStatus)));
                return;
            }
            effective = QString::fromLatin1(sid);
        }

        ChatContext ctx{this, effective, {}};
        char operation[37] = {0};
        const QByteArray input = text.toUtf8();
        const QByteArray session = effective.toUtf8();
        const AtenaStatus status = atena_client_chat_send(
            c, session.constData(), "", input.constData(), chatEvent, &ctx, operation);

        const QString op = ctx.operation.isEmpty() ? QString::fromLatin1(operation) : ctx.operation;
        if (status == ATENA_OK) {
            Q_EMIT operationFinished(op, QStringLiteral("completed"), QString());
        } else {
            Q_EMIT operationFinished(
                op,
                status == ATENA_ERR_CANCELLED ? QStringLiteral("cancelled") : QStringLiteral("failed"),
                QString::fromUtf8(atena_status_string(status)));
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::cancelOperation(const QString &id)
{
    AtenaClient *c = client();
    if (c) atena_client_cancel(c, id.toUtf8().constData());
}

void RealAtenaClient::requestProviders()
{
    auto *thread = QThread::create([this] {
        AtenaClient *c = client();
        if (!c) {
            Q_EMIT clientError(QStringLiteral("NOT_CONFIGURED"), QStringLiteral("Core não conectado."));
            return;
        }
        char *raw = nullptr;
        const AtenaStatus status = atena_client_call(c, "providers.list", "{}", &raw);
        if (status == ATENA_OK) {
            const QVariantList list = jsonList(raw);
            atena_client_free_string(raw);
            Q_EMIT providersReady(list);
        } else {
            if (raw) atena_client_free_string(raw);
            Q_EMIT clientError(QStringLiteral("CORE_ERROR"),
                               QString::fromUtf8(atena_status_string(status)));
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::requestTools()
{
    auto *thread = QThread::create([this] {
        AtenaClient *c = client();
        if (!c) {
            Q_EMIT clientError(QStringLiteral("NOT_CONFIGURED"), QStringLiteral("Core não conectado."));
            return;
        }
        char *raw = nullptr;
        const AtenaStatus status = atena_client_call(c, "tools.list", "{}", &raw);
        if (status == ATENA_OK) {
            const QVariantList list = jsonList(raw);
            atena_client_free_string(raw);
            Q_EMIT toolsReady(list);
        } else {
            if (raw) atena_client_free_string(raw);
            Q_EMIT clientError(QStringLiteral("CORE_ERROR"),
                               QString::fromUtf8(atena_status_string(status)));
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::unavailable(const QString &action)
{
    Q_EMIT clientError(QStringLiteral("FEATURE_UNAVAILABLE"),
                       QStringLiteral("%1 ainda não está disponível nesta baseline.").arg(action));
}

void RealAtenaClient::requestModels()
{
    auto *thread = QThread::create([this] {
        AtenaClient *c = client();
        if (!c) {
            Q_EMIT clientError(QStringLiteral("NOT_CONFIGURED"), QStringLiteral("Core não conectado."));
            return;
        }
        const QByteArray params = modelListParams();
        char *raw = nullptr;
        const AtenaStatus status = atena_client_call(c, "models.list", params.constData(), &raw);
        if (status == ATENA_OK) {
            const QVariantList list = jsonList(raw);
            atena_client_free_string(raw);
            Q_EMIT modelsReady(list);
        } else {
            if (raw) atena_client_free_string(raw);
            Q_EMIT clientError(QStringLiteral("MODELS_ERROR"),
                               QString::fromUtf8(atena_status_string(status)));
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::selectModel(const QString &providerId, const QString &modelId)
{
    auto *thread = QThread::create([this, providerId, modelId] {
        AtenaClient *c = client();
        if (!c) return;
        const QJsonObject object{{QStringLiteral("provider_id"), providerId.isEmpty() ? QStringLiteral("ollama") : providerId},
                                 {QStringLiteral("model"), modelId}};
        const QByteArray params = QJsonDocument(object).toJson(QJsonDocument::Compact);
        char *raw = nullptr;
        const AtenaStatus status = atena_client_call(c, "models.select", params.constData(), &raw);
        if (raw) atena_client_free_string(raw);
        if (status == ATENA_OK) {
            char *listRaw = nullptr;
            const QByteArray listParams = modelListParams();
            const AtenaStatus listStatus = atena_client_call(c, "models.list", listParams.constData(), &listRaw);
            if (listStatus == ATENA_OK) {
                const QVariantList list = jsonList(listRaw);
                atena_client_free_string(listRaw);
                Q_EMIT modelsReady(list);
            } else if (listRaw) atena_client_free_string(listRaw);
        } else {
            Q_EMIT clientError(QStringLiteral("MODEL_SELECT_ERROR"),
                               QString::fromUtf8(atena_status_string(status)));
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::pullModel(const QString &providerId, const QString &modelId)
{
    auto *thread = QThread::create([this, providerId, modelId] {
        AtenaClient *c = client();
        if (!c) return;
        const QJsonObject object{{QStringLiteral("provider_id"), providerId.isEmpty() ? QStringLiteral("ollama") : providerId},
                                 {QStringLiteral("model"), modelId}};
        const QByteArray params = QJsonDocument(object).toJson(QJsonDocument::Compact);
        Q_EMIT modelProgress(QString(), QStringLiteral("Baixando %1").arg(modelId), 0, 0);
        char *raw = nullptr;
        const AtenaStatus status = atena_client_call(c, "models.pull", params.constData(), &raw);
        if (raw) atena_client_free_string(raw);
        if (status == ATENA_OK) {
            Q_EMIT modelProgress(QString(), QStringLiteral("Modelo %1 instalado").arg(modelId), 1, 1);
            QMetaObject::invokeMethod(this, [this] { requestModels(); }, Qt::QueuedConnection);
        } else {
            Q_EMIT clientError(QStringLiteral("MODEL_PULL_ERROR"),
                               QString::fromUtf8(atena_status_string(status)));
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::removeModel(const QString &providerId, const QString &modelId)
{
    auto *thread = QThread::create([this, providerId, modelId] {
        AtenaClient *c = client();
        if (!c) return;
        const QJsonObject object{{QStringLiteral("provider_id"), providerId.isEmpty() ? QStringLiteral("ollama") : providerId},
                                 {QStringLiteral("model"), modelId}};
        const QByteArray params = QJsonDocument(object).toJson(QJsonDocument::Compact);
        char *raw = nullptr;
        const AtenaStatus status = atena_client_call(c, "models.remove", params.constData(), &raw);
        if (raw) atena_client_free_string(raw);
        if (status == ATENA_OK) QMetaObject::invokeMethod(this, [this] { requestModels(); }, Qt::QueuedConnection);
        else Q_EMIT clientError(QStringLiteral("MODEL_REMOVE_ERROR"),
                                QString::fromUtf8(atena_status_string(status)));
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::configureProvider(const QString &presetId, const QVariantMap &configuration)
{
    auto *thread = QThread::create([this, presetId, configuration] {
        AtenaClient *c = client();
        if (!c) {
            Q_EMIT clientError(QStringLiteral("NOT_CONFIGURED"), QStringLiteral("Core não conectado."));
            return;
        }

        QVariantMap values = configuration;
        values.insert(QStringLiteral("provider_id"), presetId);
        values.insert(QStringLiteral("preset_id"), presetId);
        values.insert(QStringLiteral("type"), presetId);
        const QByteArray params = QJsonDocument(QJsonObject::fromVariantMap(values)).toJson(QJsonDocument::Compact);

        char *raw = nullptr;
        const AtenaStatus status = atena_client_call(c, "providers.put", params.constData(), &raw);
        if (raw) atena_client_free_string(raw);
        if (status != ATENA_OK) {
            Q_EMIT clientError(QStringLiteral("PROVIDER_CONFIG_ERROR"),
                               QString::fromUtf8(atena_status_string(status)));
            return;
        }

        char *providersRaw = nullptr;
        const AtenaStatus providersStatus = atena_client_call(c, "providers.list", "{}", &providersRaw);
        if (providersStatus == ATENA_OK) {
            const QVariantList list = jsonList(providersRaw);
            atena_client_free_string(providersRaw);
            Q_EMIT providersReady(list);
        } else if (providersRaw) atena_client_free_string(providersRaw);

        const QByteArray testParams = QJsonDocument(
            QJsonObject{{QStringLiteral("provider_id"), presetId}}).toJson(QJsonDocument::Compact);
        char *testRaw = nullptr;
        const AtenaStatus testStatus = atena_client_call(c, "providers.test", testParams.constData(), &testRaw);
        if (testStatus == ATENA_OK) {
            const QVariantMap result = jsonMap(testRaw);
            atena_client_free_string(testRaw);
            Q_EMIT providerTestFinished(
                presetId, true,
                result.value(QStringLiteral("message"), QStringLiteral("Provider disponível")).toString());
        } else {
            if (testRaw) atena_client_free_string(testRaw);
            Q_EMIT providerTestFinished(presetId, false,
                                        QString::fromUtf8(atena_status_string(testStatus)));
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::testProvider(const QString &providerId)
{
    auto *thread = QThread::create([this, providerId] {
        AtenaClient *c = client();
        if (!c) {
            Q_EMIT providerTestFinished(providerId, false, QStringLiteral("Core desconectado"));
            return;
        }
        const QByteArray params = QJsonDocument(
            QJsonObject{{QStringLiteral("provider_id"), providerId}}).toJson(QJsonDocument::Compact);
        char *raw = nullptr;
        const AtenaStatus status = atena_client_call(c, "providers.test", params.constData(), &raw);
        if (status == ATENA_OK) {
            const QVariantMap result = jsonMap(raw);
            atena_client_free_string(raw);
            Q_EMIT providerTestFinished(
                providerId, true,
                result.value(QStringLiteral("message"), QStringLiteral("Provider disponível")).toString());
        } else {
            if (raw) atena_client_free_string(raw);
            Q_EMIT providerTestFinished(providerId, false,
                                        QString::fromUtf8(atena_status_string(status)));
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::ingestDocument(const QString &path)
{
    auto *thread = QThread::create([this, path] {
        AtenaClient *c = client();
        if (!c) {
            Q_EMIT clientError(QStringLiteral("NOT_CONFIGURED"), QStringLiteral("Core não conectado."));
            return;
        }

        const QFileInfo info(path);
        const QString suffix = info.suffix().toLower();
        if (suffix != QStringLiteral("txt") && suffix != QStringLiteral("md") && suffix != QStringLiteral("markdown")) {
            Q_EMIT clientError(QStringLiteral("DOCUMENT_FORMAT"),
                               QStringLiteral("Nesta revisão, a ingestão real aceita TXT e Markdown. PDF ainda requer parser próprio no Core."));
            return;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            Q_EMIT clientError(QStringLiteral("DOCUMENT_IO"), QStringLiteral("Não foi possível ler o documento."));
            return;
        }
        constexpr qint64 maxBytes = 8 * 1024 * 1024;
        if (file.size() > maxBytes) {
            Q_EMIT clientError(QStringLiteral("DOCUMENT_TOO_LARGE"),
                               QStringLiteral("Documento maior que 8 MiB. Divida-o antes da ingestão."));
            return;
        }
        const QByteArray bytes = file.readAll();
        const QString text = QString::fromUtf8(bytes);
        if (text.trimmed().isEmpty()) {
            Q_EMIT clientError(QStringLiteral("DOCUMENT_EMPTY"), QStringLiteral("O documento está vazio."));
            return;
        }

        Q_EMIT documentProgress(QString(), QStringLiteral("Indexando %1").arg(info.fileName()), 0, 1);
        const QJsonObject object{
            {QStringLiteral("title"), info.fileName()},
            {QStringLiteral("locator"), info.absoluteFilePath()},
            {QStringLiteral("text"), text}
        };
        const QByteArray params = QJsonDocument(object).toJson(QJsonDocument::Compact);
        char *raw = nullptr;
        const AtenaStatus status = atena_client_call(c, "rag.ingest", params.constData(), &raw);
        if (raw) atena_client_free_string(raw);
        if (status == ATENA_OK) {
            Q_EMIT documentProgress(QString(), QStringLiteral("Documento indexado"), 1, 1);
            QMetaObject::invokeMethod(this, [this] { requestDocuments(); }, Qt::QueuedConnection);
        } else {
            Q_EMIT clientError(QStringLiteral("DOCUMENT_INGEST_ERROR"),
                               QString::fromUtf8(atena_status_string(status)));
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::requestDocuments()
{
    auto *thread = QThread::create([this] {
        AtenaClient *c = client();
        if (!c) {
            Q_EMIT clientError(QStringLiteral("NOT_CONFIGURED"), QStringLiteral("Core não conectado."));
            return;
        }
        char *raw = nullptr;
        const AtenaStatus status = atena_client_call(c, "documents.list", "{}", &raw);
        if (status == ATENA_OK) {
            const QVariantList list = jsonList(raw);
            atena_client_free_string(raw);
            Q_EMIT documentsReady(list);
        } else {
            if (raw) atena_client_free_string(raw);
            Q_EMIT clientError(QStringLiteral("DOCUMENT_LIST_ERROR"),
                               QString::fromUtf8(atena_status_string(status)));
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void RealAtenaClient::confirmTool(const QString &, bool)
{
    unavailable(QStringLiteral("Confirmação de ferramenta"));
}

} // namespace AtenaUi
