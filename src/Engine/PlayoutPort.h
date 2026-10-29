#pragma once

// Porta de saída do núcleo para o playout. O núcleo não conhece AMCP, versão do CasparCG nem
// redundância: quem implementa esta interface (adaptador AMCP, Playout Router, servidor simulado)
// decide como os comandos chegam aos servidores.
//
// Toda operação é assíncrona e termina chamando a Completion exatamente uma vez.
// A Completion pode ser chamada dentro da própria chamada (síncrona) ou depois (resposta do servidor).

#include "Item.h"
#include "Resource.h"

#include <functional>
#include <string>

namespace engine
{
    struct CommandResult
    {
        bool ok = true;
        std::string error;

        static CommandResult success() { return {}; }
        static CommandResult failure(std::string message) { return { false, std::move(message) }; }
    };

    using Completion = std::function<void(const CommandResult&)>;

    class PlayoutPort
    {
    public:
        virtual ~PlayoutPort() = default;

        // Prepara o item no destino sem colocá-lo no ar (ex.: LOADBG).
        virtual void prepare(const Endpoint& endpoint, const Item& item, Completion done) = 0;

        // Coloca no ar o que foi preparado no destino (ex.: PLAY sem argumentos).
        virtual void take(const Endpoint& endpoint, Completion done) = 0;

        // Tira do ar o que estiver no destino (ex.: STOP).
        virtual void stop(const Endpoint& endpoint, Completion done) = 0;
    };
}
