#include "MainWindow.h"

#include "ConnectionManager.h"
#include "ExecutionController.h"
#include "MediaLibraryWidget.h"
#include "ResourcePanel.h"
#include "RundownModel.h"
#include "RundownStore.h"
#include "RundownWidget.h"
#include "StationDialog.h"
#include "StationStore.h"

#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtGui/QAction>
#include <QtGui/QCloseEvent>
#include <QtGui/QKeySequence>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>

namespace
{
    const char* const RundownFilter = "Rundown CasparClient (*.ccrundown);;JSON (*.json)";
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    this->connections = new ConnectionManager(this);
    this->model = new RundownModel(this);
    this->execution = new ExecutionController(this->connections, this->model, this);

    this->library = new MediaLibraryWidget(this->connections, this);
    this->rundown = new RundownWidget(this->model, this->execution, this);

    QWidget* panelColumn = new QWidget(this);
    QVBoxLayout* panelLayout = new QVBoxLayout(panelColumn);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    for (engine::LogicalResource resource : engine::AllResources)
    {
        ResourcePanel* panel = new ResourcePanel(resource, this->execution, this->connections, this->model, panelColumn);
        panelLayout->addWidget(panel);
        this->panels << panel;
    }
    panelLayout->addStretch();

    this->splitter = new QSplitter(Qt::Horizontal, this);
    this->splitter->addWidget(this->library);
    this->splitter->addWidget(this->rundown);
    this->splitter->addWidget(panelColumn);
    this->splitter->setStretchFactor(0, 2);
    this->splitter->setStretchFactor(1, 5);
    this->splitter->setStretchFactor(2, 2);
    setCentralWidget(this->splitter);

    this->serverStatus = new QLabel(this);
    statusBar()->addPermanentWidget(this->serverStatus);

    buildMenus();

    QObject::connect(this->library, &MediaLibraryWidget::mediaActivated, this->rundown, &RundownWidget::addMedia);
    QObject::connect(this->execution, &ExecutionController::message, this, [this](const QString& text, bool isError) {
        statusBar()->showMessage(text, isError ? 15000 : 6000);
    });
    QObject::connect(this->connections, &ConnectionManager::connectionChanged, this, [this]() { refreshServerStatus(); });
    QObject::connect(this->connections, &ConnectionManager::serversChanged, this, [this]() { refreshServerStatus(); });
    QObject::connect(this->model, &RundownModel::modifiedChanged, this, [this]() { refreshTitle(); });
    QObject::connect(this->connections, &ConnectionManager::oscError, this, [this](const QString& text) {
        statusBar()->showMessage(text);  // fica até a próxima mensagem
        refreshServerStatus();
    });

    resize(1500, 860);
    restoreLayout();
    loadStation();
    refreshTitle();
}

MainWindow::~MainWindow()
{
    // A execução depende das conexões (porta AMCP, OSC): destruí-la primeiro, antes que a queda das
    // conexões dispare callbacks com a janela pela metade.
    this->execution->disconnect(this);
    delete this->execution;
    this->execution = nullptr;
}

void MainWindow::buildMenus()
{
    // Ações criadas à mão: a forma addAction(texto, atalho, objeto, função) não existe no Qt 6.2.
    auto add = [this](QMenu* menu, const QString& text, const QKeySequence& shortcut, auto&& slot) {
        QAction* action = menu->addAction(text);
        if (!shortcut.isEmpty())
            action->setShortcut(shortcut);
        QObject::connect(action, &QAction::triggered, this, slot);
        return action;
    };

    QMenu* file = menuBar()->addMenu(tr("&Arquivo"));
    add(file, tr("&Novo rundown"), QKeySequence(QKeySequence::New), [this]() { newRundown(); });
    add(file, tr("&Abrir..."), QKeySequence(QKeySequence::Open), [this]() { openRundownDialog(); });
    add(file, tr("&Salvar"), QKeySequence(QKeySequence::Save), [this]() { saveRundown(); });
    add(file, tr("Salvar &como..."), QKeySequence(QKeySequence::SaveAs), [this]() { saveRundownAs(); });
    file->addSeparator();
    add(file, tr("Sai&r"), QKeySequence(QKeySequence::Quit), [this]() { close(); });

    QMenu* rundownMenu = menuBar()->addMenu(tr("&Rundown"));
    rundownMenu->addActions(this->rundown->playoutActions());
    rundownMenu->addSeparator();
    rundownMenu->addActions(this->rundown->editActions());

    QMenu* stationMenu = menuBar()->addMenu(tr("&Estação"));
    add(stationMenu, tr("&Configurar..."), QKeySequence(), [this]() { configureStation(); });

    QMenu* help = menuBar()->addMenu(tr("Aj&uda"));
    add(help, tr("&Sobre"), QKeySequence(), [this]() {
        QMessageBox::about(this, tr("CasparClient"),
                           tr("<b>CasparClient</b><br>Cliente de playout para CasparCG Server 2.4.<br><br>"
                              "F7 prepara · F8 dispara · F9 para<br>"
                              "Ctrl+1 Bloco · Ctrl+2 Sequência · Ctrl+3 Conjunto<br>"
                              "Alt+1 VT · Alt+2 Selo 1 · Alt+3 Selo 2 · Ctrl+L loop da Sequência"));
    });
}

