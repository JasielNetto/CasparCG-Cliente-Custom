#include "StationStore.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QObject>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>

namespace StationStore
{
    QString configPath()
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        return QDir(dir).filePath("station.json");
    }

    QString resourceKey(engine::LogicalResource resource)
    {
        switch (resource)
        {
            case engine::LogicalResource::Vt: return "vt";
            case engine::LogicalResource::Selo1: return "selo1";
            case engine::LogicalResource::Selo2: return "selo2";
        }
        return QString();
    }

    engine::StationConfig defaults()
    {
        engine::StationConfig config;
        config.servers.push_back({ "Playout 1", "127.0.0.1", 5250 });
        config.resources.set(engine::LogicalResource::Vt, { "Playout 1", 1, 10 });
        config.resources.set(engine::LogicalResource::Selo1, { "Playout 1", 1, 20 });
        config.resources.set(engine::LogicalResource::Selo2, { "Playout 1", 1, 30 });
        return config;
    }

    QJsonObject toJson(const engine::StationConfig& config)
    {
        QJsonArray servers;
        for (const engine::ServerConfig& server : config.servers)
        {
            QJsonObject item;
            item["name"] = QString::fromStdString(server.name);
            item["host"] = QString::fromStdString(server.host);
            item["port"] = server.port;
            servers.append(item);
        }

        QJsonObject resources;
        for (const auto& [resource, endpoint] : config.resources.all())
        {
            QJsonObject item;
            item["server"] = QString::fromStdString(endpoint.server);
            item["channel"] = endpoint.channel;
            item["layer"] = endpoint.layer;
            resources[resourceKey(resource)] = item;
        }

        QJsonObject root;
        root["version"] = 1;
        root["oscPort"] = config.oscPort;
        root["servers"] = servers;
        root["resources"] = resources;
        return root;
    }

    bool fromJson(const QJsonObject& json, engine::StationConfig& config, QString* error)
    {
        if (json.value("version").toInt() != 1)
        {
            if (error != nullptr)
                *error = QObject::tr("Versão de configuração não suportada");
            return false;
        }

        engine::StationConfig result;
        result.oscPort = json.value("oscPort").toInt(engine::StationConfig::DefaultOscPort);

        for (const QJsonValue& value : json.value("servers").toArray())
        {
            const QJsonObject item = value.toObject();
            engine::ServerConfig server;
            server.name = item.value("name").toString().toStdString();
            server.host = item.value("host").toString().toStdString();
            server.port = item.value("port").toInt(5250);
            result.servers.push_back(server);
        }

        const QJsonObject resources = json.value("resources").toObject();
        for (engine::LogicalResource resource : engine::AllResources)
        {
            const QString key = resourceKey(resource);
            if (!resources.contains(key))
                continue;
            const QJsonObject item = resources.value(key).toObject();
            engine::Endpoint endpoint;
            endpoint.server = item.value("server").toString().toStdString();
            endpoint.channel = item.value("channel").toInt(1);
            endpoint.layer = item.value("layer").toInt(10);
            result.resources.set(resource, endpoint);
        }

        config = result;
        return true;
    }

    bool load(engine::StationConfig& config, QString* error)
    {
        QFile file(configPath());
        if (!file.exists())
        {
            config = defaults();
            return true;
        }
        if (!file.open(QIODevice::ReadOnly))
        {
            if (error != nullptr)
                *error = QObject::tr("Não foi possível abrir %1: %2").arg(file.fileName(), file.errorString());
            return false;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            if (error != nullptr)
                *error = QObject::tr("Arquivo %1 inválido: %2").arg(file.fileName(), parseError.errorString());
            return false;
        }
        return fromJson(document.object(), config, error);
    }

    bool save(const engine::StationConfig& config, QString* error)
    {
        const QString path = configPath();
        QDir().mkpath(QFileInfo(path).absolutePath());

        // QSaveFile grava em arquivo temporário e troca no fim: um travamento não corrompe a configuração.
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly))
        {
            if (error != nullptr)
                *error = QObject::tr("Não foi possível gravar %1: %2").arg(path, file.errorString());
            return false;
        }
        file.write(QJsonDocument(toJson(config)).toJson(QJsonDocument::Indented));
        if (!file.commit())
        {
            if (error != nullptr)
                *error = QObject::tr("Não foi possível gravar %1: %2").arg(path, file.errorString());
            return false;
        }
        return true;
    }
}
