#include "RundownModel.h"

#include <QtCore/QMimeData>
#include <QtCore/QSet>
#include <QtGui/QBrush>
#include <QtGui/QColor>
#include <QtGui/QFont>

const char* const RundownModel::NodeMimeType = "application/x-casparclient-nodes";
const char* const RundownModel::MediaMimeType = "application/x-casparclient-media";

namespace
{
    QString resourceName(engine::LogicalResource resource)
    {
        return QString::fromUtf8(engine::toString(resource));
    }

    // Só para checar conflitos dentro do Conjunto (o destino real vem da estação, no disparo).
    engine::ResourceMap allMapped()
    {
        engine::ResourceMap map;
        for (engine::LogicalResource resource : engine::AllResources)
            map.set(resource, { "-", 1, 1 });
        return map;
    }
}

RundownModel::RundownModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

void RundownModel::setRundown(const engine::Rundown& rundown)
{
    beginResetModel();
    this->model = rundown;
    endResetModel();
    setModified(false);
}

void RundownModel::setModified(bool value)
{
    if (this->modified == value)
        return;
    this->modified = value;
    emit modifiedChanged(value);
}

void RundownModel::markModified()
{
    setModified(true);
}

engine::NodeId RundownModel::nodeId(const QModelIndex& index) const
{
    return index.isValid() ? static_cast<engine::NodeId>(index.internalId()) : engine::RootId;
}

const engine::Node* RundownModel::node(const QModelIndex& index) const
{
    return this->model.find(nodeId(index));
}

QModelIndex RundownModel::indexOf(engine::NodeId id, int column) const
{
    if (this->model.find(id) == nullptr)
        return QModelIndex();
    return createIndex(this->model.indexOf(id), column, static_cast<quintptr>(id));
}

QModelIndex RundownModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    const engine::NodeId parentId = nodeId(parent);
    const engine::Node* parentNode = parentId == engine::RootId ? &this->model.root() : this->model.find(parentId);
    if (parentNode == nullptr || row >= static_cast<int>(parentNode->children.size()))
        return QModelIndex();
    return createIndex(row, column, static_cast<quintptr>(parentNode->children[static_cast<std::size_t>(row)]));
}

QModelIndex RundownModel::parent(const QModelIndex& child) const
{
    const engine::Node* item = node(child);
    if (item == nullptr || item->parent == engine::RootId)
        return QModelIndex();
    return indexOf(item->parent);
}

int RundownModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;
    const engine::NodeId parentId = nodeId(parent);
    const engine::Node* parentNode = parentId == engine::RootId ? &this->model.root() : this->model.find(parentId);
    return parentNode == nullptr ? 0 : static_cast<int>(parentNode->children.size());
}

int RundownModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant RundownModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    switch (section)
    {
        case ColumnName: return tr("Nome");
        case ColumnResource: return tr("Recurso");
        case ColumnMedia: return tr("Mídia");
        case ColumnState: return tr("Estado");
        default: return QVariant();
    }
}

