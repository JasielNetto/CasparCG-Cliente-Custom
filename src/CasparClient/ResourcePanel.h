#pragma once

// Painel de um recurso (VT, Selo 1 ou Selo 2): destino configurado, conexão, o que está no ar e
// tempo decorrido / restante. Só acompanhamento: o transporte fica na barra do Rundown.

#include "Resource.h"
#include "Rundown.h"

#include <QtCore/QMetaObject>
#include <QtWidgets/QFrame>

class ConnectionManager;
class ExecutionController;
class QLabel;
class QProgressBar;
class RundownModel;

class ResourcePanel : public QFrame
{
    Q_OBJECT

    public:
        ResourcePanel(engine::LogicalResource resource, ExecutionController* execution, ConnectionManager* connections,
                      RundownModel* model, QWidget* parent = nullptr);

        engine::LogicalResource resource() const { return this->target; }

    private:
        engine::LogicalResource target;
        ExecutionController* execution = nullptr;
        ConnectionManager* connections = nullptr;
        RundownModel* model = nullptr;

        QLabel* title = nullptr;
        QLabel* destination = nullptr;
        QLabel* connection = nullptr;
        QLabel* item = nullptr;
        QLabel* clip = nullptr;
        QLabel* elapsed = nullptr;
        QLabel* remaining = nullptr;
        QLabel* paused = nullptr;
        QProgressBar* progress = nullptr;

        QList<QMetaObject::Connection> monitorConnections;

        void attachMonitor();
        void refreshDestination();
        void refreshItem();
        void showTime(double elapsedSeconds, double totalSeconds);
};
