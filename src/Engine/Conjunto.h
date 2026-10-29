#pragma once

// Conjunto (RF-04): disparo simultâneo de itens em recursos distintos, como uma única operação lógica.
//
// Estratégia de disparo em duas fases (o AMCP não tem transação):
//   1. Preparação: prepare() em todos os itens. Se qualquer um falhar, nada vai ao ar e o
//      disparo termina como Aborted (os preparados com sucesso não são tirados do background,
//      pois LOADBG não afeta o ar).
//   2. Take: take() em todos, em rajada. Uma falha aqui é falha parcial: o relatório identifica
//      quais itens entraram e quais não entraram. Não há rollback automático (decisão pendente).

#include "Item.h"
#include "PlayoutPort.h"
#include "Resource.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace engine
{
    enum class ValidationError
    {
        Empty,              // Conjunto sem itens
        DuplicateResource,  // dois itens no mesmo recurso lógico
        UnmappedResource    // recurso sem destino configurado
    };

    const char* toString(ValidationError error);

    struct ValidationIssue
    {
        ValidationError error;
        std::string itemId;  // vazio quando o erro é do Conjunto como um todo
        LogicalResource resource = LogicalResource::Vt;
    };

    struct ValidationResult
    {
        std::vector<ValidationIssue> issues;
        bool ok() const { return issues.empty(); }
    };

    struct Conjunto
    {
        std::string id;
        std::vector<Item> items;
    };

    ValidationResult validate(const Conjunto& conjunto, const ResourceMap& map);

    enum class ItemOutcome
    {
        NotAttempted,  // não chegou à fase em que falharia (ex.: Conjunto abortado na preparação)
        PrepareFailed,
        TakeFailed,
        OnAir
    };

    const char* toString(ItemOutcome outcome);

    enum class FireStatus
    {
        Rejected,  // bloqueado pela validação; nenhum comando enviado
        Aborted,   // falha na preparação; nada foi ao ar
        Partial,   // parte dos itens foi ao ar
        Failed,    // nenhum item foi ao ar na fase de take
        Success
    };

    const char* toString(FireStatus status);

    struct ItemReport
    {
        std::string itemId;
        LogicalResource resource = LogicalResource::Vt;
        ItemOutcome outcome = ItemOutcome::NotAttempted;
        std::string error;
    };

    struct FireReport
    {
        std::string conjuntoId;
        FireStatus status = FireStatus::Rejected;
        ValidationResult validation;
        std::vector<ItemReport> items;
    };

    using FireCallback = std::function<void(const FireReport&)>;
    using BeforeTake = std::function<void()>;

    // Dispara um Conjunto. O callback é chamado exatamente uma vez, quando todas as respostas chegarem.
    // O estado do disparo é mantido internamente, então o chamador não precisa manter nada vivo.
    // 'beforeTake' (opcional) é chamado uma vez, só se todas as preparações deram certo, imediatamente
    // antes dos takes: é o ponto para tirar o controle de quem usava os recursos (ex.: Sequência).
    void fire(const Conjunto& conjunto, const ResourceMap& map, PlayoutPort& port, FireCallback done, BeforeTake beforeTake = {});
}
