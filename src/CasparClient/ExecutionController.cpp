#include "ExecutionController.h"

#include "ConnectionManager.h"
#include "RundownModel.h"

#include "AmcpFormat.h"
#include "AmcpPlayoutPort.h"
#include "CasparDevice.h"
#include "ClipEndDetector.h"
#include "Conjunto.h"
#include "OscLayerMonitor.h"
#include "Sequence.h"

#include <QtCore/QSet>
#include <QtCore/QStringList>

ExecutionController::ExecutionController(ConnectionManager* connections, RundownModel* model, QObject* parent)
    : QObject(parent), connections(connections), model(model)
{
    this->port = new AmcpPlayoutPort([connections](const QString& name) { return connections->device(name); }, this);

    QObject::connect(connections, &ConnectionManager::oscSourceChanged, this, &ExecutionController::rebuildMonitors);
    QObject::connect(connections, &ConnectionManager::serversChanged, this, &ExecutionController::createMissingMonitors);
    QObject::connect(connections, &ConnectionManager::connectionChanged, this, &ExecutionController::connectionChanged);
    QObject::connect(model, &RundownModel::nodesAboutToBeRemoved, this, &ExecutionController::nodesAboutToBeRemoved);
}

ExecutionController::~ExecutionController()
{
    // Os runners referenciam station.resources e a porta: destruí-los antes.
    this->sequences.clear();
}

QString ExecutionController::resourceName(engine::LogicalResource resource)
{
    return QString::fromUtf8(engine::toString(resource));
}

void ExecutionController::forgetAll()
{
    ++this->epoch;  // respostas pendentes passam a ser ignoradas
    this->sequences.clear();
    this->onAirByResource.clear();
    this->preparedByResource.clear();
    for (engine::LogicalResource resource : engine::AllResources)
        emit onAirChanged(resource, engine::RootId);
}

void ExecutionController::setStation(const engine::StationConfig& config)
{
    // Execuções em andamento continuam no ar, mas deixam de ser acompanhadas (o destino pode ter mudado).
    forgetAll();
    this->model->resetStates();
    this->station = config;
    rebuildMonitors();
}

void ExecutionController::reset()
{
    forgetAll();
}

// ---------- Monitores OSC ----------

void ExecutionController::rebuildMonitors()
{
    for (const QPointer<OscLayerMonitor>& monitor : this->monitors)
        delete monitor.data();
    this->monitors.clear();
    createMissingMonitors();
}

void ExecutionController::createMissingMonitors()
{
    // Só cria os que faltam: recriar um monitor no meio de um clipe perderia o estado da detecção de
    // fim e poderia avançar a Sequência duas vezes.
    bool changed = false;
    for (const auto& [resource, endpoint] : this->station.resources.all())
    {
        if (!this->monitors.value(resource).isNull())
            continue;
        const QString host = this->connections->hostAddress(QString::fromStdString(endpoint.server));
        if (host.isEmpty() || this->connections->oscSource() == nullptr)
            continue;

        OscLayerMonitor* monitor = new OscLayerMonitor(endpoint, host, this->connections->oscSource(), this);
        const engine::LogicalResource key = resource;
        QObject::connect(monitor, &OscLayerMonitor::clipEnded, this, [this, key]() { clipEnded(key); });
        this->monitors.insert(resource, monitor);
        changed = true;
    }
    if (changed || this->monitors.isEmpty())
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
        *error = tr("%1 não está configurado na estação").arg(resourceName(resource));
    return endpoint;
}

QString ExecutionController::labelOf(engine::NodeId id) const
{
    const engine::Node* node = this->model->rundown().find(id);
    return node == nullptr ? QString() : QString::fromStdString(node->label);
}

