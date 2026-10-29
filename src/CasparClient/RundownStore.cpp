#include "RundownStore.h"

#include "StationStore.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QObject>
#include <QtCore/QSaveFile>

namespace
{
    QString kindKey(engine::NodeKind kind)
    {
        switch (kind)
        {
            case engine::NodeKind::Item: return "item";
            case engine::NodeKind::Bloco: return "bloco";
            case engine::NodeKind::Sequencia: return "sequencia";
            case engine::NodeKind::Conjunto: return "conjunto";
        }
        return QString();
    }

    bool kindFromKey(const QString& key, engine::NodeKind& kind)
    {
        if (key == "item") kind = engine::NodeKind::Item;
        else if (key == "bloco") kind = engine::NodeKind::Bloco;
        else if (key == "sequencia") kind = engine::NodeKind::Sequencia;
        else if (key == "conjunto") kind = engine::NodeKind::Conjunto;
        else return false;
        return true;
    }

    bool resourceFromKey(const QString& key, engine::LogicalResource& resource)
    {
        for (engine::LogicalResource candidate : engine::AllResources)
        {
            if (StationStore::resourceKey(candidate) == key)
            {
                resource = candidate;
                return true;
            }
        }
        return false;
    }

    QJsonObject nodeToJson(const engine::Rundown& rundown, engine::NodeId id)
    {
        const engine::Node* node = rundown.find(id);
        QJsonObject json;
        json["kind"] = kindKey(node->kind);
        json["label"] = QString::fromStdString(node->label);
        if (!node->externalId.empty())
            json["externalId"] = QString::fromStdString(node->externalId);

        if (node->kind == engine::NodeKind::Item)
        {
            json["media"] = QString::fromStdString(node->item.media);
            json["resource"] = StationStore::resourceKey(node->item.resource);
            json["loop"] = node->item.loop;
        }
        if (node->kind == engine::NodeKind::Sequencia)
            json["repeat"] = node->repeat == engine::RepeatMode::Loop ? "loop" : "once";

        if (!node->children.empty())
        {
            QJsonArray children;
            for (engine::NodeId child : node->children)
                children.append(nodeToJson(rundown, child));
            json["children"] = children;
        }
        return json;
    }

    bool nodeFromJson(const QJsonObject& json, engine::Rundown& rundown, engine::NodeId parent, QString* error)
    {
        engine::NodeKind kind;
        if (!kindFromKey(json.value("kind").toString(), kind))
        {
            if (error != nullptr)
                *error = QObject::tr("Tipo de item desconhecido: %1").arg(json.value("kind").toString());
            return false;
        }

        const std::string label = json.value("label").toString().toStdString();
        std::optional<engine::NodeId> id;
        if (kind == engine::NodeKind::Item)
        {
            engine::Item item;
            item.media = json.value("media").toString().toStdString();
            item.loop = json.value("loop").toBool(false);
            if (!resourceFromKey(json.value("resource").toString("vt"), item.resource))
                item.resource = engine::LogicalResource::Vt;
            id = rundown.addItem(parent, -1, item, label);
        }
        else
        {
            id = rundown.addGroup(kind, parent, -1, label);
        }

        if (!id)
        {
            if (error != nullptr)
                *error = QObject::tr("Estrutura inválida perto de \"%1\"").arg(QString::fromStdString(label));
            return false;
        }

        if (json.contains("externalId"))
            rundown.setExternalId(*id, json.value("externalId").toString().toStdString());
        if (kind == engine::NodeKind::Sequencia)
            rundown.setRepeat(*id, json.value("repeat").toString() == "loop" ? engine::RepeatMode::Loop : engine::RepeatMode::Once);

        for (const QJsonValue& child : json.value("children").toArray())
            if (!nodeFromJson(child.toObject(), rundown, *id, error))
                return false;
        return true;
    }
}

namespace RundownStore
{
    QJsonObject toJson(const engine::Rundown& rundown)
    {
        QJsonArray items;
        for (engine::NodeId child : rundown.root().children)
            items.append(nodeToJson(rundown, child));

        QJsonObject root;
        root["version"] = 1;
        root["items"] = items;
        return root;
    }

    bool fromJson(const QJsonObject& json, engine::Rundown& rundown, QString* error)
    {
        if (json.value("version").toInt() != 1)
        {
            if (error != nullptr)
                *error = QObject::tr("Versão de rundown não suportada");
            return false;
        }

        engine::Rundown result;
        for (const QJsonValue& value : json.value("items").toArray())
            if (!nodeFromJson(value.toObject(), result, engine::RootId, error))
                return false;

        rundown = result;
        return true;
    }

    bool saveFile(const QString& path, const engine::Rundown& rundown, QString* error)
    {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly))
        {
            if (error != nullptr)
                *error = file.errorString();
            return false;
        }
        file.write(QJsonDocument(toJson(rundown)).toJson(QJsonDocument::Indented));
        if (!file.commit())
        {
            if (error != nullptr)
                *error = file.errorString();
            return false;
        }
        return true;
    }

    bool loadFile(const QString& path, engine::Rundown& rundown, QString* error)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            if (error != nullptr)
                *error = file.errorString();
            return false;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            if (error != nullptr)
                *error = parseError.errorString();
            return false;
        }
        return fromJson(document.object(), rundown, error);
    }
}
