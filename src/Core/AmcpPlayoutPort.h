#pragma once

// Adaptador AMCP do nucleo de execucao (src/Engine): implementa engine::PlayoutPort sobre os
// CasparDevice cadastrados no DeviceManager.
//
// Cada comando sai como "REQ <id> <comando>" e so e concluido quando chega "RES <id> ...",
// quando o tempo limite expira ou quando o servidor desconecta. Assim o nucleo recebe sucesso ou
// falha reais, nunca uma falha mascarada de sucesso.
//
// Uso somente na thread da interface, como o restante do envio de comandos.
// Requer CasparCG Server 2.2+ (REQ/RES). Validado para 2.4.3.

#include "Shared.h"

#include "PlayoutPort.h"

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QString>

class CasparDevice;
class QTimer;

class CORE_EXPORT AmcpPlayoutPort : public QObject, public engine::PlayoutPort
{
    Q_OBJECT

    public:
        static constexpr int DefaultTimeoutMs = 2000;

        explicit AmcpPlayoutPort(QObject* parent = nullptr);
        ~AmcpPlayoutPort() override;

        void setTimeout(int milliseconds);

        void prepare(const engine::Endpoint& endpoint, const engine::Item& item, engine::Completion done) override;
        void take(const engine::Endpoint& endpoint, engine::Completion done) override;
        void stop(const engine::Endpoint& endpoint, engine::Completion done) override;

        int pendingCount() const;

    private:
        struct Pending
        {
            engine::Completion done;
            QPointer<CasparDevice> device;
            QTimer* timer = nullptr;
        };

        int timeoutMs = DefaultTimeoutMs;
        quint64 nextId = 0;
        QHash<QString, Pending> pending;
        QSet<CasparDevice*> watched;

        void send(const engine::Endpoint& endpoint, const QString& command, engine::Completion done);
        void watch(CasparDevice* device);
        void complete(const QString& id, const engine::CommandResult& result);
        void failAllFor(const CasparDevice* device, const QString& reason);

        Q_SLOT void requestCompleted(const QString& id, int code, const QList<QString>& response);
        Q_SLOT void connectionStateChanged(CasparDevice& device);
};
