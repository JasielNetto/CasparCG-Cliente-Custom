#include "StationDialog.h"

#include <QtCore/QSignalBlocker>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

namespace
{
    enum ServerColumn { ColumnName = 0, ColumnHost, ColumnPort };
}

StationDialog::StationDialog(const engine::StationConfig& config, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Estação"));
    resize(620, 520);

    // Servidores
    QGroupBox* serverBox = new QGroupBox(tr("Servidores CasparCG"), this);
    this->servers = new QTableWidget(0, 3, serverBox);
    this->servers->setHorizontalHeaderLabels({ tr("Nome"), tr("Endereço"), tr("Porta AMCP") });
    this->servers->horizontalHeader()->setSectionResizeMode(ColumnHost, QHeaderView::Stretch);
    this->servers->verticalHeader()->hide();
    this->servers->setSelectionBehavior(QAbstractItemView::SelectRows);

    QPushButton* addServer = new QPushButton(tr("Adicionar"), serverBox);
    QPushButton* removeServer = new QPushButton(tr("Remover"), serverBox);
    QHBoxLayout* serverButtons = new QHBoxLayout();
    serverButtons->addWidget(addServer);
    serverButtons->addWidget(removeServer);
    serverButtons->addStretch();

    QVBoxLayout* serverLayout = new QVBoxLayout(serverBox);
    serverLayout->addWidget(this->servers);
    serverLayout->addLayout(serverButtons);

    // Recursos
    QGroupBox* resourceBox = new QGroupBox(tr("Recursos de playout"), this);
    QGridLayout* resourceLayout = new QGridLayout(resourceBox);
    resourceLayout->addWidget(new QLabel(tr("Servidor"), resourceBox), 0, 1);
    resourceLayout->addWidget(new QLabel(tr("Canal"), resourceBox), 0, 2);
    resourceLayout->addWidget(new QLabel(tr("Layer"), resourceBox), 0, 3);

    int row = 1;
    for (engine::LogicalResource resource : engine::AllResources)
    {
        ResourceRow widgets;
        widgets.server = new QComboBox(resourceBox);
        widgets.channel = new QSpinBox(resourceBox);
        widgets.channel->setRange(1, 99);
        widgets.layer = new QSpinBox(resourceBox);
        widgets.layer->setRange(0, 9999);

        const std::optional<engine::Endpoint> endpoint = config.resources.find(resource);
        if (endpoint)
        {
            widgets.server->addItem(QString::fromStdString(endpoint->server));
            widgets.channel->setValue(endpoint->channel);
            widgets.layer->setValue(endpoint->layer);
        }

        resourceLayout->addWidget(new QLabel(QString::fromUtf8(engine::toString(resource)), resourceBox), row, 0);
        resourceLayout->addWidget(widgets.server, row, 1);
        resourceLayout->addWidget(widgets.channel, row, 2);
        resourceLayout->addWidget(widgets.layer, row, 3);
        resourceLayout->setColumnStretch(1, 1);
        this->resourceRows.insert(resource, widgets);
        ++row;

        QObject::connect(widgets.server, &QComboBox::currentTextChanged, this, &StationDialog::validate);
        QObject::connect(widgets.channel, QOverload<int>::of(&QSpinBox::valueChanged), this, &StationDialog::validate);
        QObject::connect(widgets.layer, QOverload<int>::of(&QSpinBox::valueChanged), this, &StationDialog::validate);
    }

    // OSC
    QGroupBox* oscBox = new QGroupBox(tr("OSC"), this);
    QFormLayout* oscLayout = new QFormLayout(oscBox);
    this->oscPort = new QSpinBox(oscBox);
    this->oscPort->setRange(1, 65535);
    this->oscPort->setValue(config.oscPort);
    oscLayout->addRow(tr("Porta de recepção"), this->oscPort);
    oscLayout->addRow(new QLabel(tr("Se o cliente antigo rodar no mesmo computador, use outra porta."), oscBox));

    this->issues = new QLabel(this);
    this->issues->setWordWrap(true);
    this->issues->setStyleSheet("color: #e0574f;");

    this->buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(serverBox, 1);
    layout->addWidget(resourceBox);
    layout->addWidget(oscBox);
    layout->addWidget(this->issues);
    layout->addWidget(this->buttons);

    for (const engine::ServerConfig& server : config.servers)
        addServerRow(server);
    refreshServerChoices();

    // Restaura a seleção dos servidores depois de preencher as opções.
    for (engine::LogicalResource resource : engine::AllResources)
    {
        const std::optional<engine::Endpoint> endpoint = config.resources.find(resource);
        if (endpoint)
            this->resourceRows[resource].server->setCurrentText(QString::fromStdString(endpoint->server));
    }

    QObject::connect(addServer, &QPushButton::clicked, this, [this]() {
        addServerRow({ QString(tr("Playout %1")).arg(this->servers->rowCount() + 1).toStdString(), "127.0.0.1", 5250 });
        refreshServerChoices();
        validate();
    });
    QObject::connect(removeServer, &QPushButton::clicked, this, [this]() {
        const int current = this->servers->currentRow();
        if (current >= 0)
            this->servers->removeRow(current);
        refreshServerChoices();
        validate();
    });
    QObject::connect(this->servers, &QTableWidget::cellChanged, this, [this](int, int column) {
        if (column == ColumnName)
            refreshServerChoices();
        validate();
    });
    QObject::connect(this->oscPort, QOverload<int>::of(&QSpinBox::valueChanged), this, &StationDialog::validate);
    QObject::connect(this->buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(this->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    validate();
}

void StationDialog::addServerRow(const engine::ServerConfig& server)
{
    const QSignalBlocker blocker(this->servers);
    const int row = this->servers->rowCount();
    this->servers->insertRow(row);
    this->servers->setItem(row, ColumnName, new QTableWidgetItem(QString::fromStdString(server.name)));
    this->servers->setItem(row, ColumnHost, new QTableWidgetItem(QString::fromStdString(server.host)));
    this->servers->setItem(row, ColumnPort, new QTableWidgetItem(QString::number(server.port)));
}

void StationDialog::refreshServerChoices()
{
    QStringList names;
    for (int row = 0; row < this->servers->rowCount(); ++row)
    {
        const QTableWidgetItem* item = this->servers->item(row, ColumnName);
        if (item != nullptr && !item->text().trimmed().isEmpty())
            names << item->text().trimmed();
    }

    for (ResourceRow& widgets : this->resourceRows)
    {
        const QSignalBlocker blocker(widgets.server);
        const QString selected = widgets.server->currentText();
        widgets.server->clear();
        widgets.server->addItems(names);
        if (!names.contains(selected) && !selected.isEmpty())
            widgets.server->addItem(selected);  // mantém o valor; a validação aponta o servidor inexistente
        widgets.server->setCurrentText(selected);
    }
}

engine::StationConfig StationDialog::config() const
{
    engine::StationConfig result;
    for (int row = 0; row < this->servers->rowCount(); ++row)
    {
        engine::ServerConfig server;
        const QTableWidgetItem* name = this->servers->item(row, ColumnName);
        const QTableWidgetItem* host = this->servers->item(row, ColumnHost);
        const QTableWidgetItem* port = this->servers->item(row, ColumnPort);
        server.name = name ? name->text().trimmed().toStdString() : std::string();
        server.host = host ? host->text().trimmed().toStdString() : std::string();
        bool ok = false;
        server.port = port ? port->text().trimmed().toInt(&ok) : 0;
        if (!ok)
            server.port = 0;
        result.servers.push_back(server);
    }

    for (auto it = this->resourceRows.cbegin(); it != this->resourceRows.cend(); ++it)
    {
        const QString server = it->server->currentText().trimmed();
        if (server.isEmpty())
            continue;
        result.resources.set(it.key(), { server.toStdString(), it->channel->value(), it->layer->value() });
    }

    result.oscPort = this->oscPort->value();
    return result;
}

void StationDialog::validate()
{
    const std::vector<engine::ConfigIssue> found = config().validate();

    QStringList lines;
    for (const engine::ConfigIssue& issue : found)
        lines << QString("• %1%2").arg(QString::fromUtf8(engine::toString(issue.error)),
                                         issue.subject.empty() ? QString() : QString(" — %1").arg(QString::fromStdString(issue.subject)));
    this->issues->setText(lines.join('\n'));
    this->buttons->button(QDialogButtonBox::Save)->setEnabled(found.empty());
}
