#include "Conjunto.h"

#include <set>

namespace engine
{
    const char* toString(ValidationError error)
    {
        switch (error)
        {
            case ValidationError::Empty: return "Conjunto vazio";
            case ValidationError::DuplicateResource: return "Recurso usado por mais de um item";
            case ValidationError::UnmappedResource: return "Recurso sem destino configurado";
        }
        return "?";
    }

    const char* toString(ItemOutcome outcome)
    {
        switch (outcome)
        {
            case ItemOutcome::NotAttempted: return "Não executado";
            case ItemOutcome::PrepareFailed: return "Falha na preparação";
            case ItemOutcome::TakeFailed: return "Falha no disparo";
            case ItemOutcome::OnAir: return "No ar";
        }
        return "?";
    }

    const char* toString(FireStatus status)
    {
        switch (status)
        {
            case FireStatus::Rejected: return "Bloqueado";
            case FireStatus::Aborted: return "Abortado na preparação";
            case FireStatus::Partial: return "Parcial";
            case FireStatus::Failed: return "Falhou";
            case FireStatus::Success: return "Sucesso";
        }
        return "?";
    }

    ValidationResult validate(const Conjunto& conjunto, const ResourceMap& map)
    {
        ValidationResult result;
        if (conjunto.items.empty())
        {
            result.issues.push_back({ ValidationError::Empty, {}, LogicalResource::Vt });
            return result;
        }

        std::set<LogicalResource> used;
        for (const Item& item : conjunto.items)
        {
            if (!used.insert(item.resource).second)
                result.issues.push_back({ ValidationError::DuplicateResource, item.id, item.resource });
            if (!map.find(item.resource))
                result.issues.push_back({ ValidationError::UnmappedResource, item.id, item.resource });
        }
        return result;
    }

    namespace
    {
        // Estado compartilhado entre as respostas assíncronas de um disparo.
        struct FireOperation
        {
            FireReport report;
            std::vector<Endpoint> endpoints;
            std::vector<Item> items;
            PlayoutPort* port = nullptr;
            FireCallback done;
            std::size_t pending = 0;
        };

        void finishTake(const std::shared_ptr<FireOperation>& op)
        {
            std::size_t onAir = 0;
            for (const ItemReport& item : op->report.items)
                if (item.outcome == ItemOutcome::OnAir)
                    ++onAir;

            if (onAir == op->report.items.size())
                op->report.status = FireStatus::Success;
            else if (onAir == 0)
                op->report.status = FireStatus::Failed;
            else
                op->report.status = FireStatus::Partial;

            op->done(op->report);
        }

        void startTake(const std::shared_ptr<FireOperation>& op)
        {
            op->pending = op->items.size();
            for (std::size_t i = 0; i < op->items.size(); ++i)
            {
                op->port->take(op->endpoints[i], [op, i](const CommandResult& result)
                {
                    ItemReport& item = op->report.items[i];
                    item.outcome = result.ok ? ItemOutcome::OnAir : ItemOutcome::TakeFailed;
                    item.error = result.error;
                    if (--op->pending == 0)
                        finishTake(op);
                });
            }
        }

        void finishPrepare(const std::shared_ptr<FireOperation>& op)
        {
            bool anyFailed = false;
            for (const ItemReport& item : op->report.items)
                if (item.outcome == ItemOutcome::PrepareFailed)
                    anyFailed = true;

            if (anyFailed)
            {
                op->report.status = FireStatus::Aborted;
                op->done(op->report);
                return;
            }
            startTake(op);
        }
    }

    void fire(const Conjunto& conjunto, const ResourceMap& map, PlayoutPort& port, FireCallback done)
    {
        auto op = std::make_shared<FireOperation>();
        op->report.conjuntoId = conjunto.id;
        op->report.validation = validate(conjunto, map);
        op->port = &port;
        op->done = std::move(done);

        for (const Item& item : conjunto.items)
            op->report.items.push_back({ item.id, item.resource, ItemOutcome::NotAttempted, {} });

        if (!op->report.validation.ok())
        {
            op->report.status = FireStatus::Rejected;
            op->done(op->report);
            return;
        }

        op->items = conjunto.items;
        for (const Item& item : conjunto.items)
            op->endpoints.push_back(*map.find(item.resource));

        // pending é definido antes de qualquer envio, pois respostas síncronas decrementam na hora.
        op->pending = op->items.size();
        for (std::size_t i = 0; i < op->items.size(); ++i)
        {
            port.prepare(op->endpoints[i], op->items[i], [op, i](const CommandResult& result)
            {
                if (!result.ok)
                {
                    op->report.items[i].outcome = ItemOutcome::PrepareFailed;
                    op->report.items[i].error = result.error;
                }
                if (--op->pending == 0)
                    finishPrepare(op);
            });
        }
    }
}