QVariant RundownModel::data(const QModelIndex& index, int role) const
{
    const engine::Node* item = node(index);
    if (item == nullptr)
        return QVariant();

    std::optional<engine::ValidationResult> conjuntoCheck;
    if (item->kind == engine::NodeKind::Conjunto)
    {
        const std::optional<engine::Conjunto> conjunto = this->model.toConjunto(item->id);
        if (conjunto)
            conjuntoCheck = engine::validate(*conjunto, allMapped());
    }
    const bool conflict = conjuntoCheck && !conjuntoCheck->ok();

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::EditRole:
            switch (index.column())
            {
                case ColumnName:
                    return QString::fromStdString(item->label);
                case ColumnResource:
                    if (item->kind == engine::NodeKind::Item)
                        return resourceName(item->item.resource);
                    if (item->kind == engine::NodeKind::Sequencia)
                        return item->repeat == engine::RepeatMode::Loop ? tr("Sequência · loop") : tr("Sequência · uma vez");
                    if (item->kind == engine::NodeKind::Conjunto)
                    {
                        QStringList names;
                        for (engine::NodeId child : item->children)
                            names << resourceName(this->model.find(child)->item.resource);
                        return tr("Conjunto") + (names.isEmpty() ? QString() : " · " + names.join(" + "));
                    }
                    return tr("Bloco");
                case ColumnMedia:
                    if (item->kind == engine::NodeKind::Item)
                        return QString::fromStdString(item->item.media) + (item->item.loop ? tr(" (loop)") : QString());
                    return tr("%n item(ns)", nullptr, static_cast<int>(this->model.itemsOf(item->id).size()));
                case ColumnState:
                    if (item->state == engine::NodeState::Idle && conflict)
                        return tr("Conflito de recurso");
                    return QString::fromUtf8(engine::toString(item->state));
            }
            break;

        case Qt::ToolTipRole:
            if (!item->error.empty())
                return QString::fromStdString(item->error);
            if (conflict)
                return tr("Cada recurso (VT, Selo 1, Selo 2) só pode aparecer uma vez no Conjunto");
            break;

        case Qt::FontRole:
            if (item->kind != engine::NodeKind::Item)
            {
                QFont font;
                font.setBold(true);
                return font;
            }
            break;

        case Qt::BackgroundRole:
            switch (item->state)
            {
                case engine::NodeState::OnAir: return QBrush(QColor(170, 25, 25));
                case engine::NodeState::Prepared: return QBrush(QColor(196, 140, 0));
                case engine::NodeState::Preparing: return QBrush(QColor(110, 85, 20));
                default: break;
            }
            break;

        case Qt::ForegroundRole:
            switch (item->state)
            {
                case engine::NodeState::OnAir: return QBrush(Qt::white);
                case engine::NodeState::Prepared: return QBrush(Qt::black);
                case engine::NodeState::Error: return QBrush(QColor(235, 70, 70));
                case engine::NodeState::Finished: return QBrush(QColor(130, 130, 130));
                default: break;
            }
            if (conflict)
                return QBrush(QColor(230, 120, 40));
            break;
    }
    return QVariant();
}

Qt::ItemFlags RundownModel::flags(const QModelIndex& index) const
{
    const engine::Node* item = node(index);
    if (item == nullptr)
        return Qt::ItemIsDropEnabled;  // raiz

    Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    if (index.column() == ColumnName)
        result |= Qt::ItemIsEditable;
    if (item->kind != engine::NodeKind::Item)
        result |= Qt::ItemIsDropEnabled;
    return result;
}

bool RundownModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || index.column() != ColumnName)
        return false;
    const QString label = value.toString().trimmed();
    if (label.isEmpty() || !this->model.setLabel(nodeId(index), label.toStdString()))
        return false;
    emit dataChanged(index, index);
    markModified();
    return true;
}

void RundownModel::setState(engine::NodeId id, engine::NodeState state, const QString& error)
{
    if (!this->model.setState(id, state, error.toStdString()))
        return;
    emit dataChanged(indexOf(id, 0), indexOf(id, ColumnCount - 1));
}

void RundownModel::resetStates()
{
    this->model.resetStates();
    for (engine::NodeId id : this->model.flatten())
        emit dataChanged(indexOf(id, 0), indexOf(id, ColumnCount - 1));
}

bool RundownModel::resolveAnchor(const QModelIndex& anchor, engine::NodeKind kind, engine::NodeId& targetParent, int& targetRow) const
{
    const engine::Node* item = node(anchor);
    if (item == nullptr)
    {
        targetParent = engine::RootId;
        targetRow = -1;
        return this->model.canAdd(engine::RootId, kind);
    }

    // Dentro do selecionado, se ele for um grupo que aceita o tipo.
    if (item->kind != engine::NodeKind::Item && this->model.canAdd(item->id, kind))
    {
        targetParent = item->id;
        targetRow = -1;
        return true;
    }

    // Logo depois do selecionado; se o pai não aceitar, logo depois do pai.
    engine::NodeId current = item->id;
    while (current != engine::RootId)
    {
        const engine::Node* currentNode = this->model.find(current);
        if (this->model.canAdd(currentNode->parent, kind))
        {
            targetParent = currentNode->parent;
            targetRow = this->model.indexOf(current) + 1;
            return true;
        }
        current = currentNode->parent;
    }
    return false;
}

bool RundownModel::resolveDrop(const QModelIndex& parent, int row, engine::NodeKind kind, engine::NodeId& targetParent, int& targetRow) const
{
    engine::NodeId parentId = nodeId(parent);
    const engine::Node* parentNode = this->model.find(parentId);

    if (row < 0 && parentNode != nullptr && parentNode->kind == engine::NodeKind::Item)
    {
        // Solto em cima de um Item: vai logo depois dele.
        row = this->model.indexOf(parentId) + 1;
        parentId = parentNode->parent;
    }

    while (!this->model.canAdd(parentId, kind))
    {
        if (parentId == engine::RootId)
            return false;
        const engine::Node* blocked = this->model.find(parentId);
        row = this->model.indexOf(parentId) + 1;
        parentId = blocked->parent;
    }

    targetParent = parentId;
    targetRow = row;
    return true;
}

