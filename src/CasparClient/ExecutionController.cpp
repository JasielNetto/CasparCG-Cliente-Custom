#include "ExecutionController.h"

#include "ConnectionManager.h"
#include "RundownModel.h"

#include "AmcpFormat.h"
#include "AmcpPlayoutPort.h"
#include "CasparDevice.h"
#include "Conjunto.h"
#include "OscLayerMonitor.h"
#include "Sequence.h"

#include <QtCore/QStringList>

ExecutionController::ExecutionController(ConnectionManager* connections, RundownModel* model, QObject* parent)
    : QObject(parent), connections(connections), model(model)
{
    this->port = new AmcpPlayoutPort([connections](const QString& name) { return connections->device(name); }, this);

    QObject::connect(connections, &ConnectionManager::oscSourceChanged, this, &ExecutionController::rebuildMonitors);
    QObject::connect(connections, &ConnectionManager::serversChanged, this, &ExecutionController::rebuildMonitors);
}

ExecutionController::~ExecutionController()
{
    // Os runners referenciam station.resources e a porta: destruí-los antes.
    this->sequences.clear();
}

void ExecutionController::setStation(const engine::StationConfig& config)
{
    // Execuções em andamento continuam no ar, mas deixam de ser acompanhadas (o destino pode ter mudado).
    this->sequences.clear();
    this->onAirByResource.clear();
    this->preparedByResource.clear();
    this->model->resetStates();

    this->station = config;
    rebuildMonitors();
}

void ExecutionController::reset()
{
    this->sequences.clear();
    this->onAirByResource.clear();
    this->preparedByResource.clear();
}

void ExecutionController::rebuildMonitors()
{
    for (const QPointer<OscLayerMonitor>& monitor : this->monitors)
        delete monitor.data();
    this->monitors.clear();

    for (const auto& [resource, endpoint] : this->station.resources.all())
    {
        const QString server = QString::fromStdString(endpoint.server);
        const QString host = this->connections->hostAddress(server);
        if (host.isEmpty())
            continue;

        OscLayerMonitor* monitor = new OscLayerMonitor(endpoint, host, this->connections->oscSource(), this);
        const engine::LogicalResource key = resource;
        QObject::connect(monitor, &OscLayerMonitor::clipEnded, this, [this, key]() { clipEnded(key); });
        this->monitors.insert(resource, monitor);
    }
    emit monitorsChanged();
}

OscLayerMonitor* ExecutionController::monitor(engine::LogicalResource resource) const
{
    return this->monitors.value(resource).data();
}

engine::NodeId ExecutionController::onAir(engine::LogicalResource resource) const
{
    return this->onAirByResource.value(resource, engine::RootId);
}

std::optional<engine::Endpoint> ExecutionController::endpointFor(engine::LogicalResource resource, QString* error) const
{
    const std::optional<engine::Endpoint> endpoint = this->station.resources.find(resource);
    if (!endpoint && error != nullptr)
        *error = tr("%1 não está configurado na estação").arg(QString::fromUtf8(engine::toString(resource)));
    return endpoint;
}

QString ExecutionController::labelOf(engine::NodeId id) const
{
    const engine::Node* node = this->model->rundown().find(id);
    return node == nullptr ? QString() : QString::fromStdString(node->label);
}

// ---------- Preparar ----------

void ExecutionController::prepare(engine::NodeId id)
{
    const engine::Node* node = this->model->rundown().find(id);
    if (node == nullptr)
        return;

    switch (node->kind)
    {
        case engine::NodeKind::Item:
            prepareItem(id);
            break;
        case engine::NodeKind::Conjunto:
            for (engine::NodeId child : node->children)
                prepareItem(child);
            break;
        case engine::NodeKind::Sequencia:
            if (!node->children.empty())
                prepareItem(node->children.front());
            break;
        case engine::NodeKind::Bloco:
            emit message(tr("Bloco só organiza o rundown; selecione um item, Sequência ou Conjunto"), true);
            break;
    }
}

