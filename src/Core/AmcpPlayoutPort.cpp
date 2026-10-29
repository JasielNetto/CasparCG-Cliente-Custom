#include "AmcpPlayoutPort.h"

#include "DeviceManager.h"

#include "AmcpFormat.h"
#include "CasparDevice.h"

#include <QtCore/QTimer>

#include <atomic>

namespace
{
    // Ids unicos no processo, mesmo com mais de uma porta ativa.
    std::atomic<quint64> requestCounter { 0 };

    engine::CommandResult failure(const QString& message)
    {
        return engine::CommandResult::failure(message.toStdString());
    }
}

AmcpPlayoutPort::AmcpPlayoutPort(QObject* parent)
    : QObject(parent)
{
}

AmcpPlayoutPort::~AmcpPlayoutPort()
{
    // Respostas pendentes sao descartadas: o nucleo ignora conclusoes que nunca chegam
    // (SequenceRunner descarta por geracao; um Conjunto sem resposta simplesmente nao reporta).
    this->pending.clear();
}

void AmcpPlayoutPort::setTimeout(int milliseconds)
{
    this->timeoutMs = milliseconds > 0 ? milliseconds : DefaultTimeoutMs;
}

int AmcpPlayoutPort::pendingCount() const
{
    return this->pending.count();
}

void AmcpPlayoutPort::prepare(const engine::Endpoint& endpoint, const engine::Item& item, engine::Completion done)
{
    send(endpoint, QString::fromStdString(engine::amcp::loadBg(endpoint, item)), std::move(done));
}

void AmcpPlayoutPort::take(const engine::Endpoint& endpoint, engine::Completion done)
{
    send(endpoint, QString::fromStdString(engine::amcp::play(endpoint)), std::move(done));
}

void AmcpPlayoutPort::stop(const engine::Endpoint& endpoint, engine::Completion done)
{
    send(endpoint, QString::fromStdString(engine::amcp::stop(endpoint)), std::move(done));
}

void AmcpPlayoutPort::send(const engine::Endpoint& endpoint, const QString& command, engine::Completion done)
{
    const QString server = QString::fromStdString(endpoint.server);
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(server);
    if (device.isNull())
    {
        done(failure(QString("Server '%1' is not configured").arg(server)));
        return;
    }
    if (!device->isConnected())
    {
        done(failure(QString("Server '%1' is disconnected").arg(server)));
        return;
    }

    watch(device.data());

    const QString id = QString("ce%1").arg(++requestCounter);

    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, this, [this, id, server]() {
        complete(id, failure(QString("No response from '%1' within %2 ms").arg(server).arg(this->timeoutMs)));
    });

    this->pending.insert(id, Pending { std::move(done), QPointer<CasparDevice>(device.data()), timer });

    if (!device->sendRequest(id, command))
    {
        complete(id, failure(QString("Server '%1' refused the command").arg(server)));
        return;
    }
    timer->start(this->timeoutMs);
}

void AmcpPlayoutPort::watch(CasparDevice* device)
{
    if (this->watched.contains(device))
        return;
    this->watched.insert(device);

    QObject::connect(device, &AmcpDevice::requestCompleted, this, &AmcpPlayoutPort::requestCompleted);
    QObject::connect(device, &CasparDevice::connectionStateChanged, this, &AmcpPlayoutPort::connectionStateChanged);
    QObject::connect(device, &QObject::destroyed, this, [this, device]() {
        this->watched.remove(device);
        failAllFor(device, "Server removed");
    });
}

void AmcpPlayoutPort::complete(const QString& id, const engine::CommandResult& result)
{
    auto it = this->pending.find(id);
    if (it == this->pending.end())
        return;

    // Remove antes de chamar: a conclusao pode disparar novos envios (ex.: take do Conjunto).
    Pending entry = std::move(it.value());
    this->pending.erase(it);

    if (entry.timer != nullptr)
    {
        entry.timer->stop();
        entry.timer->deleteLater();
    }

    if (entry.done)
        entry.done(result);
}

void AmcpPlayoutPort::failAllFor(const CasparDevice* device, const QString& reason)
{
    QList<QString> ids;
    for (auto it = this->pending.cbegin(); it != this->pending.cend(); ++it)
    {
        if (it.value().device.isNull() || it.value().device.data() == device)
            ids.append(it.key());
    }

    for (const QString& id : ids)
        complete(id, failure(reason));
}

void AmcpPlayoutPort::requestCompleted(const QString& id, int code, const QList<QString>& response)
{
    auto it = this->pending.constFind(id);
    if (it == this->pending.cend())
        return;  // resposta de outra porta ou ja concluida por tempo limite

    if (it.value().device.data() != qobject_cast<CasparDevice*>(sender()))
        return;

    if (code >= 200 && code < 300)
    {
        complete(id, engine::CommandResult::success());
        return;
    }

    const QString detail = response.isEmpty() ? QString::number(code) : response.join(" | ").trimmed();
    complete(id, failure(QString("Server replied %1").arg(detail)));
}

void AmcpPlayoutPort::connectionStateChanged(CasparDevice& device)
{
    if (!device.isConnected())
        failAllFor(&device, "Connection to the server was lost");
}
