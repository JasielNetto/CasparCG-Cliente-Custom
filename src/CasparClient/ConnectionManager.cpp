#include "ConnectionManager.h"

#include "CasparDevice.h"
#include "OscMonitorListener.h"

#include <QtNetwork/QHostAddress>

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
        this->resolvedHosts.remove(name);
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

        // Endereço IP literal não precisa de resolução (que bloquearia a interface).
        const QString host = QString::fromStdString(it->host);
        if (!QHostAddress(host).isNull())
            this->resolvedHosts.insert(it.key(), host);

        device->connectDevice();
    }

    // OSC: o listener só é recriado quando a porta muda.
    if (this->osc == nullptr || config.oscPort != this->currentOscPort)
    {
        delete this->osc;
        this->osc = new OscMonitorListener(this);
        this->oscListening = this->osc->start(config.oscPort);
        this->currentOscPort = config.oscPort;
        emit oscSourceChanged();
        if (!this->oscListening)
            emit oscError(tr("Não foi possível abrir a porta OSC %1 (já está em uso?). Sem OSC, não há tempo "
                             "nos painéis nem avanço automático das Sequências.").arg(config.oscPort));
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
    // Nomes de host são resolvidos uma vez, ao conectar (ver deviceConnectionChanged).
    return this->resolvedHosts.value(name);
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
    {
        // Conectado, a resolução já está no cache do sistema e é imediata.
        if (!this->resolvedHosts.contains(name))
            this->resolvedHosts.insert(name, device.resolveIpAddress());
        device.refreshMedia();
    }
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