std::optional<engine::NodeId> RundownModel::addGroup(engine::NodeKind kind, const QModelIndex& anchor, const QString& label)
{
    engine::NodeId target;
    int row;
    if (kind == engine::NodeKind::Item || !resolveAnchor(anchor, kind, target, row))
        return std::nullopt;

    const QModelIndex parentIndex = target == engine::RootId ? QModelIndex() : indexOf(target);
    const int position = row < 0 ? rowCount(parentIndex) : row;
    beginInsertRows(parentIndex, position, position);
    const std::optional<engine::NodeId> id = this->model.addGroup(kind, target, position, label.toStdString());
    endInsertRows();
    if (id)
        markModified();
    return id;
}

std::optional<engine::NodeId> RundownModel::addMedia(const QString& media, engine::LogicalResource resource, const QModelIndex& anchor)
{
    engine::NodeId target;
    int row;
    if (!resolveAnchor(anchor, engine::NodeKind::Item, target, row))
        return std::nullopt;

    engine::Item item;
    item.media = media.toStdString();
    item.resource = resource;

    const QModelIndex parentIndex = target == engine::RootId ? QModelIndex() : indexOf(target);
    const int position = row < 0 ? rowCount(parentIndex) : row;
    beginInsertRows(parentIndex, position, position);
    const std::optional<engine::NodeId> id = this->model.addItem(target, position, item, media.section('/', -1).toStdString());
    endInsertRows();
    if (id)
    {
        markModified();
        if (target != engine::RootId)
            emit dataChanged(indexOf(target, 0), indexOf(target, ColumnCount - 1));  // resumo do grupo
    }
    return id;
}

void RundownModel::removeNodes(const QList<engine::NodeId>& ids)
{
    bool changed = false;
    for (engine::NodeId id : ids)
    {
        const engine::Node* item = this->model.find(id);
        if (item == nullptr)
            continue;  // já removido junto com o pai
        const engine::NodeId parentId = item->parent;
        const QModelIndex parentIndex = parentId == engine::RootId ? QModelIndex() : indexOf(parentId);
        const int row = this->model.indexOf(id);
        beginRemoveRows(parentIndex, row, row);
        this->model.remove(id);
        endRemoveRows();
        changed = true;
        if (parentId != engine::RootId)
            emit dataChanged(indexOf(parentId, 0), indexOf(parentId, ColumnCount - 1));
    }
    if (changed)
        markModified();
}

int RundownModel::setResource(const QList<engine::NodeId>& ids, engine::LogicalResource resource)
{
    int count = 0;
    for (engine::NodeId id : ids)
    {
        for (engine::NodeId itemId : this->model.itemsOf(id))
        {
            const engine::Node* item = this->model.find(itemId);
            if (item->item.resource == resource)
                continue;
            engine::Item changed = item->item;
            changed.resource = resource;
            this->model.setItem(itemId, changed);
            emit dataChanged(indexOf(itemId, 0), indexOf(itemId, ColumnCount - 1));
            if (item->parent != engine::RootId)
                emit dataChanged(indexOf(item->parent, 0), indexOf(item->parent, ColumnCount - 1));
            ++count;
        }
    }
    if (count > 0)
        markModified();
    return count;
}

bool RundownModel::setRepeat(engine::NodeId id, engine::RepeatMode repeat)
{
    if (!this->model.setRepeat(id, repeat))
        return false;
    emit dataChanged(indexOf(id, 0), indexOf(id, ColumnCount - 1));
    markModified();
    return true;
}

bool RundownModel::moveNode(engine::NodeId id, engine::NodeId newParent, int row)
{
    const engine::Node* item = this->model.find(id);
    if (item == nullptr || !this->model.canMove(id, newParent))
        return false;

    const engine::NodeId oldParent = item->parent;
    const int sourceRow = this->model.indexOf(id);
    const QModelIndex sourceIndex = oldParent == engine::RootId ? QModelIndex() : indexOf(oldParent);
    const QModelIndex targetIndex = newParent == engine::RootId ? QModelIndex() : indexOf(newParent);
    const int destination = row < 0 ? rowCount(targetIndex) : row;  // numeração de antes do movimento

    if (oldParent == newParent && (destination == sourceRow || destination == sourceRow + 1))
        return true;  // já está no lugar

    if (!beginMoveRows(sourceIndex, sourceRow, sourceRow, targetIndex, destination))
        return false;
    const int finalRow = (oldParent == newParent && destination > sourceRow) ? destination - 1 : destination;
    this->model.move(id, newParent, finalRow);
    endMoveRows();

    if (oldParent != engine::RootId)
        emit dataChanged(indexOf(oldParent, 0), indexOf(oldParent, ColumnCount - 1));
    if (newParent != engine::RootId)
        emit dataChanged(indexOf(newParent, 0), indexOf(newParent, ColumnCount - 1));
    markModified();
    return true;
}

