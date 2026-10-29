#pragma once

// Acompanha pelo OSC o que esta no ar em um destino (servidor/canal/layer) do CasparCG 2.4:
// nome do clipe, tempo decorrido/total, pausa, loop e taxa de quadros do canal. Tambem detecta o
// fim do clipe (engine::ClipEndDetector), que alimenta SequenceRunner::onItemFinished.
//
// 'source' e qualquer objeto com o sinal messageReceived(const QString&, const QList<QVariant>&)
// cujo caminho comeca pelo IP do servidor (ex.: OscMonitorListener). 'hostAddress' e o IP do servidor.
//
// Nao chame restart() a cada take: o detector rearma sozinho quando o novo clipe comeca; um restart()
// logo apos o take faria um pacote atrasado do clipe anterior parecer o fim do novo.

#include "ClipEndDetector.h"
#include "Resource.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

class OscLayerMonitor : public QObject
{
    Q_OBJECT

    public:
        OscLayerMonitor(const engine::Endpoint& endpoint, const QString& hostAddress, QObject* source, QObject* parent = nullptr);

        const engine::Endpoint& endpoint() const { return this->target; }

        void restart();

        // Zera o que se sabe do layer (apos STOP/CLEAR o servidor para de enviar OSC dele).
        void resetState();

        double elapsed() const { return this->lastElapsed; }
        double total() const { return this->lastTotal; }
        double fps() const { return this->lastFps; }
        const QString& clipName() const { return this->lastName; }
        bool isPaused() const { return this->paused; }

        Q_SIGNAL void clipEnded();
        Q_SIGNAL void timeChanged(double elapsed, double total);
        Q_SIGNAL void clipNameChanged(const QString& name);
        Q_SIGNAL void pausedChanged(bool paused);
        Q_SIGNAL void loopChanged(bool loop);
        Q_SIGNAL void fpsChanged(double fps);

    private:
        engine::Endpoint target;
        engine::ClipEndDetector detector;

        QString layerPrefix;    // "<ip>/channel/N/stage/layer/L/foreground/"
        QString frameratepath;  // "<ip>/channel/N/framerate"

        double lastElapsed = 0.0;
        double lastTotal = 0.0;
        double lastFps = 0.0;
        QString lastName;
        bool paused = false;

        Q_SLOT void messageReceived(const QString& path, const QList<QVariant>& arguments);
};
