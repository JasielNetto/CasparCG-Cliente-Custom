#include "MirrorManager.h"
#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "Models/ConfigurationModel.h"

#include "MirrorTranslator.h"

#include <QtCore/QDebug>
#include <QtCore/QSharedPointer>

Q_GLOBAL_STATIC(MirrorManager, mirrorManager)

namespace
{
    struct VerbName { const char* name; uint32_t bit; };

    const VerbName verbTable[] = {
        { "PLAY",   mirror::VerbPlay },
        { "LOAD",   mirror::VerbLoad },
        { "STOP",   mirror::VerbStop },
        { "PAUSE",  mirror::VerbPause },
        { "RESUME", mirror::VerbResume },
        { "CLEAR",  mirror::VerbClear },
        { "MIXER",  mirror::VerbMixer },
        { "CALL",   mirror::VerbCall },
        { "CG",     mirror::VerbCg },
    };

    QString readConfig(const QString& name, const QString& fallback)
    {
        const ConfigurationModel model = DatabaseManager::getInstance().getConfigurationByName(name);
        return model.getName().isEmpty() ? fallback : model.getValue();
    }

    void writeConfig(const QString& name, const QString& value)
    {
        DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(-1, name, value));
    }
}

MirrorManager::MirrorManager()
{
    this->settings.verbs = mirror::DefaultVerbs;
}

MirrorManager& MirrorManager::getInstance()
{
    return *mirrorManager();
}

void MirrorManager::initialize()
{
    load();
    apply();
}

void MirrorManager::load()
{
    Settings s;
    s.enabled = (readConfig("MirrorEnabled", "false") == "true");
    s.sourceDevice = readConfig("MirrorSourceDevice", "");
    s.sourceChannel = readConfig("MirrorSourceChannel", "1").toInt();
    s.targetDevice = readConfig("MirrorTargetDevice", "");
    s.targetChannel = readConfig("MirrorTargetChannel", "2").toInt();
    s.verbs = verbsFromString(readConfig("MirrorVerbs", "PLAY,LOAD,STOP"));
    if (s.verbs == 0)
        s.verbs = mirror::DefaultVerbs;

    this->settings = s;
}

void MirrorManager::save() const
{
    writeConfig("MirrorEnabled", this->settings.enabled ? "true" : "false");
    writeConfig("MirrorSourceDevice", this->settings.sourceDevice);
    writeConfig("MirrorSourceChannel", QString::number(this->settings.sourceChannel));
    writeConfig("MirrorTargetDevice", this->settings.targetDevice);
    writeConfig("MirrorTargetChannel", QString::number(this->settings.targetChannel));
    writeConfig("MirrorVerbs", verbsToString(this->settings.verbs));
}

MirrorManager::Settings MirrorManager::getSettings() const
{
    return this->settings;
}

QString MirrorManager::validate(const Settings& s) const
{
    if (s.sourceDevice.isEmpty() || s.targetDevice.isEmpty())
        return tr("Selecione o servidor de origem e o de destino.");

    if (s.sourceChannel < 1 || s.targetChannel < 1)
        return tr("Canal invalido.");

    if (s.sourceDevice == s.targetDevice && s.sourceChannel == s.targetChannel)
        return tr("Origem e destino nao podem ser o mesmo servidor/canal.");

    if (s.verbs == 0)
        return tr("Selecione pelo menos um comando para espelhar.");

    return QString();
}

void MirrorManager::setSettings(const Settings& s)
{
    this->settings = s;
    save();
    apply();

    emit settingsChanged();
}

void MirrorManager::setEnabled(bool enabled)
{
    Settings s = this->settings;
    s.enabled = enabled;

    // So liga se a configuracao for valida.
    if (enabled && !validate(s).isEmpty())
        return;

    setSettings(s);
}

void MirrorManager::apply()
{
    DeviceManager& dm = DeviceManager::getInstance();

    // Limpa qualquer regra anterior em todos os dispositivos.
    foreach (const DeviceModel& model, dm.getDeviceModels())
    {
        const QSharedPointer<CasparDevice> device = dm.getDeviceByName(model.getName());
        if (device)
            device->clearMirror();
    }

    if (!this->settings.enabled || !validate(this->settings).isEmpty())
        return;

    const QSharedPointer<CasparDevice> source = dm.getDeviceByName(this->settings.sourceDevice);
    const QSharedPointer<CasparDevice> target = dm.getDeviceByName(this->settings.targetDevice);
    if (!source || !target)
        return;

    mirror::Rule rule;
    rule.enabled = true;
    rule.sourceChannel = this->settings.sourceChannel;
    rule.targetChannel = this->settings.targetChannel;
    rule.verbs = this->settings.verbs;

    // Origem e destino podem ser o mesmo dispositivo (canal 1 -> canal 2)
    // ou dispositivos distintos (S1/C1 -> S2/C2).
    source->setMirror(target.data(), rule);
}

bool MirrorManager::isActive() const
{
    if (!this->settings.enabled)
        return false;

    const QSharedPointer<CasparDevice> source = DeviceManager::getInstance().getDeviceByName(this->settings.sourceDevice);
    return source && source->isMirrorActive();
}

bool MirrorManager::isSourceConnected() const
{
    const QSharedPointer<CasparDevice> d = DeviceManager::getInstance().getDeviceByName(this->settings.sourceDevice);
    return d && d->isConnected();
}

bool MirrorManager::isTargetConnected() const
{
    const QSharedPointer<CasparDevice> d = DeviceManager::getInstance().getDeviceByName(this->settings.targetDevice);
    return d && d->isConnected();
}

quint64 MirrorManager::getMirroredCount() const
{
    const QSharedPointer<CasparDevice> d = DeviceManager::getInstance().getDeviceByName(this->settings.sourceDevice);
    return d ? d->getMirroredCount() : 0;
}

quint64 MirrorManager::getDroppedCount() const
{
    const QSharedPointer<CasparDevice> d = DeviceManager::getInstance().getDeviceByName(this->settings.sourceDevice);
    return d ? d->getMirrorDroppedCount() : 0;
}

QStringList MirrorManager::verbNames()
{
    QStringList names;
    for (const VerbName& v : verbTable)
        names << QString::fromLatin1(v.name);
    return names;
}

uint32_t MirrorManager::verbsFromString(const QString& csv)
{
    uint32_t mask = 0;
    foreach (const QString& part, csv.split(',', Qt::SkipEmptyParts))
    {
        const QString name = part.trimmed().toUpper();
        for (const VerbName& v : verbTable)
        {
            if (name == QLatin1String(v.name))
                mask |= v.bit;
        }
    }
    return mask;
}

QString MirrorManager::verbsToString(uint32_t verbs)
{
    QStringList names;
    for (const VerbName& v : verbTable)
    {
        if (verbs & v.bit)
            names << QString::fromLatin1(v.name);
    }
    return names.join(',');
}