void ExecutionController::prepareItem(engine::NodeId id)
{
    const engine::Node* node = this->model->rundown().find(id);
    if (node == nullptr || node->kind != engine::NodeKind::Item)
        return;

    QString error;
    const std::optional<engine::Endpoint> endpoint = endpointFor(node->item.resource, &error);
    if (!endpoint)
    {
        this->model->setState(id, engine::NodeState::Error, error);
        emit message(error, true);
        return;
    }

    // Um LOADBG novo substitui o que estava preparado no mesmo recurso.
    const engine::LogicalResource resource = node->item.resource;
    const engine::NodeId previous = this->preparedByResource.value(resource, engine::RootId);
    if (previous != engine::RootId && previous != id)
        this->model->setState(previous, engine::NodeState::Idle);
    this->preparedByResource.insert(resource, id);

    this->model->setState(id, engine::NodeState::Preparing);
    const engine::Item item = node->item;
    this->port->prepare(*endpoint, item, [this, id, resource](const engine::CommandResult& result) {
        if (this->model->rundown().find(id) == nullptr)
            return;
        if (!result.ok)
        {
            if (this->preparedByResource.value(resource) == id)
                this->preparedByResource.remove(resource);
            this->model->setState(id, engine::NodeState::Error, QString::fromStdString(result.error));
            emit message(tr("%1: %2").arg(labelOf(id), QString::fromStdString(result.error)), true);
            return;
        }
        if (this->preparedByResource.value(resource) == id)
            this->model->setState(id, engine::NodeState::Prepared);
    });
}

// ---------- Disparar ----------

void ExecutionController::fire(engine::NodeId id)
{
    const engine::Node* node = this->model->rundown().find(id);
    if (node == nullptr)
        return;

    switch (node->kind)
    {
        case engine::NodeKind::Item:
        {
            if (node->state == engine::NodeState::Prepared && this->preparedByResource.value(node->item.resource) == id)
            {
                takePrepared(id);
                return;
            }
            engine::Conjunto single;
            single.id = engine::Rundown::engineId(id);
            single.items.push_back(node->item);
            fireItems(id, single);
            break;
        }
        case engine::NodeKind::Conjunto:
        {
            const std::optional<engine::Conjunto> conjunto = this->model->rundown().toConjunto(id);
            if (conjunto)
                fireItems(id, *conjunto);
            break;
        }
        case engine::NodeKind::Sequencia:
            startSequence(id);
            break;
        case engine::NodeKind::Bloco:
            emit message(tr("Bloco só organiza o rundown; selecione um item, Sequência ou Conjunto"), true);
            break;
    }
}

void ExecutionController::takePrepared(engine::NodeId id)
{
    const engine::Node* node = this->model->rundown().find(id);
    QString error;
    const std::optional<engine::Endpoint> endpoint = endpointFor(node->item.resource, &error);
    if (!endpoint)
    {
        this->model->setState(id, engine::NodeState::Error, error);
        emit message(error, true);
        return;
    }

    const engine::LogicalResource resource = node->item.resource;
    interruptSequencesUsing(resource);
    this->port->take(*endpoint, [this, id, resource](const engine::CommandResult& result) {
        if (this->model->rundown().find(id) == nullptr)
            return;
        if (this->preparedByResource.value(resource) == id)
            this->preparedByResource.remove(resource);
        if (!result.ok)
        {
            this->model->setState(id, engine::NodeState::Error, QString::fromStdString(result.error));
            emit message(tr("%1: %2").arg(labelOf(id), QString::fromStdString(result.error)), true);
            return;
        }
        markOnAir(id);
    });
}