void MainWindow::loadStation()
{
    QString error;
    if (!StationStore::load(this->station, &error))
    {
        QMessageBox::warning(this, tr("Estação"), tr("%1\n\nUsando a configuração padrão.").arg(error));
        this->station = StationStore::defaults();
    }

    // station.json pode ter sido editado à mão: mostrar o que não passa na validação.
    const std::vector<engine::ConfigIssue> issues = this->station.validate();
    if (!issues.empty())
    {
        QStringList lines;
        for (const engine::ConfigIssue& issue : issues)
            lines << QString("• %1 — %2").arg(QString::fromUtf8(engine::toString(issue.error)), QString::fromStdString(issue.subject));
        QMessageBox::warning(this, tr("Estação"), tr("A configuração da estação tem problemas:\n\n%1\n\nCorrija em Estação → Configurar.").arg(lines.join('\n')));
    }

    this->connections->apply(this->station);
    this->execution->setStation(this->station);
    refreshServerStatus();
}

void MainWindow::configureStation()
{
    StationDialog dialog(this->station, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const engine::StationConfig updated = dialog.config();
    QString error;
    if (!StationStore::save(updated, &error))
    {
        QMessageBox::critical(this, tr("Estação"), error);
        return;
    }

    this->station = updated;
    this->connections->apply(this->station);
    this->execution->setStation(this->station);
    refreshServerStatus();
    statusBar()->showMessage(tr("Configuração da estação salva"), 5000);
}

void MainWindow::refreshServerStatus()
{
    QStringList parts;
    for (const engine::ServerConfig& server : this->station.servers)
    {
        const QString name = QString::fromStdString(server.name);
        const bool online = this->connections->isConnected(name);
        const QString color = QString::fromLatin1(online ? "#3fb950" : "#e0574f");
        parts << QString("<span style='color:%1'>●</span> %2").arg(color, name.toHtmlEscaped());
    }
    parts << (this->connections->isOscListening() ? tr("OSC %1").arg(this->connections->oscPort())
                                                  : tr("<span style='color:#e0574f'>OSC %1 indisponível</span>").arg(this->connections->oscPort()));
    this->serverStatus->setText(parts.join("&nbsp;&nbsp;&nbsp;"));
}

void MainWindow::refreshTitle()
{
    const QString name = this->rundownPath.isEmpty() ? tr("Sem título") : QFileInfo(this->rundownPath).fileName();
    setWindowTitle(QString("%1[*] — CasparClient").arg(name));
    setWindowModified(this->model->isModified());
}

bool MainWindow::confirmDiscard()
{
    if (!this->model->isModified())
        return true;

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Rundown alterado"), tr("Salvar as alterações do rundown?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return saveRundown();
    return answer == QMessageBox::Discard;
}

void MainWindow::newRundown()
{
    if (!confirmDiscard())
        return;
    this->execution->reset();
    this->model->setRundown(engine::Rundown());
    this->rundownPath.clear();
    refreshTitle();
}

void MainWindow::openRundownDialog()
{
    if (!confirmDiscard())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Abrir rundown"), QFileInfo(this->rundownPath).absolutePath(), tr(RundownFilter));
    if (!path.isEmpty())
        openRundown(path);
}

bool MainWindow::openRundown(const QString& path)
{
    engine::Rundown loaded;
    QString error;
    if (!RundownStore::loadFile(path, loaded, &error))
    {
        QMessageBox::critical(this, tr("Abrir rundown"), tr("Não foi possível abrir %1:\n%2").arg(path, error));
        return false;
    }
    this->execution->reset();
    this->model->setRundown(loaded);
    this->rundownPath = path;
    refreshTitle();
    return true;
}

bool MainWindow::saveRundown()
{
    if (this->rundownPath.isEmpty())
        return saveRundownAs();

    QString error;
    if (!RundownStore::saveFile(this->rundownPath, this->model->rundown(), &error))
    {
        QMessageBox::critical(this, tr("Salvar rundown"), tr("Não foi possível salvar %1:\n%2").arg(this->rundownPath, error));
        return false;
    }
    this->model->setModified(false);
    statusBar()->showMessage(tr("Rundown salvo"), 4000);
    return true;
}

bool MainWindow::saveRundownAs()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Salvar rundown"), this->rundownPath, tr(RundownFilter));
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += ".ccrundown";
    this->rundownPath = path;
    return saveRundown();
}

void MainWindow::restoreLayout()
{
    QSettings settings;
    restoreGeometry(settings.value("window/geometry").toByteArray());
    this->splitter->restoreState(settings.value("window/splitter").toByteArray());
}

void MainWindow::saveLayout() const
{
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/splitter", this->splitter->saveState());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!confirmDiscard())
    {
        event->ignore();
        return;
    }
    saveLayout();
    event->accept();
}
