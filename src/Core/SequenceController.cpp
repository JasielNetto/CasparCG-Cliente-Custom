#include "SequenceController.h"

#include "AmcpPlayoutPort.h"
#include "DeviceManager.h"
#include "OscDeviceManager.h"
#include "OscLayerMonitor.h"

#include "CasparDevice.h"
#include "OscMonitorListener.h"

#include "Resource.h"
#include "Sequence.h"

SequenceController::SequenceController(QObject* parent)
    : QObject(parent)
{
    this->port = new AmcpPlayoutPort([](const QString& name) { return DeviceManager::getInstance().getDeviceByName(name); }, this);
}

SequenceController::~SequenceController()
{
    // O runner referencia 'resources' e 'port': destruir o runner primeiro.
    this->runner.reset();
    this->resources.reset();
}

bool SequenceController::validate(const QList<Clip>& clips, QString* error)
{
    auto fail = [error](const QString& message) {
        if (error != nullptr)
            *error = message;
        return false;
    };

    if (clips.isEmpty())
        return fail("Select at least one video item");

    const Clip& first = clips.first();
    for (const Clip& clip : clips)
    {
        if (clip.deviceName != first.deviceName || clip.channel != first.channel || clip.layer != first.layer)
            return fail(QString("All items must use the same server, channel and layer (%1 %2-%3)")
                        .arg(first.deviceName).arg(first.channel).arg(first.layer));
        if (clip.loop)
            return fail(QString("Item '%1' has Loop enabled; it would never end").arg(clip.label));
        if (clip.media.isEmpty())
            return fail(QString("Item '%1' has no video").arg(clip.label));
    }

    if (DeviceManager::getInstance().getDeviceByName(first.deviceName).isNull())
        return fail(QString("Server '%1' is not configured").arg(first.deviceName));

    return true;
}

bool SequenceController::start(const QList<Clip>& newClips, bool loop, QString* error)
{
    if (!validate(newClips, error))
        return false;

    const Clip& first = newClips.first();
    const engine::Endpoint endpoint { first.deviceName.toStdString(), first.channel, first.layer };

    // Sequencia anterior: no mesmo destino a nova assume o ar (sem STOP, que poderia cortar a nova);
    // em outro destino, a anterior sai do ar.
    if (this->runner)
    {
        const engine::Endpoint previous = *this->resources->find(engine::LogicalResource::Vt);
        if (previous == endpoint)
            this->runner->interrupt();
        else
            this->runner->stop();
    }
    this->runner.reset();

    if (!this->watcher.isNull())
        delete this->watcher.data();
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(first.deviceName);
    if (device.isNull())
    {
        if (error != nullptr)
            *error = QString("Server '%1' is not configured").arg(first.deviceName);
        return false;
    }
    this->watcher = new OscLayerMonitor(endpoint, device->resolveIpAddress(),
                                        OscDeviceManager::getInstance().getOscMonitorListener().data(), this);
    QObject::connect(this->watcher.data(), &OscLayerMonitor::clipEnded, this, &SequenceController::clipEnded);

    this->clips = newClips;
    this->loopMode = loop;

    this->resources = std::make_unique<engine::ResourceMap>();
    this->resources->set(engine::LogicalResource::Vt, endpoint);

    engine::Sequence sequence;
    sequence.id = "rundown-selection";
    sequence.repeat = loop ? engine::RepeatMode::Loop : engine::RepeatMode::Once;
    for (int i = 0; i < newClips.count(); ++i)
    {
        engine::Item item;
        item.id = std::to_string(i);
        item.resource = engine::LogicalResource::Vt;
        item.media = newClips.at(i).media.toStdString();
        sequence.items.push_back(item);
    }

    this->runner = std::make_unique<engine::SequenceRunner>(sequence, *this->resources, *this->port,
                                                            [this](const engine::SequenceEvent& event) { onEvent(event); });
    if (!this->runner->start())
    {
        if (error != nullptr)
            *error = "Could not start the sequence";
        return false;
    }
    return true;
}

void SequenceController::stop()
{
    if (this->runner)
        this->runner->stop();
}

bool SequenceController::isRunning() const
{
    if (!this->runner)
        return false;
    const engine::SequenceState state = this->runner->state();
    return state == engine::SequenceState::Starting || state == engine::SequenceState::Running;
}

void SequenceController::clipEnded()
{
    if (!this->runner || this->runner->state() != engine::SequenceState::Running)
        return;

    const int index = this->runner->currentIndex();
    if (index >= 0)
        this->runner->onItemFinished(std::to_string(index));
}

void SequenceController::onEvent(const engine::SequenceEvent& event)
{
    const int total = this->clips.count();
    const QString mode = this->loopMode ? " (loop)" : "";

    switch (event.state)
    {
        case engine::SequenceState::Starting:
            emit statusChanged(QString("Sequence%1: starting...").arg(mode), false);
            break;
        case engine::SequenceState::Running:
        {
            const QString label = (event.index >= 0 && event.index < total) ? this->clips.at(event.index).label : QString();
            emit statusChanged(QString("Sequence%1: on air %2/%3 - %4").arg(mode).arg(event.index + 1).arg(total).arg(label), false);
            break;
        }
        case engine::SequenceState::Finished:
            emit statusChanged("Sequence finished", false);
            break;
        case engine::SequenceState::Stopped:
            emit statusChanged("Sequence stopped", false);
            break;
        case engine::SequenceState::Interrupted:
            emit statusChanged("Sequence replaced", false);
            break;
        case engine::SequenceState::Error:
            emit statusChanged(QString("Sequence error: %1").arg(QString::fromStdString(event.error)), true);
            break;
        case engine::SequenceState::Idle:
            break;
    }
}
