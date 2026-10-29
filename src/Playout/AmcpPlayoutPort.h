#pragma once

// Adaptador AMCP do nucleo de execucao (src/Engine): implementa engine::PlayoutPort sobre
// CasparDevice. Quem cria a porta informa como achar o dispositivo pelo nome (DeviceResolver):
// o cliente antigo usa o DeviceManager; o CasparClient usa a sua propria configuracao.
//
// Cada comando sai como "REQ <id> <comando>" e so e concluido quando chega "RES <id> ...",
// quando o tempo limite expira ou quando o servidor desconecta. Assim o nucleo recebe sucesso ou
// falha reais, nunca uma falha mascarada de sucesso.
//
// Uso somente na thread da interface, como o restante do envio de comandos.
// Requer CasparCG Server 2.2+ (REQ/RES). Validado para 2.4.3.

#include "PlayoutPort.h"

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>

#include <functional>

class CasparDevice;
class QTimer;

class AmcpPlayoutPort : public QObject, public engine::PlayoutPort
{
    Q_OBJECT

    public:
        static constexpr int DefaultTimeoutMs = 2000;

        using DeviceResolver = std::function<QSharedPointer<CasparDevice>(const QString& name)>;

        explicit AmcpPlayoutPort(DeviceResolver resolver, QObject* parent = nullptr);
        ~AmcpPlayoutPort() override;

        void setTimeout(int milliseconds);

        void prepare(const engine::Endpoint& endpoint, const engine::Item& item, engine::Completion done) override;
        void take(const engine::Endpoint& endpoint, engine::Completion done) override;
        void stop(const engine::Endpoint& endpoint, engine::Completion done) override;

        // Comando AMCP avulso (ex.: PAUSE, RESUME, CLEAR), com a mesma correlacao REQ/RES.
        void sendCommand(const QString& server, const QString& command, engine::Completion done);

        int pendingCount() const;

    private:
        struct Pending
        {
            engine::Completion done;
            QPointer<CasparDevice> device;
            QTimer* timer = nullptr;
        };

        DeviceResolver resolver;
        int timeoutMs = DefaultTimeoutMs;
        quint64 nextId = 0;
        QHash<QString, Pending> pending;
        QSet<CasparDevice*> watched;

        void send(const engine::Endpoint& endpoint, const QString& command, engine::Completion done);
        void send(const QString& server, const QString& command, engine::Completion done);
        void watch(CasparDevice* device);
        void complete(const QString& id, const engine::CommandResult& result);
        void failAllFor(const CasparDevice* device, const QString& reason);

        Q_SLOT void requestCompleted(const QString& id, int code, const QList<QString>& response);
        Q_SLOT void connectionStateChanged(CasparDevice& device);
};