void ExecutionController::clearPrepared(engine::LogicalResource resource)
{
    const engine::NodeId previous = this->preparedByResource.value(resource, engine::RootId);
    this->preparedByResource.remove(resource);
    const engine::Node* node = this->model->rundown().find(previous);
    if (node != nullptr && (node->state == engine::NodeState::Prepared || node->state == engine::NodeState::Preparing))
        this->model->setState(previous, engine::NodeState::Idle);
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
            emit message(tr("A Sequência prepara os próprios itens ao ser disparada (F8)"), false);
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

    const engine::LogicalResource resource = node->item.resource;
    QString error;
    const std::optional<engine::Endpoint> endpoint = endpointFor(resource, &error);
    if (!endpoint)
    {
        this->model->setState(id, engine::NodeState::Error, error);
        emit message(error, true);
        return;
    }

    // O background do recurso é da Sequência em execução: preparar aqui trocaria o próximo clipe dela.
    const engine::NodeId owner = sequenceOwning(resource);
    if (owner != engine::RootId)
    {
        emit message(tr("%1 está em uso pela Sequência \"%2\". Pare a Sequência (F9) ou dispare o item direto (F8).")
                         .arg(resourceName(resource), labelOf(owner)), true);
        return;
    }

    // Um LOADBG novo substitui o que estava preparado no mesmo recurso.
    if (this->preparedByResource.value(resource, engine::RootId) != id)
        clearPrepared(resource);
    this->preparedByResource.insert(resource, id);

    this->model->setState(id, engine::NodeState::Preparing);
    const quint64 e = this->epoch;
    this->port->prepare(*endpoint, node->item, [this, e, id, resource](const engine::CommandResult& result) {
        if (!isCurrent(e) || this->model->rundown().find(id) == nullptr)
            return;
        const bool stillOurs = this->preparedByResource.value(resource, engine::RootId) == id;
        if (!result.ok)
        {
            if (stillOurs)
                this->preparedByResource.remove(resource);
            this->model->setState(id, engine::NodeState::Error, QString::fromStdString(result.error));
            emit message(tr("%1: %2").arg(labelOf(id), QString::fromStdString(result.error)), true);
            return;
        }
        if (stillOurs)
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
            const engine::LogicalResource resource = node->item.resource;
            if (node->state == engine::NodeState::Prepared && this->preparedByResource.value(resource, engine::RootId) == id)
            {
                takePrepared(id, resource);
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

void ExecutionController::takePrepared(engine::NodeId id, engine::LogicalResource resource)
{
    const engine::Node* node = this->model->rundown().find(id);
    if (node == nullptr || this->preparedByResource.value(resource, engine::RootId) != id)
    {
        this->preparedByResource.remove(resource);
        emit message(tr("Nada preparado em %1").arg(resourceName(resource)), true);
        return;
    }

    // O item mudou de recurso depois do LOADBG: o PLAY iria para onde nada foi carregado.
    if (node->item.resource != resource)
    {
        clearPrepared(resource);
        this->model->setState(id, engine::NodeState::Idle);
        emit message(tr("\"%1\" mudou de recurso depois de preparado; prepare de novo").arg(labelOf(id)), true);
        return;
    }

    // Por construção nenhuma Sequência usa este recurso (o preparo seria recusado), mas se usar o
    // background pode ser o dela: não arriscar.
    if (sequenceOwning(resource) != engine::RootId)
    {
        clearPrepared(resource);
        emit message(tr("%1 está em uso por uma Sequência; dispare o item direto (F8)").arg(resourceName(resource)), true);
        return;
    }

    QString error;
    const std::optional<engine::Endpoint> endpoint = endpointFor(resource, &error);
    if (!endpoint)
    {
        this->model->setState(id, engine::NodeState::Error, error);
        emit message(error, true);
        return;
    }

    // Sai de "preparado" já no envio: um segundo F8 não manda outro PLAY.
    this->preparedByResource.remove(resource);
    this->model->setState(id, engine::NodeState::Preparing);

    const quint64 e = this->epoch;
    this->port->take(*endpoint, [this, e, id](const engine::CommandResult& result) {
        if (!isCurrent(e) || this->model->rundown().find(id) == nullptr)
            return;
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
    QList<engine::LogicalResource> used;
    for (const engine::Item& item : conjunto.items)
    {
        const std::optional<engine::NodeId> itemId = engine::Rundown::fromEngineId(item.id);
        // O LOADBG do disparo substitui o que estava preparado no recurso.
        if (!itemId || this->preparedByResource.value(item.resource, engine::RootId) != *itemId)
            clearPrepared(item.resource);
        this->preparedByResource.remove(item.resource);
        if (itemId)
            this->model->setState(*itemId, engine::NodeState::Preparing);
        used << item.resource;
    }

    const quint64 e = this->epoch;

    // Só depois de todas as preparações darem certo as Sequências desses recursos perdem o controle.
    auto beforeTake = [this, e, used]() {
        if (!isCurrent(e))
            return;
        for (engine::LogicalResource resource : used)
            interruptSequencesUsing(resource);
    };

    engine::fire(conjunto, this->station.resources, *this->port, [this, e, groupId](const engine::FireReport& report) {
        if (!isCurrent(e))
            return;

        bool anyOnAir = false;
        QStringList failures;
        for (const engine::ItemReport& item : report.items)
        {
            const std::optional<engine::NodeId> itemId = engine::Rundown::fromEngineId(item.itemId);
            if (!itemId || this->model->rundown().find(*itemId) == nullptr)
                continue;

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
                problems << QString("%1 (%2)").arg(QString::fromUtf8(engine::toString(issue.error)), resourceName(issue.resource));
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
    }, beforeTake);
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

bool ExecutionController::isActive(const RunningSequence& running) const
{
    const engine::SequenceState state = running.runner->state();
    return state == engine::SequenceState::Starting || state == engine::SequenceState::Running;
}

engine::NodeId ExecutionController::sequenceOwning(engine::LogicalResource resource) const
{
    for (const auto& [id, running] : this->sequences)
    {
        if (!isActive(running))
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

    // Os recursos passam a ser desta Sequência: outras Sequências neles param de avançar e o que
    // estava preparado manualmente deixa de valer (a pré-carga vai sobrescrever o background).
    for (const engine::Item& item : sequence->items)
    {
        interruptSequencesUsing(item.resource, id);
        clearPrepared(item.resource);
    }

    // Reiniciar a mesma Sequência substitui a execução anterior dela.
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
    const engine::Node* currentNode = this->model->rundown().find(running.current);

    switch (event.state)
    {
        case engine::SequenceState::Starting:
            this->model->setState(sequenceId, engine::NodeState::Preparing);
            break;

        case engine::SequenceState::Running:
            if (itemId && *itemId != running.current)
            {
                const engine::Node* next = this->model->rundown().find(*itemId);
                // Trocou de recurso: o anterior sai do ar (o runner manda STOP lá).
                if (currentNode != nullptr && next != nullptr && currentNode->item.resource != next->item.resource
                    && this->onAirByResource.value(currentNode->item.resource) == running.current)
                    releaseResource(currentNode->item.resource, engine::NodeState::Finished);
                running.current = *itemId;
                markOnAir(*itemId);
            }
            this->model->setState(sequenceId, engine::NodeState::OnAir);
            break;

        case engine::SequenceState::Finished:
            if (currentNode != nullptr && this->onAirByResource.value(currentNode->item.resource) == running.current)
                releaseResource(currentNode->item.resource, engine::NodeState::Finished);
            this->model->setState(sequenceId, engine::NodeState::Finished);
            emit message(tr("Sequência concluída: %1").arg(labelOf(sequenceId)), false);
            break;

        case engine::SequenceState::Stopped:
            if (currentNode != nullptr && this->onAirByResource.value(currentNode->item.resource) == running.current)
                releaseResource(currentNode->item.resource, engine::NodeState::Idle);
            this->model->setState(sequenceId, engine::NodeState::Idle);
            break;

        case engine::SequenceState::Interrupted:
            // O item continua no ar até quem assumiu o recurso colocar outra coisa.
            this->model->setState(sequenceId, engine::NodeState::Idle);
            break;

        case engine::SequenceState::Error:
        {
            const std::optional<engine::NodeId> failed = engine::Rundown::fromEngineId(event.failedItemId);
            if (failed)
                this->model->setState(*failed, engine::NodeState::Error, QString::fromStdString(event.error));
            this->model->setState(sequenceId, engine::NodeState::Error, QString::fromStdString(event.error));
            emit message(tr("Sequência %1: %2").arg(labelOf(sequenceId), QString::fromStdString(event.error)), true);
            break;
        }

        case engine::SequenceState::Idle:
            break;
    }
}

void ExecutionController::clipEnded(engine::LogicalResource resource)
{
    const OscLayerMonitor* layer = monitor(resource);
    const std::string clip = layer != nullptr ? layer->clipName().toStdString() : std::string();

    const engine::NodeId owner = sequenceOwning(resource);
    if (owner != engine::RootId)
    {
        RunningSequence& running = this->sequences.at(owner);
        const int index = running.runner->currentIndex();
        if (index < 0)
            return;
        const engine::Item& current = running.runner->definition().items[static_cast<std::size_t>(index)];
        // Fim de outro clipe (pacote atrasado do anterior) não avança a Sequência.
        if (current.resource == resource && engine::clipMatches(clip, current.media))
            running.runner->onItemFinished(current.id);
        return;
    }

    const engine::NodeId item = this->onAirByResource.value(resource, engine::RootId);
    const engine::Node* node = this->model->rundown().find(item);
    if (node != nullptr && !node->item.loop && engine::clipMatches(clip, node->item.media))
        releaseResource(resource, engine::NodeState::Finished);
}

// ---------- Parar e transporte ----------

void ExecutionController::stop(engine::NodeId id)
{
    const engine::Node* node = this->model->rundown().find(id);
    if (node == nullptr)
        return;

    auto running = this->sequences.find(id);
    if (running != this->sequences.end() && isActive(running->second))
    {
        running->second.runner->stop();
        return;
    }

    // Item, Conjunto, Bloco ou Sequência já encerrada: tira do ar o que dele ainda estiver no ar.
    bool any = false;
    for (engine::NodeId itemId : this->model->rundown().itemsOf(id))
    {
        const engine::LogicalResource resource = this->model->rundown().find(itemId)->item.resource;
        if (this->onAirByResource.value(resource, engine::RootId) == itemId)
        {
            stopResource(resource);
            any = true;
        }
    }
    if (!any)
        emit message(tr("\"%1\" não está no ar").arg(labelOf(id)), false);
}

QList<engine::LogicalResource> ExecutionController::onAirResourcesOf(engine::NodeId id) const
{
    QList<engine::LogicalResource> result;
    for (engine::NodeId itemId : this->model->rundown().itemsOf(id))
    {
        const engine::LogicalResource resource = this->model->rundown().find(itemId)->item.resource;
        if (this->onAirByResource.value(resource, engine::RootId) == itemId && !result.contains(resource))
            result << resource;
    }
    return result;
}

void ExecutionController::playNode(engine::NodeId id)
{
    if (this->model->rundown().find(id) == nullptr)
        return;

    // No ar e pausado: retoma (não reinicia o clipe).
    bool resumed = false;
    for (engine::LogicalResource resource : onAirResourcesOf(id))
    {
        OscLayerMonitor* layer = monitor(resource);
        const std::optional<engine::Endpoint> endpoint = this->station.resources.find(resource);
        if (layer != nullptr && layer->isPaused() && endpoint)
        {
            sendTransport(resource, engine::amcp::resume(*endpoint), tr("Retomar"));
            resumed = true;
        }
    }
    if (!resumed)
        fire(id);
}

void ExecutionController::pauseNode(engine::NodeId id)
{
    const QList<engine::LogicalResource> resources = onAirResourcesOf(id);
    if (resources.isEmpty())
    {
        emit message(tr("\"%1\" não está no ar").arg(labelOf(id)), false);
        return;
    }
    for (engine::LogicalResource resource : resources)
        pause(resource);
}

void ExecutionController::clearNode(engine::NodeId id)
{
    // Só limpa layers cujo conteúdo (no ar ou preparado) é do selecionado, ou da Sequência selecionada.
    QList<engine::LogicalResource> targets = onAirResourcesOf(id);
    for (engine::NodeId itemId : this->model->rundown().itemsOf(id))
    {
        const engine::LogicalResource resource = this->model->rundown().find(itemId)->item.resource;
        if (this->preparedByResource.value(resource, engine::RootId) == itemId && !targets.contains(resource))
            targets << resource;
    }
    auto running = this->sequences.find(id);
    if (running != this->sequences.end() && isActive(running->second))
        for (const engine::Item& item : running->second.runner->definition().items)
            if (!targets.contains(item.resource))
                targets << item.resource;

    if (targets.isEmpty())
    {
        emit message(tr("\"%1\" não está no ar nem preparado").arg(labelOf(id)), false);
        return;
    }
    for (engine::LogicalResource resource : targets)
        clearResource(resource);
}

void ExecutionController::sendTransport(engine::LogicalResource resource, const std::string& command, const QString& label,
                                        std::function<void()> onSuccess)
{
    QString error;
    const std::optional<engine::Endpoint> endpoint = endpointFor(resource, &error);
    if (!endpoint)
    {
        emit message(error, true);
        return;
    }
    const quint64 e = this->epoch;
    this->port->sendCommand(QString::fromStdString(endpoint->server), QString::fromStdString(command),
        [this, e, resource, label, onSuccess](const engine::CommandResult& result) {
            if (!result.ok)
            {
                emit message(tr("%1 %2: %3").arg(label, resourceName(resource), QString::fromStdString(result.error)), true);
                return;
            }
            if (onSuccess && isCurrent(e))
                onSuccess();
        });
}

void ExecutionController::play(engine::LogicalResource resource)
{
    const std::optional<engine::Endpoint> endpoint = this->station.resources.find(resource);
    if (!endpoint)
    {
        emit message(tr("%1 não está configurado na estação").arg(resourceName(resource)), true);
        return;
    }

    OscLayerMonitor* layer = monitor(resource);
    if (layer != nullptr && layer->isPaused())
    {
        sendTransport(resource, engine::amcp::resume(*endpoint), tr("Retomar"));
        return;
    }

    const engine::NodeId prepared = this->preparedByResource.value(resource, engine::RootId);
    if (prepared != engine::RootId)
    {
        takePrepared(prepared, resource);
        return;
    }

    // Sem pausa e sem preparo deste cliente: um PLAY puro colocaria no ar o que estiver no background.
    emit message(tr("Nada preparado em %1. Prepare um item (F7) ou dispare direto (F8).").arg(resourceName(resource)), true);
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

    // O recurso só é liberado na tela quando o servidor confirmar o STOP.
    sendTransport(resource, engine::amcp::stop(*endpoint), tr("Stop"), [this, resource]() {
        releaseResource(resource, engine::NodeState::Idle);
        if (OscLayerMonitor* layer = monitor(resource))
            layer->resetState();
    });
}

void ExecutionController::clearResource(engine::LogicalResource resource)
{
    const std::optional<engine::Endpoint> endpoint = this->station.resources.find(resource);
    if (!endpoint)
        return;
    const engine::NodeId owner = sequenceOwning(resource);
    if (owner != engine::RootId)
        this->sequences.at(owner).runner->interrupt();

    sendTransport(resource, engine::amcp::clear(*endpoint), tr("Clear"), [this, resource]() {
        releaseResource(resource, engine::NodeState::Idle);
        clearPrepared(resource);
        if (OscLayerMonitor* layer = monitor(resource))
            layer->resetState();
    });
}

// ---------- Eventos externos ----------

void ExecutionController::nodesAboutToBeRemoved(const QList<engine::NodeId>& ids)
{
    const QSet<engine::NodeId> removed(ids.begin(), ids.end());

    for (engine::LogicalResource resource : engine::AllResources)
    {
        if (removed.contains(this->preparedByResource.value(resource, engine::RootId)))
            this->preparedByResource.remove(resource);
        if (removed.contains(this->onAirByResource.value(resource, engine::RootId)))
        {
            // O conteúdo continua no ar no servidor; só deixa de ser acompanhado.
            this->onAirByResource.remove(resource);
            emit onAirChanged(resource, engine::RootId);
        }
    }

    // Sequência removida (ou com item removido) para de avançar.
    QList<engine::NodeId> finished;
    for (auto& [id, running] : this->sequences)
    {
        bool affected = removed.contains(id);
        for (const engine::Item& item : running.runner->definition().items)
        {
            const std::optional<engine::NodeId> itemId = engine::Rundown::fromEngineId(item.id);
            affected = affected || (itemId && removed.contains(*itemId));
        }
        if (affected)
        {
            running.runner->interrupt();
            finished << id;
        }
    }
    for (engine::NodeId id : finished)
        this->sequences.erase(id);
}

void ExecutionController::connectionChanged(const QString& server, bool connected)
{
    if (connected)
    {
        createMissingMonitors();
        return;
    }

    // Servidor caiu: o background dele se perdeu; nada está mais "preparado" ali.
    for (const auto& [resource, endpoint] : this->station.resources.all())
        if (QString::fromStdString(endpoint.server) == server)
            clearPrepared(resource);
}
