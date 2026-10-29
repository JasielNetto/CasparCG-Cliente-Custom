#pragma once

// Execução do Rundown no CasparCG: preparar, disparar e parar Itens, Conjuntos e Sequências, e o
// transporte dos recursos (VT, Selo 1, Selo 2). Mantém os estados dos nós coerentes por recurso:
//   - um recurso tem no máximo um nó "no ar" e um "preparado" (LOADBG);
//   - disparar algo num recurso usado por uma Sequência em execução substitui a Sequência (interrupt);
//   - o fim de clipe (OSC) avança a Sequência dona do recurso ou marca o item como concluído.

#include "Rundown.h"
#include "StationConfig.h"

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>

#include <map>
#include <memory>

class AmcpPlayoutPort;
class ConnectionManager;
class OscLayerMonitor;
class RundownModel;

namespace engine
{
    class SequenceRunner;
}

class ExecutionController : public QObject
{
    Q_OBJECT

    public:
        ExecutionController(ConnectionManager* connections, RundownModel* model, QObject* parent = nullptr);
        ~ExecutionController() override;

        void setStation(const engine::StationConfig& station);
        const engine::ResourceMap& resources() const { return this->station.resources; }

        void prepare(engine::NodeId id);
        void fire(engine::NodeId id);
        void stop(engine::NodeId id);

        // Transporte do recurso (painéis VT / Selo 1 / Selo 2).
        void play(engine::LogicalResource resource);
        void pause(engine::LogicalResource resource);
        void stopResource(engine::LogicalResource resource);
        void clearResource(engine::LogicalResource resource);

        OscLayerMonitor* monitor(engine::LogicalResource resource) const;
        engine::NodeId onAir(engine::LogicalResource resource) const;

        // Esquece estados e execuções (ex.: ao abrir outro rundown). Não envia comandos.
        void reset();

        Q_SIGNAL void message(const QString& text, bool isError);
        Q_SIGNAL void monitorsChanged();
        Q_SIGNAL void onAirChanged(engine::LogicalResource resource, engine::NodeId id);

    private:
        struct RunningSequence
        {
            std::unique_ptr<engine::SequenceRunner> runner;
            engine::NodeId current = engine::RootId;
        };

        ConnectionManager* connections = nullptr;
        RundownModel* model = nullptr;
        AmcpPlayoutPort* port = nullptr;
        engine::StationConfig station;

        QMap<engine::LogicalResource, QPointer<OscLayerMonitor>> monitors;
        QMap<engine::LogicalResource, engine::NodeId> onAirByResource;
        QMap<engine::LogicalResource, engine::NodeId> preparedByResource;
        std::map<engine::NodeId, RunningSequence> sequences;

        void rebuildMonitors();
        std::optional<engine::Endpoint> endpointFor(engine::LogicalResource resource, QString* error) const;
        QString labelOf(engine::NodeId id) const;

        void prepareItem(engine::NodeId id);
        void fireItems(engine::NodeId groupId, const engine::Conjunto& conjunto);
        void takePrepared(engine::NodeId id);
        void startSequence(engine::NodeId id);

        void markOnAir(engine::NodeId itemId);
        void releaseResource(engine::LogicalResource resource, engine::NodeState previousState);
        void interruptSequencesUsing(engine::LogicalResource resource, engine::NodeId except = engine::RootId);
        engine::NodeId sequenceOwning(engine::LogicalResource resource) const;
        void sequenceEvent(engine::NodeId sequenceId, const engine::SequenceEvent& event);
        void sendTransport(engine::LogicalResource resource, const std::string& command, const QString& label);

        void clipEnded(engine::LogicalResource resource);
};
