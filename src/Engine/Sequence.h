#pragma once

// Sequência (RF-04): execução ordenada de itens, uma vez ou em loop.
//
// Avanço dirigido pelo cliente, com pré-carga: enquanto um item está no ar, o próximo já é
// preparado (LOADBG); quando o fim do item é informado (onItemFinished, vindo do OSC), o próximo
// entra com take(). Avanço pelo próprio servidor (LOADBG ... AUTO) fica para quando o adaptador
// AMCP expuser essa capacidade.
//
// Interrupção x parada:
//  - stop(): o operador encerra a sequência; o item no ar é tirado do ar.
//  - interrupt(): outra execução assumiu o recurso; nada é enviado, quem assumiu manda no ar.

#include "Item.h"
#include "PlayoutPort.h"
#include "Resource.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace engine
{
    enum class RepeatMode
    {
        Once,
        Loop
    };

    struct Sequence
    {
        std::string id;
        std::vector<Item> items;
        RepeatMode repeat = RepeatMode::Once;
    };

    enum class SequenceState
    {
        Idle,
        Starting,     // primeiro item sendo preparado/disparado
        Running,
        Finished,     // modo Once: o último item terminou
        Stopped,      // parada pelo operador
        Interrupted,  // substituída por outra execução
        Error
    };

    const char* toString(SequenceState state);

    struct SequenceEvent
    {
        SequenceState state = SequenceState::Idle;
        int index = -1;      // item corrente (-1 se nenhum)
        std::string itemId;
        std::string error;
        int failedIndex = -1;      // em Error: o item cuja preparação/take falhou (pode diferir de index)
        std::string failedItemId;
    };

    using SequenceListener = std::function<void(const SequenceEvent&)>;

    class SequenceRunner
    {
    public:
        SequenceRunner(Sequence sequence, const ResourceMap& map, PlayoutPort& port, SequenceListener listener = {});
        ~SequenceRunner();

        SequenceRunner(const SequenceRunner&) = delete;
        SequenceRunner& operator=(const SequenceRunner&) = delete;

        // Retorna false (e não envia nada) se a sequência for vazia, tiver recurso sem destino
        // ou já estiver em execução.
        bool start();
        void stop();
        void interrupt();

        // Fim de reprodução informado pelo playout. Eventos de itens que não são o corrente são ignorados.
        void onItemFinished(const std::string& itemId);

        SequenceState state() const { return currentState; }
        int currentIndex() const { return index; }
        const Sequence& definition() const { return sequence; }

    private:
        enum class Preload { None, Pending, Ready, Failed };

        bool isActive() const;
        int nextIndex() const;
        void goTo(int target);
        void takePrepared(int target);
        void preloadNext();
        void fail(const std::string& error, int failedIndex);
        void publish(const std::string& error = {}, int failedIndex = -1);
        Endpoint endpointOf(int i) const;

        Sequence sequence;
        const ResourceMap& map;
        PlayoutPort& port;
        SequenceListener listener;

        SequenceState currentState = SequenceState::Idle;
        int index = -1;

        int preloadIndex = -1;
        Preload preload = Preload::None;
        bool takeWhenReady = false;
        bool advancing = false;  // transição em curso: novos eventos de fim são ignorados

        // Respostas que chegam depois de stop/interrupt/destruição são descartadas.
        unsigned generation = 0;
        std::shared_ptr<int> alive = std::make_shared<int>(0);
    };
}
