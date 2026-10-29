#pragma once

// Playout simulado para testar o núcleo sem servidor real (princípio 7).
// Registra cada comando, permite injetar falhas por operação/destino e, no modo adiado,
// segura as respostas até o teste liberá-las (simula latência e respostas fora de ordem).

#include "PlayoutPort.h"

#include <deque>
#include <set>
#include <string>
#include <tuple>
#include <vector>

class FakePlayoutPort : public engine::PlayoutPort
{
public:
    struct Call
    {
        std::string op;  // "prepare", "take", "stop"
        engine::Endpoint endpoint;
        std::string media;
    };

    bool deferred = false;
    std::vector<Call> calls;

    void failOn(const std::string& op, const std::string& server, int channel, int layer, std::string message = "falha simulada")
    {
        failures.insert({ op, server, channel, layer });
        failureMessage = std::move(message);
    }

    void clearFailures() { failures.clear(); }

    void prepare(const engine::Endpoint& endpoint, const engine::Item& item, engine::Completion done) override
    {
        handle("prepare", endpoint, item.media, std::move(done));
    }

    void take(const engine::Endpoint& endpoint, engine::Completion done) override
    {
        handle("take", endpoint, {}, std::move(done));
    }

    void stop(const engine::Endpoint& endpoint, engine::Completion done) override
    {
        handle("stop", endpoint, {}, std::move(done));
    }

    std::size_t pendingCount() const { return pending.size(); }

    // Libera a resposta mais antiga (ou a de índice informado, para simular respostas fora de ordem).
    void release(std::size_t position = 0)
    {
        auto it = pending.begin() + static_cast<std::ptrdiff_t>(position);
        auto entry = std::move(*it);
        pending.erase(it);
        entry.second(entry.first);
    }

    void releaseAll()
    {
        while (!pending.empty())
            release();
    }

    std::vector<std::string> ops() const
    {
        std::vector<std::string> result;
        for (const Call& call : calls)
            result.push_back(call.op + ":" + std::to_string(call.endpoint.channel) + "-" + std::to_string(call.endpoint.layer) + (call.media.empty() ? "" : ":" + call.media));
        return result;
    }

private:
    void handle(const std::string& op, const engine::Endpoint& endpoint, const std::string& media, engine::Completion done)
    {
        calls.push_back({ op, endpoint, media });
        const bool fails = failures.count({ op, endpoint.server, endpoint.channel, endpoint.layer }) > 0;
        const engine::CommandResult result = fails ? engine::CommandResult::failure(failureMessage) : engine::CommandResult::success();
        if (deferred)
            pending.emplace_back(result, std::move(done));
        else
            done(result);
    }

    std::set<std::tuple<std::string, std::string, int, int>> failures;
    std::string failureMessage;
    std::deque<std::pair<engine::CommandResult, engine::Completion>> pending;
};
