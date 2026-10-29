#include "ConnectionManager.h"

#include "CasparDevice.h"
#include "OscMonitorListener.h"

ConnectionManager::ConnectionManager(QObject* parent)
    : QObject(parent)
{
}

ConnectionManager::~ConnectionManager()
{
    for (const QSharedPointer<CasparDevice>& device : this->devices)
    {
        device->disconnect(this);
        device->disconnectDevice();
    }
    this->devices.clear();
}

void ConnectionManager::apply(const engine::StationConfig& config)
{
    QMap<QString, engine::ServerConfig> wanted;
    for (const engine::ServerConfig& server : config.servers)
        wanted.insert(QString::fromStdString(server.name), server);

    // Remove servidores que saíram ou mudaram de endereço.
    const QStringList current = this->devices.keys();
    for (const QString& name : current)
    {
        const auto it = wanted.constFind(name);
        const engine::ServerConfig& old = this->configs[name];
        const bool unchanged = it != wanted.cend() && it->host == old.host && it->port == old.port;
        if (unchanged)
            continue;

        QSharedPointer<CasparDevice> device = this->devices.take(name);
        device->disconnect(this);
        device->disconnectDevice();
        this->configs.remove(name);
        this->mediaLists.remove(name);
    }

    // Cria os novos.
    for (auto it = wanted.cbegin(); it != wanted.cend(); ++it)
    {
        if (this->devices.contains(it.key()))
            continue;

        QSharedPointer<CasparDevice> device(new CasparDevice(QString::fromStdString(it->host), it->port));
        QObject::connect(device.data(), &CasparDevice::connectionStateChanged, this, &ConnectionManager::deviceConnectionChanged);
        QObject::connect(device.data(), &CasparDevice::mediaChanged, this, &ConnectionManager::deviceMediaChanged);
        this->devices.insert(it.key(), device);
        this->configs.insert(it.key(), it.value());
        device->connectDevice();
    }

    // OSC: o listener só é recriado quando a porta muda.
    if (this->osc == nullptr || config.oscPort != this->currentOscPort)
    {
        delete this->osc;
        this->osc = new OscMonitorListener(this);
        this->osc->start(config.oscPort);
        this->currentOscPort = config.oscPort;
        emit oscSourceChanged();
    }

    emit serversChanged();
}

QSharedPointer<CasparDevice> ConnectionManager::device(const QString& name) const
{
    return this->devices.value(name);
}

QStringList ConnectionManager::serverNames() const
{
    return this->devices.keys();
}

bool ConnectionManager::isConnected(const QString& name) const
{
    const QSharedPointer<CasparDevice> device = this->devices.value(name);
    return !device.isNull() && device->isConnected();
}

QString ConnectionManager::hostAddress(const QString& name) const
{
    const QSharedPointer<CasparDevice> device = this->devices.value(name);
    return device.isNull() ? QString() : device->resolveIpAddress();
}

QObject* ConnectionManager::oscSource() const
{
    return this->osc;
}

QList<CasparMedia> ConnectionManager::media(const QString& server) const
{
    return this->mediaLists.value(server);
}

void ConnectionManager::refreshMedia(const QString& server)
{
    const QSharedPointer<CasparDevice> device = this->devices.value(server);
    if (!device.isNull() && device->isConnected())
        device->refreshMedia();
}

QString ConnectionManager::nameOf(const CasparDevice* device) const
{
    for (auto it = this->devices.cbegin(); it != this->devices.cend(); ++it)
        if (it.value().data() == device)
            return it.key();
    return QString();
}

void ConnectionManager::deviceConnectionChanged(CasparDevice& device)
{
    const QString name = nameOf(&device);
    if (name.isEmpty())
        return;

    if (device.isConnected())
        device.refreshMedia();
    emit connectionChanged(name, device.isConnected());
}

void ConnectionManager::deviceMediaChanged(const QList<CasparMedia>& media, CasparDevice& device)
{
    const QString name = nameOf(&device);
    if (name.isEmpty())
        return;

    this->mediaLists.insert(name, media);
    emit mediaChanged(name);
}
