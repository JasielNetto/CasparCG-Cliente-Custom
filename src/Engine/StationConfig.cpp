#include "StationConfig.h"

#include <set>
#include <tuple>

namespace engine
{
    const char* toString(ConfigError error)
    {
        switch (error)
        {
            case ConfigError::EmptyServerName: return "Servidor sem nome";
            case ConfigError::DuplicateServerName: return "Nome de servidor repetido";
            case ConfigError::EmptyHost: return "Servidor sem endereço";
            case ConfigError::InvalidPort: return "Porta AMCP inválida";
            case ConfigError::UnknownServer: return "Recurso aponta para servidor não cadastrado";
            case ConfigError::InvalidChannel: return "Canal inválido";
            case ConfigError::InvalidLayer: return "Layer inválida";
            case ConfigError::SharedEndpoint: return "Dois recursos no mesmo servidor, canal e layer";
            case ConfigError::InvalidOscPort: return "Porta OSC inválida";
        }
        return "?";
    }

    const ServerConfig* StationConfig::findServer(const std::string& name) const
    {
        for (const ServerConfig& server : servers)
            if (server.name == name)
                return &server;
        return nullptr;
    }

    std::vector<ConfigIssue> StationConfig::validate() const
    {
        std::vector<ConfigIssue> issues;

        std::set<std::string> names;
        for (const ServerConfig& server : servers)
        {
            if (server.name.empty())
                issues.push_back({ ConfigError::EmptyServerName, server.host });
            else if (!names.insert(server.name).second)
                issues.push_back({ ConfigError::DuplicateServerName, server.name });
            if (server.host.empty())
                issues.push_back({ ConfigError::EmptyHost, server.name });
            if (server.port < 1 || server.port > 65535)
                issues.push_back({ ConfigError::InvalidPort, server.name });
        }

        std::set<std::tuple<std::string, int, int>> endpoints;
        for (const auto& [resource, endpoint] : resources.all())
        {
            const std::string subject = toString(resource);
            if (findServer(endpoint.server) == nullptr)
                issues.push_back({ ConfigError::UnknownServer, subject });
            if (endpoint.channel < 1)
                issues.push_back({ ConfigError::InvalidChannel, subject });
            if (endpoint.layer < 0)
                issues.push_back({ ConfigError::InvalidLayer, subject });
            if (!endpoints.insert({ endpoint.server, endpoint.channel, endpoint.layer }).second)
                issues.push_back({ ConfigError::SharedEndpoint, subject });
        }

        if (oscPort < 1 || oscPort > 65535)
            issues.push_back({ ConfigError::InvalidOscPort, std::to_string(oscPort) });

        return issues;
    }
}
