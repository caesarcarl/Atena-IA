#pragma once
#include "AtenaClientFacade.h"

extern "C" { struct AtenaClient; struct AtenaStreamEvent; }

#include <QMutex>
#include <QString>

namespace AtenaUi {
class RealAtenaClient final : public AtenaClientFacade {
    Q_OBJECT
public:
    explicit RealAtenaClient(QObject *parent = nullptr);
    ~RealAtenaClient() override;

    void connectToCore() override;
    void requestStatus() override;
    void startChat(const QString &text, const QString &sessionId) override;
    void cancelOperation(const QString &operationId) override;
    void requestModels() override;
    void selectModel(const QString &providerId, const QString &modelId) override;
    void pullModel(const QString &providerId, const QString &modelId) override;
    void removeModel(const QString &providerId, const QString &modelId) override;
    void requestProviders() override;
    void configureProvider(const QString &presetId, const QVariantMap &configuration) override;
    void testProvider(const QString &providerId) override;
    void ingestDocument(const QString &path) override;
    void requestDocuments() override;
    void requestTools() override;
    void confirmTool(const QString &confirmationId, bool allowOnce) override;
    void requestDoctor() override;

private:
    AtenaClient *client();
    void callMap(const char *method, const char *params,
                 void (RealAtenaClient::*signal)(const QVariantMap &));
    void unavailable(const QString &action);
    QVariantMap localDiagnostics() const;
    void rememberConnectionResult(int status, const QString &detail);

    AtenaClient *m_client = nullptr;
    mutable QMutex m_mutex;
    bool m_connecting = false;
    int m_lastConnectionStatus = 0;
    QString m_lastConnectionDetail;
};
}
