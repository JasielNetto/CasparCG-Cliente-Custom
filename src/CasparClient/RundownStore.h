#pragma once

// Gravação e leitura do Rundown em arquivo JSON (.ccrundown). Só a estrutura é salva:
// estados operacionais (no ar, erro) começam zerados ao abrir.

#include "Rundown.h"

#include <QtCore/QJsonObject>
#include <QtCore/QString>

namespace RundownStore
{
    QJsonObject toJson(const engine::Rundown& rundown);
    bool fromJson(const QJsonObject& json, engine::Rundown& rundown, QString* error);

    bool saveFile(const QString& path, const engine::Rundown& rundown, QString* error);
    bool loadFile(const QString& path, engine::Rundown& rundown, QString* error);
}
