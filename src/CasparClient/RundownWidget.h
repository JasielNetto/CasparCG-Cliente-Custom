#pragma once

// Rundown operacional (RF-03/RF-04): árvore de Blocos, Sequências, Conjuntos e itens.
// Seleção (clique) é separada de preparar (F7) e do transporte sobre o selecionado:
// Play (F8), Stop (F9), Pause (F10) e Clear (F11).

#include "Resource.h"
#include "Rundown.h"

#include <QtCore/QList>
#include <QtWidgets/QWidget>

class ExecutionController;
class QAction;
class QTreeView;
class RundownModel;

class RundownWidget : public QWidget
{
    Q_OBJECT

    public:
        RundownWidget(RundownModel* model, ExecutionController* execution, QWidget* parent = nullptr);

        // Adiciona mídias no ponto selecionado (duplo clique na biblioteca).
        void addMedia(const QStringList& media);

        QList<QAction*> editActions() const { return this->editing; }
        QList<QAction*> playoutActions() const { return this->playout; }

    private:
        RundownModel* model = nullptr;
        ExecutionController* execution = nullptr;
        QTreeView* view = nullptr;

        QList<QAction*> editing;
        QList<QAction*> playout;
        QAction* repeatAction = nullptr;

        QList<engine::NodeId> selectedIds() const;
        engine::NodeId currentId() const;
        void addGroup(engine::NodeKind kind);
        void setResource(engine::LogicalResource resource);
        void updateActions();
        void selectNode(engine::NodeId id);
};