void ExecutionController::fireItems(engine::NodeId groupId, const engine::Conjunto& conjunto)
{
    for (const engine::Item& item : conjunto.items)
    {
        const std::optional<engine::NodeId> itemId = engine::Rundown::fromEngineId(item.id);
        if (itemId)
            this->model->setState(*itemId, engine::NodeState::Preparing);
        interruptSequencesUsing(item.resource);

        // O LOADBG do disparo substitui o que estava preparado no recurso.
        const engine::NodeId previous = this->preparedByResource.value(item.resource, engine::RootId);
        if (previous != engine::RootId && (!itemId || previous != *itemId))
            this->model->setState(previous, engine::NodeState::Idle);
        this->preparedByResource.remove(item.resource);
    }

    engine::fire(conjunto, this->station.resources, *this->port, [this, groupId](const engine::FireReport& report) {
        bool anyOnAir = false;
        QStringList failures;
        for (const engine::ItemReport& item : report.items)
        {
            const std::optional<engine::NodeId> itemId = engine::Rundown::fromEngineId(item.itemId);
            if (!itemId || this->model->rundown().find(*itemId) == nullptr)
                continue;
            if (this->preparedByResource.value(item.resource) == *itemId)
                this->preparedByResource.remove(item.resource);

            switch (item.outcome)
            {
                case engine::ItemOutcome::OnAir:
                    markOnAir(*itemId);
                    anyOnAir = true;
                    break;
                case engine::ItemOutcome::NotAttempted:
                    this->model->setState(*itemId, engine::NodeState::Idle);
                    break;
                default:
                    this->model->setState(*itemId, engine::NodeState::Error, QString::fromStdString(item.error));
                    failures << QString("%1: %2").arg(labelOf(*itemId), QString::fromStdString(item.error));
                    break;
            }
        }

        if (report.status == engine::FireStatus::Rejected)
        {
            QStringList problems;
            for (const engine::ValidationIssue& issue : report.validation.issues)
                problems << QString("%1 (%2)").arg(QString::fromUtf8(engine::toString(issue.error)), QString::fromUtf8(engine::toString(issue.resource)));
            this->model->setState(groupId, engine::NodeState::Error, problems.join("; "));
            emit message(tr("Bloqueado: %1").arg(problems.join("; ")), true);
            return;
        }

        const engine::Node* group = this->model->rundown().find(groupId);
        if (group != nullptr && group->kind == engine::NodeKind::Conjunto)
            this->model->setState(groupId, anyOnAir ? engine::NodeState::OnAir : engine::NodeState::Error, failures.join("; "));

        if (report.status == engine::FireStatus::Success)
            emit message(tr("No ar: %1").arg(labelOf(groupId)), false);
        else if (report.status == engine::FireStatus::Aborted)
            emit message(tr("Nada foi ao ar (falha na preparação): %1").arg(failures.join("; ")), true);
        else
            emit message(tr("%1 — %2: %3").arg(labelOf(groupId), QString::fromUtf8(engine::toString(report.status)), failures.join("; ")), true);
    });
}

void ExecutionController::markOnAir(engine::NodeId itemId)
{
    const engine::Node* node = this->model->rundown().find(itemId);
    if (node == nullptr)
        return;

    const engine::LogicalResource resource = node->item.resource;
    const engine::NodeId previous = this->onAirByResource.value(resource, engine::RootId);
    if (previous != engine::RootId && previous != itemId)
    {
        const engine::Node* old = this->model->rundown().find(previous);
        if (old != nullptr && old->state == engine::NodeState::OnAir)
            this->model->setState(previous, engine::NodeState::Finished);
    }

    this->onAirByResource.insert(resource, itemId);
    this->model->setState(itemId, engine::NodeState::OnAir);
    emit onAirChanged(resource, itemId);
}

void ExecutionController::releaseResource(engine::LogicalResource resource, engine::NodeState previousState)
{
    const engine::NodeId previous = this->onAirByResource.value(resource, engine::RootId);
    if (previous == engine::RootId)
        return;
    this->onAirByResource.remove(resource);
    const engine::Node* node = this->model->rundown().find(previous);
    if (node != nullptr && node->state == engine::NodeState::OnAir)
        this->model->setState(previous, previousState);
    emit onAirChanged(resource, engine::RootId);
}

// ---------- Sequência ----------

engine::NodeId ExecutionController::sequenceOwning(engine::LogicalResource resource) const
{
    for (const auto& [id, running] : this->sequences)
    {
        const engine::SequenceState state = running.runner->state();
        if (state != engine::SequenceState::Starting && state != engine::SequenceState::Running)
            continue;
        for (const engine::Item& item : running.runner->definition().items)
            if (item.resource == resource)
                return id;
    }
    return engine::RootId;
}

