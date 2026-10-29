#pragma once

// Observa o OSC "/foreground/file/time" de um destino (servidor/canal/layer) e emite clipEnded
// quando o clipe chega ao fim (engine::ClipEndDetector). E a fonte de onItemFinished do
// SequenceRunner.
//
// Nao chame restart() a cada take: o detector ja rearma sozinho quando o novo clipe comeca, e um
// restart() logo apos o take faria um pacote atrasado do clipe anterior parecer o fim do novo.

#include "Shared.h"

#include "ClipEndDetector.h"
#include "Resource.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

class OscSubscription;

class CORE_EXPORT OscClipEndWatcher : public QObject
{
    Q_OBJECT

    public:
        // Retorna o watcher ja ativo, ou nullptr se o servidor nao estiver cadastrado.
        static OscClipEndWatcher* create(const engine::Endpoint& endpoint, QObject* parent = nullptr);

        const engine::Endpoint& endpoint() const { return this->target; }

        void setTolerance(double seconds);
        void restart();

        Q_SIGNAL void clipEnded();
        Q_SIGNAL void timeChanged(double elapsed, double total);

    private:
        OscClipEndWatcher(const engine::Endpoint& endpoint, const QString& filter, QObject* parent);

        engine::Endpoint target;
        engine::ClipEndDetector detector;
        OscSubscription* subscription = nullptr;

        Q_SLOT void timeReceived(const QString& path, const QList<QVariant>& arguments);
};
