#include "OscLayerMonitor.h"

#include <cmath>

OscLayerMonitor::OscLayerMonitor(const engine::Endpoint& endpoint, const QString& hostAddress, QObject* source, QObject* parent)
    : QObject(parent), target(endpoint)
{
    this->layerPrefix = QString("%1/channel/%2/stage/layer/%3/foreground/").arg(hostAddress).arg(endpoint.channel).arg(endpoint.layer);
    this->frameratepath = QString("%1/channel/%2/framerate").arg(hostAddress).arg(endpoint.channel);

    if (source != nullptr)
        QObject::connect(source, SIGNAL(messageReceived(const QString&, const QList<QVariant>&)),
                         this, SLOT(messageReceived(const QString&, const QList<QVariant>&)));
}

void OscLayerMonitor::restart()
{
    this->detector.reset();
}

void OscLayerMonitor::resetState()
{
    this->detector.reset();
    this->lastElapsed = 0.0;
    this->lastTotal = 0.0;
    emit timeChanged(0.0, 0.0);
    if (!this->lastName.isEmpty())
    {
        this->lastName.clear();
        emit clipNameChanged(QString());
    }
    if (this->paused)
    {
        this->paused = false;
        emit pausedChanged(false);
    }
}

void OscLayerMonitor::messageReceived(const QString& path, const QList<QVariant>& arguments)
{
    if (arguments.isEmpty())
        return;

    if (path == this->frameratepath)
    {
        // 2.4 pode enviar a taxa como fracao (ex.: 50000 1000) ou como valor unico.
        double fps = arguments.at(0).toDouble();
        if (arguments.count() >= 2 && arguments.at(1).toDouble() > 0.0)
        {
            const double ratio = arguments.at(0).toDouble() / arguments.at(1).toDouble();
            if (ratio > 0.0 && ratio <= 300.0)
                fps = ratio;
        }
        if (fps > 0.0 && std::fabs(fps - this->lastFps) > 1e-6)
        {
            this->lastFps = fps;
            this->detector.setFps(fps);
            emit fpsChanged(fps);
        }
        return;
    }

    if (!path.startsWith(this->layerPrefix))
        return;

    const QString key = path.mid(this->layerPrefix.length());

    if (key == "file/time")
    {
        if (arguments.count() < 2)
            return;
        this->lastElapsed = arguments.at(0).toDouble();
        this->lastTotal = arguments.at(1).toDouble();
        emit timeChanged(this->lastElapsed, this->lastTotal);

        // Pausado, o tempo fica parado de proposito: nao alimenta a deteccao de fim.
        if (!this->paused && this->detector.update(this->lastElapsed, this->lastTotal))
            emit clipEnded();
    }
    else if (key == "file/name")
    {
        const QString name = arguments.at(0).toString();
        if (name != this->lastName)
        {
            this->lastName = name;
            emit clipNameChanged(name);
        }
    }
    else if (key == "paused")
    {
        const bool value = arguments.at(0).toBool();
        if (value != this->paused)
        {
            this->paused = value;
            emit pausedChanged(value);
        }
    }
    else if (key == "loop")
    {
        emit loopChanged(arguments.at(0).toBool());
    }
}
