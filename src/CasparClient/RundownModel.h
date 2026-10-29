#pragma once

// Modelo Qt sobre engine::Rundown. Toda alteração passa por aqui, para a árvore na tela ficar
// sempre sincronizada. As regras de composição são as do núcleo.

#include "Rundown.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QList>
#include <QtCore/QString>

class RundownModel : public QAbstractItemModel
{
    Q_OBJECT

    public:
        enum Column { ColumnName = 0, ColumnResource, ColumnMedia, ColumnState, ColumnCount };

        static const char* const NodeMimeType;   // arrastar dentro do rundown
        static const char* const MediaMimeType;  // arrastar da biblioteca de mídia

        explicit RundownModel(QObject* parent = nullptr);

        const engine::Rundown& rundown() const { return this->model; }
        void setRundown(const engine::Rundown& rundown);
        bool isModified() const { return this->modified; }
        void setModified(bool modified);

        engine::NodeId nodeId(const QModelIndex& index) const;
        QModelIndex indexOf(engine::NodeId id, int column = 0) const;
        const engine::Node* node(const QModelIndex& index) const;

        // Inserção relativa a 'anchor' (item selecionado): dentro dele se puder conter, senão logo depois.
        std::optional<engine::NodeId> addGroup(engine::NodeKind kind, const QModelIndex& anchor, const QString& label);
        std::optional<engine::NodeId> addMedia(const QString& media, engine::LogicalResource resource, const QModelIndex& anchor);

        void removeNodes(const QList<engine::NodeId>& ids);
        int setResource(const QList<engine::NodeId>& ids, engine::LogicalResource resource);  // retorna quantos itens mudaram
        bool setRepeat(engine::NodeId id, engine::RepeatMode repeat);
        void setState(engine::NodeId id, engine::NodeState state, const QString& error = QString());
        void resetStates();

        // QAbstractItemModel
        QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
        QModelIndex parent(const QModelIndex& child) const override;
        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
        Qt::ItemFlags flags(const QModelIndex& index) const override;
        bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

        Qt::DropActions supportedDropActions() const override;
        Qt::DropActions supportedDragActions() const override;
        QStringList mimeTypes() const override;
        QMimeData* mimeData(const QModelIndexList& indexes) const override;
        bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
        bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;

        // A view chama removeRows depois de um arrastar com MoveAction; o movimento já foi feito em
        // dropMimeData, então recusar aqui evita apagar os itens movidos.
        bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

        Q_SIGNAL void modifiedChanged(bool modified);

    private:
        engine::Rundown model;
        bool modified = false;

        void markModified();
        // Converte (pai, linha da view) em destino válido para 'kind'; retorna false se não houver.
        bool resolveDrop(const QModelIndex& parent, int row, engine::NodeKind kind, engine::NodeId& targetParent, int& targetRow) const;
        bool resolveAnchor(const QModelIndex& anchor, engine::NodeKind kind, engine::NodeId& targetParent, int& targetRow) const;
        bool moveNode(engine::NodeId id, engine::NodeId newParent, int row);
        QList<engine::NodeId> decodeNodes(const QMimeData* data) const;
        QStringList decodeMedia(const QMimeData* data) const;
};
