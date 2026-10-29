#include "Rundown.h"

#include <algorithm>

namespace engine
{
    const char* toString(NodeKind kind)
    {
        switch (kind)
        {
            case NodeKind::Item: return "Item";
            case NodeKind::Bloco: return "Bloco";
            case NodeKind::Sequencia: return "Sequência";
            case NodeKind::Conjunto: return "Conjunto";
        }
        return "?";
    }

    const char* toString(NodeState state)
    {
        switch (state)
        {
            case NodeState::Idle: return "";
            case NodeState::Preparing: return "Preparando";
            case NodeState::Prepared: return "Preparado";
            case NodeState::OnAir: return "No ar";
            case NodeState::Finished: return "Concluído";
            case NodeState::Error: return "Erro";
        }
        return "?";
    }

    Rundown::Rundown()
    {
        Node root;
        root.id = RootId;
        root.kind = NodeKind::Bloco;  // não usado: a raiz é tratada à parte
        nodes.emplace(RootId, root);
    }

    bool Rundown::canContain(std::optional<NodeKind> parent, NodeKind child)
    {
        if (!parent)
            return true;
        switch (*parent)
        {
            case NodeKind::Bloco: return child != NodeKind::Bloco;
            case NodeKind::Sequencia:
            case NodeKind::Conjunto: return child == NodeKind::Item;
            case NodeKind::Item: return false;
        }
        return false;
    }

    Node* Rundown::get(NodeId id)
    {
        const auto it = nodes.find(id);
        return it == nodes.end() ? nullptr : &it->second;
    }

    const Node* Rundown::find(NodeId id) const
    {
        if (id == RootId)
            return nullptr;
        const auto it = nodes.find(id);
        return it == nodes.end() ? nullptr : &it->second;
    }

    std::optional<NodeKind> Rundown::kindForParent(NodeId parent) const
    {
        if (parent == RootId)
            return std::nullopt;
        return nodes.at(parent).kind;
    }

    void Rundown::insertAt(std::vector<NodeId>& list, int index, NodeId id)
    {
        if (index < 0 || index >= static_cast<int>(list.size()))
            list.push_back(id);
        else
            list.insert(list.begin() + index, id);
    }

    std::optional<NodeId> Rundown::addItem(NodeId parent, int index, const Item& item, const std::string& label)
    {
        if (nodes.count(parent) == 0 || !canContain(kindForParent(parent), NodeKind::Item))
            return std::nullopt;

        Node node;
        node.id = nextId++;
        node.kind = NodeKind::Item;
        node.label = label;
        node.item = item;
        node.item.id = engineId(node.id);
        node.parent = parent;
        nodes.emplace(node.id, node);
        insertAt(nodes.at(parent).children, index, node.id);
        return node.id;
    }

    std::optional<NodeId> Rundown::addGroup(NodeKind kind, NodeId parent, int index, const std::string& label)
    {
        if (kind == NodeKind::Item || nodes.count(parent) == 0 || !canContain(kindForParent(parent), kind))
            return std::nullopt;

        Node node;
        node.id = nextId++;
        node.kind = kind;
        node.label = label;
        node.parent = parent;
        nodes.emplace(node.id, node);
        insertAt(nodes.at(parent).children, index, node.id);
        return node.id;
    }

    void Rundown::erase(NodeId id)
    {
        const std::vector<NodeId> children = nodes.at(id).children;
        for (NodeId child : children)
            erase(child);
        nodes.erase(id);
    }

    bool Rundown::remove(NodeId id)
    {
        Node* node = get(id);
        if (id == RootId || node == nullptr)
            return false;

        auto& siblings = nodes.at(node->parent).children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
        erase(id);
        return true;
    }

    bool Rundown::isDescendant(NodeId candidate, NodeId ancestor) const
    {
        NodeId current = candidate;
        while (current != RootId)
        {
            if (current == ancestor)
                return true;
            current = nodes.at(current).parent;
        }
        return false;
    }

    bool Rundown::canMove(NodeId id, NodeId newParent) const
    {
        const Node* node = find(id);
        if (node == nullptr || nodes.count(newParent) == 0)
            return false;
        if (isDescendant(newParent, id))  // inclui newParent == id
            return false;
        return canContain(kindForParent(newParent), node->kind);
    }

    bool Rundown::canAdd(NodeId parent, NodeKind kind) const
    {
        return nodes.count(parent) > 0 && canContain(kindForParent(parent), kind);
    }