void ExecutionController::interruptSequencesUsing(engine::LogicalResource resource, engine::NodeId except)
{
    const engine::NodeId owner = sequenceOwning(resource);
    if (owner == engine::RootId || owner == except)
        return;
    this->sequences.at(owner).runner->interrupt();
}

void ExecutionController::startSequence(engine::NodeId id)
{
    const std::optional<engine::Sequence> sequence = this->model->rundown().toSequence(id);
    if (!sequence || sequence->items.empty())
    {
        emit message(tr("Sequência vazia"), true);
        return;
    }

    for (const engine::Item& item : sequence->items)
    {
        QString error;
        if (!endpointFor(item.resource, &error))
        {
            this->model->setState(id, engine::NodeState::Error, error);
            emit message(error, true);
            return;
        }
        if (item.loop)
        {
            const QString text = tr("Item com Loop dentro de Sequência nunca termina; desligue o Loop");
            this->model->setState(id, engine::NodeState::Error, text);
            emit message(text, true);
            return;
        }
    }

    // Recursos usados por outras execuções passam a ser desta Sequência.
    for (const engine::Item& item : sequence->items)
        interruptSequencesUsing(item.resource, id);

    // Uma Sequência reiniciada substitui a execução anterior dela mesma.
    this->sequences.erase(id);

    RunningSequence& running = this->sequences[id];
    running.runner = std::make_unique<engine::SequenceRunner>(*sequence, this->station.resources, *this->port,
        [this, id](const engine::SequenceEvent& event) { sequenceEvent(id, event); });
    if (!running.runner->start())
    {
        this->sequences.erase(id);
        emit message(tr("Não foi possível iniciar a Sequência"), true);
    }
}

void ExecutionController::sequenceEvent(engine::NodeId sequenceId, const engine::SequenceEvent& event)
{
    auto it = this->sequences.find(sequenceId);
    if (it == this->sequences.end() || this->model->rundown().find(sequenceId) == nullptr)
        return;
    RunningSequence& running = it->second;

    const std::optional<engine::NodeId> itemId = engine::Rundown::fromEngineId(event.itemId);

    switch (event.state)
    {
        case engine::SequenceState::Starting:
            this->model->setState(sequenceId, engine::NodeState::Preparing);
            break;
        case engine::SequenceState::Running:
            if (itemId)
            {
                if (running.current != engine::RootId && running.current != *itemId)
                    this->model->setState(running.current, engine::NodeState::Finished);
                running.current = *itemId;
                markOnAir(*itemId);
            }
            this->model->setState(sequenceId, engine::NodeState::OnAir);
            break;
        case engine::SequenceState::Finished:
            this->model->setState(sequenceId, engine::NodeState::Finished);
            emit message(tr("Sequência concluída: %1").arg(labelOf(sequenceId)), false);
            break;
        case engine::SequenceState::Stopped:
            if (running.current != engine::RootId)
            {
                const engine::Node* current = this->model->rundown().find(running.current);
                if (current != nullptr)
                    releaseResource(current->item.resource, engine::NodeState::Idle);
            }
            this->model->setState(sequenceId, engine::NodeState::Idle);
            break;
        case engine::SequenceState::Interrupted:
            this->model->setState(sequenceId, engine::NodeState::Idle);
            break;
        case engine::SequenceState::Error:
            if (itemId)
                this->model->setState(*itemId, engine::NodeState::Error, QString::fromStdString(event.error));
            this->model->setState(sequenceId, engine::NodeState::Error, QString::fromStdString(event.error));
            emit message(tr("Sequência %1: %2").arg(labelOf(sequenceId), QString::fromStdString(event.error)), true);
            break;
        case engine::SequenceState::Idle:
            break;
    }
}

