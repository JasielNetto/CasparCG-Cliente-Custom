#pragma once

#include "Shared.h"
#include "MirrorTranslator.h"

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QPointer>

class QObject;
class QTcpSocket;
class QTextDecoder;

class CASPAR_EXPORT AmcpDevice : public QObject
{
    Q_OBJECT

    public:
        explicit AmcpDevice(const QString& address, int port, QObject* parent = 0);
        virtual ~AmcpDevice();

        void disconnectDevice();

        void setDisableCommands(bool disable);

        bool isConnected() const;
        int getPort() const;
        const QString& getAddress() const;

        // --- Mirror (redundancia cruzada) ---
        // Faz com que os comandos habilitados em 'rule' enviados a ESTE dispositivo no canal de origem
        // tambem sejam enviados a 'target' no canal de destino, no mesmo instante (mesmo turno do loop de eventos).
        // Deve ser chamado e usado somente na thread da interface (como o restante do envio de comandos).
        void setMirror(AmcpDevice* target, const mirror::Rule& rule);
        void clearMirror();
        bool isMirrorActive() const;
        // true se um comando enviado a este dispositivo chega a algum servidor: ele proprio conectado,
        // ou (com Mirror ativo) o alvo do Mirror conectado. Garante o failover quando o principal cai.
        bool canSend() const;
        quint64 getMirroredCount() const;
        quint64 getMirrorDroppedCount() const;

        // --- Requisicao com identificador (CasparCG 2.2+) ---
        // Envia "REQ <id> <command>". A resposta "RES <id> ..." nao passa pelo tratamento padrao do
        // dispositivo: ela chega somente pelo sinal requestCompleted, com o mesmo id.
        // Retorna false (e nada e enviado) se este dispositivo nao estiver conectado.
        bool sendRequest(const QString& id, const QString& command);

        Q_SIGNAL void requestCompleted(const QString& id, int code, const QList<QString>& response);

        Q_SLOT void connectDevice();

    protected:
        enum class AmcpDeviceCommand
        {
            NONE,
            CONNECTIONSTATE,
            LOAD,
            LOADBG,
            PLAY,
            STOP,
            CG,
            CLS,
            CINF,
            VERSION,
            TLS,
            INFO,
            INFOSYSTEM,
            DATALIST,
            DATARETRIEVE,
            CLEAR,
            SET,
            MIXER,
            CALL,
            REMOVE,
            ADD,
            SWAP,
            STATUS,
            ERROR,
            THUMBNAILLIST,
            THUMBNAILRETRIEVE
        };

        QTcpSocket* socket = nullptr;
        AmcpDeviceCommand command = AmcpDeviceCommand::NONE;

        QList<QString> response;

        virtual void sendNotification() = 0;

        void resetDevice();
        void writeMessage(const QString& message);

    private:
        enum class AmcpDeviceParserState
        {
            ExpectingHeader,
            ExpectingOneline,
            ExpectingTwoline,
            ExpectingMultiline
        };

        QString address;

        int port;
        int code;

        bool connected = false;
        bool disableCommands = false;

        QPointer<AmcpDevice> mirrorTarget;
        mirror::Rule mirrorRule;
        quint64 mirroredCount = 0;
        quint64 mirrorDroppedCount = 0;

        // Escreve uma linha ja pronta (com \r\n) neste dispositivo em nome de outro (Mirror).
        bool mirrorWrite(const QByteArray& payload);

        QString fragments;

        QTextDecoder* decoder = nullptr;

        AmcpDeviceParserState state = AmcpDeviceParserState::ExpectingHeader;

        // Id da resposta "RES <id>" em leitura; vazio para respostas sem id.
        QString requestId;

        // Entrega a resposta completa: pelo sinal requestCompleted (com id) ou pelo fluxo padrao.
        void dispatchResponse();

        void parseLine(const QString& line);
        void parseHeader(const QString& line);
        void parseOneline(const QString& line);
        void parseTwoline(const QString& line);
        void parseMultiline(const QString& line);

        AmcpDeviceCommand translateCommand(const QString& command);

        Q_SLOT void readMessage();
        Q_SLOT void setConnected();
        Q_SLOT void setDisconnected();
};
