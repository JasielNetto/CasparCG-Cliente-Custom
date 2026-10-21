#pragma once

#include "Shared.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <cstdint>

// Mirror: replica PLAY / LOAD / STOP (e opcionalmente outros verbos) do
// Servidor/Canal de origem para o Servidor/Canal de destino, no nivel de
// transporte (AmcpDevice::writeMessage). Substitui o antigo "Shadow server".
class CORE_EXPORT MirrorManager : public QObject
{
    Q_OBJECT

    public:
        struct Settings
        {
            bool enabled = false;
            QString sourceDevice;
            int sourceChannel = 1;
            QString targetDevice;
            int targetChannel = 2;
            uint32_t verbs = 0; // mascara mirror::Verb
        };

        explicit MirrorManager();

        static MirrorManager& getInstance();

        void initialize();
        void apply();   // (re)liga a regra nos dispositivos atuais

        Settings getSettings() const;
        void setSettings(const Settings& settings); // valida, grava no banco e aplica
        void setEnabled(bool enabled);

        // Retorna string vazia se valido, senao a mensagem de erro.
        QString validate(const Settings& settings) const;

        bool isActive() const;
        bool isSourceConnected() const;
        bool isTargetConnected() const;
        quint64 getMirroredCount() const;
        quint64 getDroppedCount() const;

        static QStringList verbNames();                       // todos os verbos conhecidos
        static uint32_t verbsFromString(const QString& csv);  // "PLAY,LOAD,STOP" -> mascara
        static QString verbsToString(uint32_t verbs);

        Q_SIGNAL void settingsChanged();

    private:
        Settings settings;

        void load();
        void save() const;
};