void ExecutionController::clipEnded(engine::LogicalResource resource)
{
    const engine::NodeId owner = sequenceOwning(resource);
    if (owner != engine::RootId)
    {
        RunningSequence& running = this->sequences.at(owner);
        const int index = running.runner->currentIndex();
        if (index >= 0)
        {
            const engine::Item& current = running.runner->definition().items[static_cast<std::size_t>(index)];
            if (current.resource == resource)
                running.runner->onItemFinished(current.id);
        }
        return;
    }

    const engine::NodeId item = this->onAirByResource.value(resource, engine::RootId);
    const engine::Node* node = this->model->rundown().find(item);
    if (node != nullptr && !node->item.loop)
        releaseResource(resource, engine::NodeState::Finished);
}

// ---------- Parar e transporte ----------

void ExecutionController::stop(engine::NodeId id)
{
    const engine::Node* node = this->model->rundown().find(id);
    if (node == nullptr)
        return;

    auto running = this->sequences.find(id);
    if (running != this->sequences.end())
    {
        running->second.runner->stop();
        return;
    }

    for (engine::NodeId itemId : this->model->rundown().itemsOf(id))
    {
        const engine::LogicalResource resource = this->model->rundown().find(itemId)->item.resource;
        if (this->onAirByResource.value(resource, engine::RootId) == itemId)
            stopResource(resource);
    }
}

void ExecutionController::sendTransport(engine::LogicalResource resource, const std::string& command, const QString& label)
{
    QString error;
    const std::optional<engine::Endpoint> endpoint = endpointFor(resource, &error);
    if (!endpoint)
    {
        emit message(error, true);
        return;
    }
    this->port->sendCommand(QString::fromStdString(endpoint->server), QString::fromStdString(command),
        [this, resource, label](const engine::CommandResult& result) {
            if (!result.ok)
                emit message(tr("%1 %2: %3").arg(label, QString::fromUtf8(engine::toString(resource)), QString::fromStdString(result.error)), true);
        });
}

void ExecutionController::play(engine::LogicalResource resource)
{
    const std::optional<engine::Endpoint> endpoint = this->station.resources.find(resource);
    if (!endpoint)
    {
        emit message(tr("%1 não está configurado na estação").arg(QString::fromUtf8(engine::toString(resource))), true);
        return;
    }
    OscLayerMonitor* layer = monitor(resource);
    if (layer != nullptr && layer->isPaused())
    {
        sendTransport(resource, engine::amcp::resume(*endpoint), tr("Retomar"));
        return;
    }

    // Sem pausa: dispara o que estiver preparado no recurso.
    const engine::NodeId prepared = this->preparedByResource.value(resource, engine::RootId);
    if (prepared != engine::RootId)
    {
        takePrepared(prepared);
        return;
    }
    sendTransport(resource, engine::amcp::play(*endpoint), tr("Play"));
}

void ExecutionController::pause(engine::LogicalResource resource)
{
    const std::optional<engine::Endpoint> endpoint = this->station.resources.find(resource);
    if (endpoint)
        sendTransport(resource, engine::amcp::pause(*endpoint), tr("Pausar"));
}

void ExecutionController::stopResource(engine::LogicalResource resource)
{
    const std::optional<engine::Endpoint> endpoint = this->station.resources.find(resource);
    if (!endpoint)
        return;
    const engine::NodeId owner = sequenceOwning(resource);
    if (owner != engine::RootId)
        this->sequences.at(owner).runner->interrupt();
    sendTransport(resource, engine::amcp::stop(*endpoint), tr("Stop"));
    releaseResource(resource, engine::NodeState::Idle);
}

void ExecutionController::clearResource(engine::LogicalResource resource)
{
    const std::optional<engine::Endpoint> endpoint = this->station.resources.find(resource);
    if (!endpoint)
        return;
    const engine::NodeId owner = sequenceOwning(resource);
    if (owner != engine::RootId)
        this->sequences.at(owner).runner->interrupt();
    sendTransport(resource, engine::amcp::clear(*endpoint), tr("Clear"));
    releaseResource(resource, engine::NodeState::Idle);

    const engine::NodeId prepared = this->preparedByResource.value(resource, engine::RootId);
    if (prepared != engine::RootId)
    {
        this->preparedByResource.remove(resource);
        this->model->setState(prepared, engine::NodeState::Idle);
    }
}
