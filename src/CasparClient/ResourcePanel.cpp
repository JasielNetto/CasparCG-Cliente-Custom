#include "ResourcePanel.h"

#include "ConnectionManager.h"
#include "ExecutionController.h"
#include "RundownModel.h"

#include "OscLayerMonitor.h"
#include "TimeFormat.h"

#include <QtCore/QList>
#include <QtGui/QAction>
#include <QtGui/QFont>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QVBoxLayout>

ResourcePanel::ResourcePanel(engine::LogicalResource resource, ExecutionController* execution, ConnectionManager* connections,
                             RundownModel* model, QWidget* parent)
    : QFrame(parent), target(resource), execution(execution), connections(connections), model(model)
{
    setFrameShape(QFrame::StyledPanel);

    this->title = new QLabel(QString::fromUtf8(engine::toString(resource)), this);
    QFont titleFont = this->title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
    this->title->setFont(titleFont);

    this->connection = new QLabel(this);
    this->destination = new QLabel(this);
    this->destination->setStyleSheet("color: gray;");

    this->item = new QLabel(this);
    this->item->setWordWrap(true);
    this->clip = new QLabel(this);
    this->clip->setStyleSheet("color: gray;");

    this->elapsed = new QLabel(this);
    this->remaining = new QLabel(this);
    QFont timeFont("monospace");
    timeFont.setStyleHint(QFont::Monospace);
    timeFont.setPointSizeF(titleFont.pointSizeF());
    this->elapsed->setFont(timeFont);
    this->remaining->setFont(timeFont);
    this->remaining->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    this->paused = new QLabel(tr("PAUSADO"), this);
    this->paused->setStyleSheet("color: #e0a800; font-weight: bold;");
    this->paused->hide();

    this->progress = new QProgressBar(this);
    this->progress->setRange(0, 1000);
    this->progress->setTextVisible(false);
    this->progress->setMaximumHeight(8);

    // Transporte fora da tela: só no clique com o botão direito sobre o painel.
    QAction* play = new QAction(tr("Play (retoma a pausa ou dispara o preparado)"), this);
    QAction* pause = new QAction(tr("Pause"), this);
    QAction* stop = new QAction(tr("Stop"), this);
    QAction* clear = new QAction(tr("Clear"), this);
    addActions({ play, pause, stop, clear });
    setContextMenuPolicy(Qt::ActionsContextMenu);
    setToolTip(tr("Botão direito: Play, Pause, Stop e Clear de %1").arg(QString::fromUtf8(engine::toString(resource))));

    QHBoxLayout* header = new QHBoxLayout();
    header->addWidget(this->title);
    header->addStretch();
    header->addWidget(this->connection);

    QHBoxLayout* times = new QHBoxLayout();
    times->addWidget(this->elapsed);
    times->addWidget(this->paused);
    times->addStretch();
    times->addWidget(this->remaining);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(header);
    layout->addWidget(this->destination);
    layout->addWidget(this->item);
    layout->addWidget(this->clip);
    layout->addLayout(times);
    layout->addWidget(this->progress);

    QObject::connect(play, &QAction::triggered, this, [this]() { this->execution->play(this->target); });
    QObject::connect(pause, &QAction::triggered, this, [this]() { this->execution->pause(this->target); });
    QObject::connect(stop, &QAction::triggered, this, [this]() { this->execution->stopResource(this->target); });
    QObject::connect(clear, &QAction::triggered, this, [this]() { this->execution->clearResource(this->target); });

    QObject::connect(execution, &ExecutionController::monitorsChanged, this, [this]() {
        attachMonitor();
        refreshDestination();
    });
    QObject::connect(execution, &ExecutionController::onAirChanged, this, [this](engine::LogicalResource changed, engine::NodeId) {
        if (changed == this->target)
            refreshItem();
    });
    QObject::connect(connections, &ConnectionManager::connectionChanged, this, [this](const QString&, bool) { refreshDestination(); });
    QObject::connect(model, &QAbstractItemModel::dataChanged, this, [this]() { refreshItem(); });
    QObject::connect(model, &QAbstractItemModel::modelReset, this, [this]() { refreshItem(); });

    attachMonitor();
    refreshDestination();
    refreshItem();
    showTime(0.0, 0.0);
}