Qt::DropActions RundownModel::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}

Qt::DropActions RundownModel::supportedDragActions() const
{
    return Qt::MoveAction;
}

QStringList RundownModel::mimeTypes() const
{
    return { NodeMimeType, MediaMimeType };
}

QMimeData* RundownModel::mimeData(const QModelIndexList& indexes) const
{
    // Um id por linha; se o pai também foi selecionado, só o pai é movido.
    QSet<engine::NodeId> selected;
    for (const QModelIndex& index : indexes)
        selected.insert(nodeId(index));

    QStringList ids;
    for (engine::NodeId id : this->model.flatten())
    {
        if (!selected.contains(id))
            continue;
        bool ancestorSelected = false;
        for (engine::NodeId up = this->model.find(id)->parent; up != engine::RootId; up = this->model.find(up)->parent)
            if (selected.contains(up))
                ancestorSelected = true;
        if (!ancestorSelected)
            ids << QString::number(id);
    }

    QMimeData* data = new QMimeData();
    data->setData(NodeMimeType, ids.join(',').toUtf8());
    return data;
}

QList<engine::NodeId> RundownModel::decodeNodes(const QMimeData* data) const
{
    QList<engine::NodeId> ids;
    for (const QString& text : QString::fromUtf8(data->data(NodeMimeType)).split(',', Qt::SkipEmptyParts))
    {
        bool ok = false;
        const engine::NodeId id = text.toULongLong(&ok);
        if (ok && this->model.find(id) != nullptr)
            ids << id;
    }
    return ids;
}

QStringList RundownModel::decodeMedia(const QMimeData* data) const
{
    return QString::fromUtf8(data->data(MediaMimeType)).split('\n', Qt::SkipEmptyParts);
}

bool RundownModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(action);
    Q_UNUSED(column);

    engine::NodeId target;
    int targetRow;
    if (data->hasFormat(NodeMimeType))
    {
        const QList<engine::NodeId> ids = decodeNodes(data);
        if (ids.isEmpty())
            return false;
        for (engine::NodeId id : ids)
            if (!resolveDrop(parent, row, this->model.find(id)->kind, target, targetRow) || !this->model.canMove(id, target))
                return false;
        return true;
    }
    if (data->hasFormat(MediaMimeType))
        return !decodeMedia(data).isEmpty() && resolveDrop(parent, row, engine::NodeKind::Item, target, targetRow);
    return false;
}

bool RundownModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    if (!canDropMimeData(data, action, row, column, parent))
        return false;

    if (data->hasFormat(NodeMimeType))
    {
        engine::NodeId previous = engine::RootId;
        for (engine::NodeId id : decodeNodes(data))
        {
            engine::NodeId target;
            int targetRow;
            if (!resolveDrop(parent, row, this->model.find(id)->kind, target, targetRow))
                continue;
            // Os seguintes vão logo depois do anterior, preservando a ordem.
            if (previous != engine::RootId && this->model.find(previous)->parent == target)
                targetRow = this->model.indexOf(previous) + 1;
            if (moveNode(id, target, targetRow))
                previous = id;
        }
        return true;
    }

    engine::NodeId target;
    int targetRow;
    if (!resolveDrop(parent, row, engine::NodeKind::Item, target, targetRow))
        return false;
    const QModelIndex parentIndex = target == engine::RootId ? QModelIndex() : indexOf(target);
    int position = targetRow < 0 ? rowCount(parentIndex) : targetRow;
    for (const QString& media : decodeMedia(data))
    {
        engine::Item item;
        item.media = media.toStdString();
        beginInsertRows(parentIndex, position, position);
        this->model.addItem(target, position, item, media.section('/', -1).toStdString());
        endInsertRows();
        ++position;
    }
    if (target != engine::RootId)
        emit dataChanged(indexOf(target, 0), indexOf(target, ColumnCount - 1));
    markModified();
    return true;
}

bool RundownModel::removeRows(int row, int count, const QModelIndex& parent)
{
    Q_UNUSED(row);
    Q_UNUSED(count);
    Q_UNUSED(parent);
    return false;
}
