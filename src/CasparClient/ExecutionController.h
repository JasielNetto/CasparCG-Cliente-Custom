#pragma once

// Execução do Rundown no CasparCG: preparar, disparar e parar Itens, Conjuntos e Sequências, e o
// transporte dos recursos (VT, Selo 1, Selo 2). Regras (PROJECT_CONTEXT.md, D-08 e D-09):
//   - um recurso tem no máximo um nó "no ar" e um "preparado" (LOADBG);
//   - um recurso usado por uma Sequência em execução pertence a ela: o preparo manual (F7) nele é
//     recusado, porque a pré-carga da Sequência usa o mesmo background;
//   - disparar algo num recurso de uma Sequência a interrompe, mas só depois de todas as preparações
//     darem certo (um disparo que falha não derruba a Sequência);
//   - o fim de clipe (OSC) só vale se o nome do clipe confere com a mídia esperada;
//   - Play no painel nunca envia PLAY "às cegas": retoma a pausa ou dispara o que este cliente preparou;
//   - respostas que chegam depois de trocar de rundown ou de estação são descartadas (época).

#include "Rundown.h"
#include "StationConfig.h"

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>

#include <functional>
#include <map>
#include <memory>
#include <optional>

class AmcpPlayoutPort;
class ConnectionManager;
class OscLayerMonitor;
class RundownModel;

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

        // Transporte sobre o nó selecionado no Rundown (barra do Rundown).
        void playNode(engine::NodeId id);   // retoma se estiver pausado; senão dispara
        void pauseNode(engine::NodeId id);
        void clearNode(engine::NodeId id);

        // Transporte do recurso.
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
        quint64 epoch = 0;

        QMap<engine::LogicalResource, QPointer<OscLayerMonitor>> monitors;
        QMap<engine::LogicalResource, engine::NodeId> onAirByResource;
        QMap<engine::LogicalResource, engine::NodeId> preparedByResource;
        std::map<engine::NodeId, RunningSequence> sequences;

        bool isCurrent(quint64 capturedEpoch) const { return capturedEpoch == this->epoch; }
        void forgetAll();

        void rebuildMonitors();
        void createMissingMonitors();
        std::optional<engine::Endpoint> endpointFor(engine::LogicalResource resource, QString* error) const;
        QString labelOf(engine::NodeId id) const;
        static QString resourceName(engine::LogicalResource resource);

        void prepareItem(engine::NodeId id);
        void fireItems(engine::NodeId groupId, const engine::Conjunto& conjunto);
        void takePrepared(engine::NodeId id, engine::LogicalResource resource);
        void startSequence(engine::NodeId id);
        void clearPrepared(engine::LogicalResource resource);

        void markOnAir(engine::NodeId itemId);
        void releaseResource(engine::LogicalResource resource, engine::NodeState previousState);
        void interruptSequencesUsing(engine::LogicalResource resource, engine::NodeId except = engine::RootId);
        engine::NodeId sequenceOwning(engine::LogicalResource resource) const;
        bool isActive(const RunningSequence& running) const;
        void sequenceEvent(engine::NodeId sequenceId, const engine::SequenceEvent& event);
        QList<engine::LogicalResource> onAirResourcesOf(engine::NodeId id) const;
        void sendTransport(engine::LogicalResource resource, const std::string& command, const QString& label,
                           std::function<void()> onSuccess = {});

        void clipEnded(engine::LogicalResource resource);
        void nodesAboutToBeRemoved(const QList<engine::NodeId>& ids);
        void connectionChanged(const QString& server, bool connected);
};