void ResourcePanel::attachMonitor()
{
    for (const QMetaObject::Connection& c : this->monitorConnections)
        QObject::disconnect(c);
    this->monitorConnections.clear();

    OscLayerMonitor* monitor = this->execution->monitor(this->target);
    if (monitor == nullptr)
    {
        this->clip->clear();
        this->paused->hide();
        showTime(0.0, 0.0);
        return;
    }

    this->monitorConnections << QObject::connect(monitor, &OscLayerMonitor::timeChanged, this, &ResourcePanel::showTime);
    this->monitorConnections << QObject::connect(monitor, &OscLayerMonitor::clipNameChanged, this, [this](const QString& name) {
        this->clip->setText(name);
    });
    this->monitorConnections << QObject::connect(monitor, &OscLayerMonitor::pausedChanged, this, [this](bool value) {
        this->paused->setVisible(value);
    });
    this->clip->setText(monitor->clipName());
    this->paused->setVisible(monitor->isPaused());
    showTime(monitor->elapsed(), monitor->total());
}

void ResourcePanel::refreshDestination()
{
    const std::optional<engine::Endpoint> endpoint = this->execution->resources().find(this->target);
    if (!endpoint)
    {
        this->destination->setText(tr("Não configurado"));
        this->connection->setText(QString());
        return;
    }

    const QString server = QString::fromStdString(endpoint->server);
    this->destination->setText(tr("%1 · canal %2 · layer %3").arg(server).arg(endpoint->channel).arg(endpoint->layer));

    const bool online = this->connections->isConnected(server);
    const bool osc = this->execution->monitor(this->target) != nullptr && this->connections->isOscListening();
    if (!online)
    {
        this->connection->setText(tr("● desconectado"));
        this->connection->setStyleSheet("color: #e0574f;");
    }
    else if (!osc)
    {
        this->connection->setText(tr("● sem OSC"));
        this->connection->setStyleSheet("color: #e0a800;");
        this->connection->setToolTip(tr("Sem OSC não há tempo no painel nem avanço automático de Sequência"));
    }
    else
    {
        this->connection->setText(tr("● conectado"));
        this->connection->setStyleSheet("color: #3fb950;");
        this->connection->setToolTip(QString());
    }
}

void ResourcePanel::refreshItem()
{
    const engine::NodeId id = this->execution->onAir(this->target);
    const engine::Node* node = this->model->rundown().find(id);
    this->item->setText(node == nullptr ? tr("—") : QString::fromStdString(node->label));
    setStyleSheet(node == nullptr ? QString() : "ResourcePanel { border: 2px solid #aa1919; }");
}

void ResourcePanel::showTime(double elapsedSeconds, double totalSeconds)
{
    const OscLayerMonitor* monitor = this->execution->monitor(this->target);
    const double fps = monitor != nullptr ? monitor->fps() : 0.0;

    this->elapsed->setText(QString::fromStdString(engine::formatTime(elapsedSeconds, fps)));
    this->remaining->setText(totalSeconds > 0.0 ? "-" + QString::fromStdString(engine::formatTime(engine::remainingTime(elapsedSeconds, totalSeconds), fps))
                                                : QString());
    const int permille = totalSeconds > 0.0 ? static_cast<int>(1000.0 * elapsedSeconds / totalSeconds) : 0;
    this->progress->setValue(qBound(0, permille, 1000));

    // Últimos 10 segundos em destaque.
    const bool ending = totalSeconds > 0.0 && engine::remainingTime(elapsedSeconds, totalSeconds) <= 10.0;
    this->remaining->setStyleSheet(ending ? "color: #e0574f; font-weight: bold;" : QString());
}
