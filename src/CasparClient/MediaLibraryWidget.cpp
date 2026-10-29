#include "MediaLibraryWidget.h"

#include "ConnectionManager.h"
#include "RundownModel.h"

#include <QtCore/QMap>
#include <QtCore/QMimeData>
#include <QtCore/QSignalBlocker>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

namespace
{
    constexpr int MediaNameRole = Qt::UserRole + 1;

    // Modelo da biblioteca com o formato de arrastar entendido pelo Rundown.
    class MediaItemModel : public QStandardItemModel
    {
        public:
            using QStandardItemModel::QStandardItemModel;

            QStringList mimeTypes() const override
            {
                return { RundownModel::MediaMimeType };
            }

            QMimeData* mimeData(const QModelIndexList& indexes) const override
            {
                QStringList names;
                for (const QModelIndex& index : indexes)
                {
                    if (index.column() != 0)
                        continue;
                    const QString name = index.data(MediaNameRole).toString();
                    if (!name.isEmpty() && !names.contains(name))
                        names << name;
                }
                if (names.isEmpty())
                    return nullptr;
                QMimeData* data = new QMimeData();
                data->setData(RundownModel::MediaMimeType, names.join('\n').toUtf8());
                return data;
            }
    };
}

MediaLibraryWidget::MediaLibraryWidget(ConnectionManager* connections, QWidget* parent)
    : QWidget(parent), connections(connections)
{
    this->server = new QComboBox(this);
    QPushButton* refresh = new QPushButton(tr("Atualizar"), this);
    this->search = new QLineEdit(this);
    this->search->setPlaceholderText(tr("Buscar mídia..."));
    this->search->setClearButtonEnabled(true);

    this->items = new MediaItemModel(this);
    this->items->setHorizontalHeaderLabels({ tr("Nome"), tr("Tipo"), tr("Duração") });

    this->filter = new QSortFilterProxyModel(this);
    this->filter->setSourceModel(this->items);
    this->filter->setFilterCaseSensitivity(Qt::CaseInsensitive);
    this->filter->setRecursiveFilteringEnabled(true);
    this->filter->setSortCaseSensitivity(Qt::CaseInsensitive);

    this->view = new QTreeView(this);
    this->view->setModel(this->filter);
    this->view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->view->setDragEnabled(true);
    this->view->setDragDropMode(QAbstractItemView::DragOnly);
    this->view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->view->setSortingEnabled(true);
    this->view->sortByColumn(0, Qt::AscendingOrder);
    this->view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    this->view->header()->setStretchLastSection(false);

    QHBoxLayout* top = new QHBoxLayout();
    top->addWidget(this->server, 1);
    top->addWidget(refresh);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(top);
    layout->addWidget(this->search);
    layout->addWidget(this->view, 1);

    QObject::connect(this->search, &QLineEdit::textChanged, this, [this](const QString& text) {
        this->filter->setFilterFixedString(text);
        if (!text.isEmpty())
            this->view->expandAll();
    });
    QObject::connect(this->server, &QComboBox::currentTextChanged, this, &MediaLibraryWidget::reloadMedia);
    QObject::connect(refresh, &QPushButton::clicked, this, [this]() {
        this->connections->refreshMedia(this->server->currentText());
    });
    QObject::connect(connections, &ConnectionManager::serversChanged, this, &MediaLibraryWidget::reloadServers);
    QObject::connect(connections, &ConnectionManager::mediaChanged, this, [this](const QString& name) {
        if (name == this->server->currentText())
            reloadMedia();
    });
    QObject::connect(this->view, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        const QString name = index.sibling(index.row(), 0).data(MediaNameRole).toString();
        if (!name.isEmpty())
            emit mediaActivated({ name });
    });

    reloadServers();
}

QStringList MediaLibraryWidget::selectedMedia() const
{
    QStringList names;
    for (const QModelIndex& index : this->view->selectionModel()->selectedRows(0))
    {
        const QString name = index.data(MediaNameRole).toString();
        if (!name.isEmpty())
            names << name;
    }
    return names;
}

void MediaLibraryWidget::reloadServers()
{
    const QString selected = this->server->currentText();
    {
        const QSignalBlocker blocker(this->server);
        this->server->clear();
        this->server->addItems(this->connections->serverNames());
        const int index = this->server->findText(selected);
        if (index >= 0)
            this->server->setCurrentIndex(index);
    }
    reloadMedia();
}

void MediaLibraryWidget::reloadMedia()
{
    // Com a ordenação ligada, cada linha inserida reordenaria a árvore inteira.
    this->view->setSortingEnabled(false);
    this->filter->setDynamicSortFilter(false);
    this->items->removeRows(0, this->items->rowCount());

    QMap<QString, QStandardItem*> folders;  // caminho da pasta -> item
    for (const CasparMedia& media : this->connections->media(this->server->currentText()))
    {
        const QStringList parts = media.getName().split('/', Qt::SkipEmptyParts);
        if (parts.isEmpty())
            continue;

        QStandardItem* parent = this->items->invisibleRootItem();
        QString path;
        for (int i = 0; i < parts.count() - 1; ++i)
        {
            path += (path.isEmpty() ? QString() : QString("/")) + parts.at(i);
            QStandardItem* folder = folders.value(path);
            if (folder == nullptr)
            {
                folder = new QStandardItem(parts.at(i));
                folder->setEditable(false);
                folder->setDragEnabled(false);
                QStandardItem* typeCell = new QStandardItem(tr("Pasta"));
                typeCell->setEditable(false);
                typeCell->setDragEnabled(false);
                QStandardItem* emptyCell = new QStandardItem();
                emptyCell->setEditable(false);
                emptyCell->setDragEnabled(false);
                parent->appendRow({ folder, typeCell, emptyCell });
                folders.insert(path, folder);
            }
            parent = folder;
        }

        QStandardItem* name = new QStandardItem(parts.last());
        name->setData(media.getName(), MediaNameRole);
        name->setToolTip(media.getName());
        QStandardItem* type = new QStandardItem(media.getType());
        QStandardItem* duration = new QStandardItem(media.getTimecode());
        for (QStandardItem* cell : { name, type, duration })
        {
            cell->setEditable(false);
            cell->setDragEnabled(true);
        }
        parent->appendRow({ name, type, duration });
    }

    this->filter->setDynamicSortFilter(true);
    this->view->setSortingEnabled(true);
    if (!this->search->text().isEmpty())
        this->view->expandAll();
}
