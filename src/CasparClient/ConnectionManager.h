#pragma once

// Conexões da estação: um CasparDevice por servidor da configuração, o listener OSC e a lista de
// mídia (CLS) de cada servidor. A reconexão automática é a do próprio AmcpDevice (a cada 5 s).

#include "StationConfig.h"

#include "Models/CasparMedia.h"

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>

class CasparDevice;
class OscMonitorListener;

class ConnectionManager : public QObject
{
    Q_OBJECT

    public:
        explicit ConnectionManager(QObject* parent = nullptr);
        ~ConnectionManager() override;

        // Cria, recria ou remove conexões conforme a configuração. Servidores sem mudança são mantidos.
        void apply(const engine::StationConfig& config);

        QSharedPointer<CasparDevice> device(const QString& name) const;
        QStringList serverNames() const;
        bool isConnected(const QString& name) const;
        QString hostAddress(const QString& name) const;

        QObject* oscSource() const;
        int oscPort() const { return this->currentOscPort; }

        QList<CasparMedia> media(const QString& server) const;
        void refreshMedia(const QString& server);

        Q_SIGNAL void serversChanged();
        Q_SIGNAL void connectionChanged(const QString& name, bool connected);
        Q_SIGNAL void mediaChanged(const QString& name);
        Q_SIGNAL void oscSourceChanged();

    private:
        QMap<QString, QSharedPointer<CasparDevice>> devices;
        QMap<QString, engine::ServerConfig> configs;
        QMap<QString, QList<CasparMedia>> mediaLists;
        OscMonitorListener* osc = nullptr;
        int currentOscPort = 0;

        QString nameOf(const CasparDevice* device) const;

        Q_SLOT void deviceConnectionChanged(CasparDevice& device);
        Q_SLOT void deviceMediaChanged(const QList<CasparMedia>& media, CasparDevice& device);
};
