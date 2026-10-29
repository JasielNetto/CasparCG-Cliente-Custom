#include "OscClipEndWatcher.h"

#include "DeviceManager.h"
#include "Global.h"
#include "OscSubscription.h"

#include "CasparDevice.h"

OscClipEndWatcher* OscClipEndWatcher::create(const engine::Endpoint& endpoint, QObject* parent)
{
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(QString::fromStdString(endpoint.server));
    if (device.isNull())
        return nullptr;

    QString filter = Osc::VIDEOLAYER_TIME_FILTER;
    filter.replace("#IPADDRESS#", device->resolveIpAddress())
          .replace("#CHANNEL#", QString::number(endpoint.channel))
          .replace("#VIDEOLAYER#", QString::number(endpoint.layer));

    return new OscClipEndWatcher(endpoint, filter, parent);
}

OscClipEndWatcher::OscClipEndWatcher(const engine::Endpoint& endpoint, const QString& filter, QObject* parent)
    : QObject(parent), target(endpoint)
{
    this->subscription = new OscSubscription(filter, this);
    QObject::connect(this->subscription, &OscSubscription::subscriptionReceived, this, &OscClipEndWatcher::timeReceived);
}

void OscClipEndWatcher::setTolerance(double seconds)
{
    this->detector = engine::ClipEndDetector(seconds);
}

void OscClipEndWatcher::restart()
{
    this->detector.reset();
}

void OscClipEndWatcher::timeReceived(const QString& path, const QList<QVariant>& arguments)
{
    Q_UNUSED(path);

    if (arguments.count() < 2)
        return;

    const double elapsed = arguments.at(0).toDouble();
    const double total = arguments.at(1).toDouble();

    emit timeChanged(elapsed, total);

    if (this->detector.update(elapsed, total))
        emit clipEnded();
}
