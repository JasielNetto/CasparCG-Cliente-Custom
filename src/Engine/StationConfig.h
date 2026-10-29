#pragma once

// Configuração da estação (RF-05, princípio 6): servidores CasparCG, vínculo dos recursos lógicos
// com servidor/canal/layer e porta OSC. Sem Qt; a leitura/gravação em arquivo fica na aplicação.

#include "Resource.h"

#include <string>
#include <vector>

namespace engine
{
    struct ServerConfig
    {
        std::string name;  // nome usado nos vínculos dos recursos
        std::string host;
        int port = 5250;
    };

    enum class ConfigError
    {
        EmptyServerName,
        DuplicateServerName,
        EmptyHost,
        InvalidPort,
        UnknownServer,       // recurso aponta para servidor não cadastrado
        InvalidChannel,
        InvalidLayer,
        SharedEndpoint,      // dois recursos no mesmo servidor/canal/layer
        InvalidOscPort
    };

    const char* toString(ConfigError error);

    struct ConfigIssue
    {
        ConfigError error;
        std::string subject;  // servidor ou recurso afetado
    };

    struct StationConfig
    {
        static constexpr int DefaultOscPort = 6250;

        std::vector<ServerConfig> servers;
        ResourceMap resources;
        int oscPort = DefaultOscPort;

        const ServerConfig* findServer(const std::string& name) const;
        std::vector<ConfigIssue> validate() const;
    };
}
