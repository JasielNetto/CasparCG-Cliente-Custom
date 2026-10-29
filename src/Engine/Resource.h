#pragma once

// Recursos lógicos de playout (RF-05) e seu vínculo configurável com servidor/canal/layer.
// Sem dependência de Qt: o núcleo é compilado e testado isoladamente.

#include <array>
#include <map>
#include <optional>
#include <string>

namespace engine
{
    enum class LogicalResource
    {
        Vt,
        Selo1,
        Selo2
    };

    constexpr std::array<LogicalResource, 3> AllResources = { LogicalResource::Vt, LogicalResource::Selo1, LogicalResource::Selo2 };

    const char* toString(LogicalResource resource);

    // Destino físico de um recurso lógico. "server" é o nome do dispositivo cadastrado no cliente.
    struct Endpoint
    {
        std::string server;
        int channel = 1;
        int layer = 10;

        bool operator==(const Endpoint& other) const
        {
            return server == other.server && channel == other.channel && layer == other.layer;
        }
    };

    // Mapa recurso lógico -> destino. É carregado da configuração da estação, nunca fixado no código.
    class ResourceMap
    {
    public:
        void set(LogicalResource resource, const Endpoint& endpoint);
        void remove(LogicalResource resource);
        std::optional<Endpoint> find(LogicalResource resource) const;

    private:
        std::map<LogicalResource, Endpoint> endpoints;
    };
}
