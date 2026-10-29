#include "Sequence.h"

namespace engine
{
    const char* toString(SequenceState state)
    {
        switch (state)
        {
            case SequenceState::Idle: return "Parada";
            case SequenceState::Starting: return "Iniciando";
            case SequenceState::Running: return "Em execução";
            case SequenceState::Finished: return "Concluída";
            case SequenceState::Stopped: return "Interrompida pelo operador";
            case SequenceState::Interrupted: return "Substituída";
            case SequenceState::Error: return "Erro";
        }
        return "?";
    }

    SequenceRunner::SequenceRunner(Sequence sequence, const ResourceMap& map, PlayoutPort& port, SequenceListener listener)
        : sequence(std::move(sequence)), map(map), port(port), listener(std::move(listener))
    {
    }

    SequenceRunner::~SequenceRunner()
    {
        alive.reset();
    }

    bool SequenceRunner::isActive() const
    {
        return currentState == SequenceState::Starting || currentState == SequenceState::Running;
    }

    Endpoint SequenceRunner::endpointOf(int i) const
    {
        return *map.find(sequence.items[static_cast<std::size_t>(i)].resource);
    }

    int SequenceRunner::nextIndex() const
    {
        const int next = index + 1;
        if (next < static_cast<int>(sequence.items.size()))
            return next;
        return sequence.repeat == RepeatMode::Loop ? 0 : -1;
    }

    bool SequenceRunner::start()
    {
        if (isActive() || sequence.items.empty())
            return false;
        for (const Item& item : sequence.items)
            if (!map.find(item.resource))
                return false;

        ++generation;
        currentState = SequenceState::Starting;
        index = -1;
        preloadIndex = -1;
        preload = Preload::None;
        takeWhenReady = false;
        advancing = false;
        publish();
        goTo(0);
        return true;
    }

    void SequenceRunner::stop()
    {
        if (!isActive())
            return;
        ++generation;
        if (index >= 0)
            port.stop(endpointOf(index), [](const CommandResult&) {});
        currentState = SequenceState::Stopped;
        publish();
    }

    void SequenceRunner::interrupt()
    {
        if (!isActive())
            return;
        ++generation;
        currentState = SequenceState::Interrupted;
        publish();
    }

    void SequenceRunner::onItemFinished(const std::string& itemId)
    {
        if (currentState != SequenceState::Running || advancing || index < 0)
            return;
        if (sequence.items[static_cast<std::size_t>(index)].id != itemId)
            return;

        const int next = nextIndex();
        if (next < 0)
        {
            ++generation;
            currentState = SequenceState::Finished;
            publish();
            return;
        }
        goTo(next);
    }

    void SequenceRunner::goTo(int target)
    {
        advancing = true;
        if (preloadIndex == target && preload == Preload::Ready)
        {
            takePrepared(target);
            return;
        }
        if (preloadIndex == target && preload == Preload::Pending)
        {
            takeWhenReady = true;
            return;
        }

        const std::weak_ptr<int> guard = alive;
        const unsigned gen = generation;
        port.prepare(endpointOf(target), sequence.items[static_cast<std::size_t>(target)], [this, guard, gen, target](const CommandResult& result)
        {
            if (guard.expired() || gen != generation)
                return;
            if (!result.ok)
            {
                fail(result.error);
                return;
            }
            takePrepared(target);
        });
    }

    void SequenceRunner::takePrepared(int target)
    {
        preloadIndex = -1;
        preload = Preload::None;

        const std::weak_ptr<int> guard = alive;
        const unsigned gen = generation;
        port.take(endpointOf(target), [this, guard, gen, target](const CommandResult& result)
        {
            if (guard.expired() || gen != generation)
                return;
            if (!result.ok)
            {
                fail(result.error);
                return;
            }

            // Se o item anterior estava em outro destino, ele sai do ar.
            if (index >= 0 && !(endpointOf(index) == endpointOf(target)))
                port.stop(endpointOf(index), [](const CommandResult&) {});

            index = target;
            advancing = false;
            currentState = SequenceState::Running;
            publish();
            preloadNext();
        });
    }

    void SequenceRunner::preloadNext()
    {
        const int next = nextIndex();
        if (next < 0)
            return;

        preloadIndex = next;
        preload = Preload::Pending;

        const std::weak_ptr<int> guard = alive;
        const unsigned gen = generation;
        port.prepare(endpointOf(next), sequence.items[static_cast<std::size_t>(next)], [this, guard, gen, next](const CommandResult& result)
        {
            if (guard.expired() || gen != generation || preloadIndex != next)
                return;

            preload = result.ok ? Preload::Ready : Preload::Failed;
            if (!takeWhenReady)
                return;

            takeWhenReady = false;
            if (result.ok)
                takePrepared(next);
            else
                fail(result.error);
        });
    }

    void SequenceRunner::fail(const std::string& error)
    {
        ++generation;
        advancing = false;
        currentState = SequenceState::Error;
        publish(error);
    }

    void SequenceRunner::publish(const std::string& error)
    {
        if (!listener)
            return;
        SequenceEvent event;
        event.state = currentState;
        event.index = index;
        if (index >= 0)
            event.itemId = sequence.items[static_cast<std::size_t>(index)].id;
        event.error = error;
        listener(event);
    }
}
