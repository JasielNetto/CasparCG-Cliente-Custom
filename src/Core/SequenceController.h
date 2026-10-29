#pragma once

// Controlador minimo de Sequencia: liga o nucleo (engine::SequenceRunner) ao CasparCG real
// (AmcpPlayoutPort) e ao fim de clipe via OSC (OscClipEndWatcher).
//
// Versao 1 (validacao com servidor): todos os clipes de uma sequencia precisam estar no mesmo
// servidor/canal/layer, que vira o recurso VT. Transicoes, seek/length e atrasos dos itens do rundown
// nao sao usados ainda. Uma sequencia por controlador; iniciar outra substitui a anterior.

#include "Shared.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>

#include <memory>

namespace engine
{
    class SequenceRunner;
    class ResourceMap;
    struct SequenceEvent;
}

class AmcpPlayoutPort;
class OscClipEndWatcher;

class CORE_EXPORT SequenceController : public QObject
{
    Q_OBJECT

    public:
        struct Clip
        {
            QString label;
            QString deviceName;
            int channel = 1;
            int layer = 10;
            QString media;
            bool loop = false;
        };

        explicit SequenceController(QObject* parent = nullptr);
        ~SequenceController() override;

        // Retorna false e preenche 'error' (sem enviar nada) se a lista for invalida.
        bool start(const QList<Clip>& clips, bool loop, QString* error = nullptr);
        void stop();
        bool isRunning() const;

        // Validacao pura, exposta para a interface decidir se habilita a acao.
        static bool validate(const QList<Clip>& clips, QString* error);

        Q_SIGNAL void statusChanged(const QString& message, bool isError);

    private:
        AmcpPlayoutPort* port = nullptr;
        QPointer<OscClipEndWatcher> watcher;
        std::unique_ptr<engine::ResourceMap> resources;
        std::unique_ptr<engine::SequenceRunner> runner;
        QList<Clip> clips;
        bool loopMode = false;

        void onEvent(const engine::SequenceEvent& event);
        Q_SLOT void clipEnded();
};
