#pragma once

// Leitura e gravação da configuração da estação (station.json) na pasta de configuração do usuário.
// Arquivo ausente não é erro: a estação começa com a configuração padrão.

#include "StationConfig.h"

#include <QtCore/QJsonObject>
#include <QtCore/QString>

namespace StationStore
{
    QString configPath();

    engine::StationConfig defaults();

    // Retorna false e preenche 'error' se o arquivo existir mas não puder ser lido.
    bool load(engine::StationConfig& config, QString* error);
    bool save(const engine::StationConfig& config, QString* error);

    QJsonObject toJson(const engine::StationConfig& config);
    bool fromJson(const QJsonObject& json, engine::StationConfig& config, QString* error);

    QString resourceKey(engine::LogicalResource resource);  // "vt", "selo1", "selo2"
}