    bool Rundown::move(NodeId id, NodeId newParent, int index)
    {
        if (!canMove(id, newParent))
            return false;
        Node* node = get(id);

        auto& oldSiblings = nodes.at(node->parent).children;
        oldSiblings.erase(std::remove(oldSiblings.begin(), oldSiblings.end(), id), oldSiblings.end());
        node->parent = newParent;
        insertAt(nodes.at(newParent).children, index, id);  // index é a posição final
        return true;
    }

    bool Rundown::setLabel(NodeId id, const std::string& label)
    {
        Node* node = get(id);
        if (id == RootId || node == nullptr)
            return false;
        node->label = label;
        return true;
    }

    bool Rundown::setItem(NodeId id, const Item& item)
    {
        Node* node = get(id);
        if (id == RootId || node == nullptr || node->kind != NodeKind::Item)
            return false;
        node->item = item;
        node->item.id = engineId(id);
        return true;
    }

    bool Rundown::setRepeat(NodeId id, RepeatMode repeat)
    {
        Node* node = get(id);
        if (id == RootId || node == nullptr || node->kind != NodeKind::Sequencia)
            return false;
        node->repeat = repeat;
        return true;
    }

    bool Rundown::setExternalId(NodeId id, const std::string& externalId)
    {
        Node* node = get(id);
        if (id == RootId || node == nullptr)
            return false;
        node->externalId = externalId;
        return true;
    }

    bool Rundown::setState(NodeId id, NodeState state, const std::string& error)
    {
        Node* node = get(id);
        if (id == RootId || node == nullptr)
            return false;
        node->state = state;
        node->error = state == NodeState::Error ? error : std::string();
        return true;
    }

    void Rundown::resetStates()
    {
        for (auto& [id, node] : nodes)
        {
            node.state = NodeState::Idle;
            node.error.clear();
        }
    }

    int Rundown::indexOf(NodeId id) const
    {
        const Node* node = find(id);
        if (node == nullptr)
            return -1;
        const auto& siblings = nodes.at(node->parent).children;
        const auto it = std::find(siblings.begin(), siblings.end(), id);
        return it == siblings.end() ? -1 : static_cast<int>(it - siblings.begin());
    }

    void Rundown::collect(NodeId id, std::vector<NodeId>& out) const
    {
        for (NodeId child : nodes.at(id).children)
        {
            out.push_back(child);
            collect(child, out);
        }
    }

    std::vector<NodeId> Rundown::flatten() const
    {
        std::vector<NodeId> out;
        collect(RootId, out);
        return out;
    }

    std::vector<NodeId> Rundown::itemsOf(NodeId id) const
    {
        std::vector<NodeId> out;
        const auto it = nodes.find(id);
        if (it == nodes.end())
            return out;
        if (id != RootId && it->second.kind == NodeKind::Item)
        {
            out.push_back(id);
            return out;
        }
        std::vector<NodeId> all;
        collect(id, all);
        for (NodeId candidate : all)
            if (nodes.at(candidate).kind == NodeKind::Item)
                out.push_back(candidate);
        return out;
    }

    std::optional<Sequence> Rundown::toSequence(NodeId id) const
    {
        const Node* node = find(id);
        if (node == nullptr || node->kind != NodeKind::Sequencia)
            return std::nullopt;

        Sequence sequence;
        sequence.id = engineId(id);
        sequence.repeat = node->repeat;
        for (NodeId child : node->children)
            sequence.items.push_back(nodes.at(child).item);
        return sequence;
    }

    std::optional<Conjunto> Rundown::toConjunto(NodeId id) const
    {
        const Node* node = find(id);
        if (node == nullptr || node->kind != NodeKind::Conjunto)
            return std::nullopt;

        Conjunto conjunto;
        conjunto.id = engineId(id);
        for (NodeId child : node->children)
            conjunto.items.push_back(nodes.at(child).item);
        return conjunto;
    }

    std::optional<NodeId> Rundown::fromEngineId(const std::string& id)
    {
        if (id.empty() || id.size() > 19)
            return std::nullopt;
        NodeId value = 0;
        for (char c : id)
        {
            if (c < '0' || c > '9')
                return std::nullopt;
            value = value * 10 + static_cast<NodeId>(c - '0');
        }
        return value;
    }
}
