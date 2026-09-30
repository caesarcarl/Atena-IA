#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace AtenaUi {

class AtenaClientFacade : public QObject {
    Q_OBJECT
public:
    enum class ConnectionState { Disconnected, Connecting, Connected, Unavailable, Failed };
    Q_ENUM(ConnectionState)

    explicit AtenaClientFacade(QObject *parent = nullptr) : QObject(parent) {}
    ~AtenaClientFacade() override = default;

    virtual void connectToCore() = 0;
    virtual void requestStatus() = 0;
    virtual void startChat(const QString &text, const QString &sessionId = {}) = 0;
    virtual void cancelOperation(const QString &operationId) = 0;
    virtual void requestModels() = 0;
    virtual void selectModel(const QString &providerId, const QString &modelId) = 0;
    virtual void pullModel(const QString &providerId, const QString &modelId) = 0;
    virtual void removeModel(const QString &providerId, const QString &modelId) = 0;
    virtual void requestProviders() = 0;
    virtual void configureProvider(const QString &presetId, const QVariantMap &configuration) = 0;
    virtual void testProvider(const QString &providerId) = 0;
    virtual void ingestDocument(const QString &path) = 0;
    virtual void requestDocuments() = 0;
    virtual void requestTools() = 0;
    virtual void confirmTool(const QString &confirmationId, bool allowOnce) = 0;
    virtual void requestDoctor() = 0;

Q_SIGNALS:
    void connectionStateChanged(AtenaUi::AtenaClientFacade::ConnectionState state, const QString &detail);
    void statusReady(const QVariantMap &status);
    void chatAccepted(const QString &operationId, const QString &sessionId);
    void chatDelta(const QString &operationId, const QString &text);
    void operationFinished(const QString &operationId, const QString &status, const QString &message);
    void modelsReady(const QVariantList &models);
    void modelProgress(const QString &operationId, const QString &label, qint64 completed, qint64 total);
    void providersReady(const QVariantList &providers);
    void providerTestFinished(const QString &providerId, bool ok, const QString &message);
    void documentsReady(const QVariantList &documents);
    void documentProgress(const QString &operationId, const QString &label, qint64 completed, qint64 total);
    void toolsReady(const QVariantList &tools);
    void toolConfirmationRequested(const QVariantMap &request);
    void doctorReady(const QVariantMap &doctor);
    void clientError(const QString &code, const QString &message);
};

} // namespace AtenaUi
