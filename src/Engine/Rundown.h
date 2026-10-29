#pragma once

// Modelo do Rundown (RF-03/RF-04), sem Qt.
//
// Árvore de nós com regras de composição:
//   - raiz: Item, Bloco, Sequência, Conjunto
//   - Bloco (organização visual): Item, Sequência, Conjunto — nunca outro Bloco
//   - Sequência e Conjunto: somente Item
//   - Item: sem filhos
//
// O estado operacional (preparado, no ar, erro) fica em cada nó, separado da seleção, que é da interface.
// Toda alteração passa por métodos que validam as regras e retornam false sem mudar nada se forem violadas.

#include "Conjunto.h"
#include "Item.h"
#include "Resource.h"
#include "Sequence.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace engine
{
    enum class NodeKind
    {
        Item,
        Bloco,
        Sequencia,
        Conjunto
    };

    const char* toString(NodeKind kind);

    enum class NodeState
    {
        Idle,
        Preparing,
        Prepared,
        OnAir,
        Finished,
        Error
    };

    const char* toString(NodeState state);

    using NodeId = std::uint64_t;
    constexpr NodeId RootId = 0;

    struct Node
    {
        NodeId id = RootId;
        NodeKind kind = NodeKind::Item;
        std::string label;
        std::string externalId;            // id em sistema externo (MOS); preservado, nunca gerado aqui
        Item item;                         // somente para NodeKind::Item
        RepeatMode repeat = RepeatMode::Once;  // somente para NodeKind::Sequencia
        NodeState state = NodeState::Idle;
        std::string error;                 // última falha, quando state == Error
        NodeId parent = RootId;
        std::vector<NodeId> children;
    };

    class Rundown
    {
    public:
        Rundown();

        static bool canContain(std::optional<NodeKind> parent, NodeKind child);  // nullopt = raiz

        // Inserção em 'index' (-1 ou além do fim = no fim). Retorna o id criado, ou vazio se inválido.
        std::optional<NodeId> addItem(NodeId parent, int index, const Item& item, const std::string& label);
        std::optional<NodeId> addGroup(NodeKind kind, NodeId parent, int index, const std::string& label);

        bool remove(NodeId id);  // remove o nó e tudo abaixo dele
        bool move(NodeId id, NodeId newParent, int index);
        bool canMove(NodeId id, NodeId newParent) const;  // mesmas regras de move(), sem alterar nada
        bool canAdd(NodeId parent, NodeKind kind) const;

        bool setLabel(NodeId id, const std::string& label);
        bool setItem(NodeId id, const Item& item);
        bool setRepeat(NodeId id, RepeatMode repeat);
        bool setExternalId(NodeId id, const std::string& externalId);
        bool setState(NodeId id, NodeState state, const std::string& error = {});
        void resetStates();

        const Node* find(NodeId id) const;
        const Node& root() const { return nodes.at(RootId); }
        int indexOf(NodeId id) const;  // posição dentro do pai, -1 se não existir
        std::size_t size() const { return nodes.size() - 1; }  // sem a raiz

        // Ordem de exibição (pré-ordem), sem a raiz.
        std::vector<NodeId> flatten() const;

        // Itens de uma Sequência ou Conjunto, já no formato do núcleo.
        std::optional<Sequence> toSequence(NodeId id) const;
        std::optional<Conjunto> toConjunto(NodeId id) const;

        // Itens executáveis dentro de 'id', em ordem (ele próprio, se for Item).
        std::vector<NodeId> itemsOf(NodeId id) const;

        // Id usado no núcleo para um nó (estável dentro do rundown).
        static std::string engineId(NodeId id) { return std::to_string(id); }
        static std::optional<NodeId> fromEngineId(const std::string& id);

    private:
        std::map<NodeId, Node> nodes;
        NodeId nextId = 1;

        Node* get(NodeId id);
        bool isDescendant(NodeId candidate, NodeId ancestor) const;
        std::optional<NodeKind> kindForParent(NodeId parent) const;
        void collect(NodeId id, std::vector<NodeId>& out) const;
        void erase(NodeId id);
        static void insertAt(std::vector<NodeId>& list, int index, NodeId id);
    };
}
