#include "RundownWidget.h"

#include "ExecutionController.h"
#include "RundownModel.h"

#include <QtCore/QItemSelectionModel>
#include <QtGui/QAction>
#include <QtGui/QKeySequence>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

RundownWidget::RundownWidget(RundownModel* model, ExecutionController* execution, QWidget* parent)
    : QWidget(parent), model(model), execution(execution)
{
    this->view = new QTreeView(this);
    this->view->setModel(model);
    this->view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->view->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->view->setDragEnabled(true);
    this->view->setAcceptDrops(true);
    this->view->setDropIndicatorShown(true);
    this->view->setDragDropMode(QAbstractItemView::DragDrop);
    this->view->setDefaultDropAction(Qt::MoveAction);
    this->view->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    this->view->setAllColumnsShowFocus(true);
    this->view->setUniformRowHeights(true);
    this->view->setContextMenuPolicy(Qt::ActionsContextMenu);
    this->view->header()->setSectionResizeMode(RundownModel::ColumnName, QHeaderView::Stretch);
    this->view->header()->setStretchLastSection(false);
    this->view->header()->resizeSection(RundownModel::ColumnResource, 180);
    this->view->header()->resizeSection(RundownModel::ColumnMedia, 200);
    this->view->header()->resizeSection(RundownModel::ColumnState, 140);

    auto makeAction = [this](const QString& text, const QKeySequence& shortcut, Qt::ShortcutContext context) {
        QAction* action = new QAction(text, this);
        action->setShortcut(shortcut);
        action->setShortcutContext(context);
        this->addAction(action);  // atalhos de edição só valem com o foco no Rundown
        return action;
    };

    // Edição (valem com o foco no Rundown, para não roubar teclas da busca da biblioteca).
    QAction* addBloco = makeAction(tr("Novo Bloco"), QKeySequence(Qt::CTRL | Qt::Key_1), Qt::WidgetWithChildrenShortcut);
    QAction* addSequencia = makeAction(tr("Nova Sequência"), QKeySequence(Qt::CTRL | Qt::Key_2), Qt::WidgetWithChildrenShortcut);
    QAction* addConjunto = makeAction(tr("Novo Conjunto"), QKeySequence(Qt::CTRL | Qt::Key_3), Qt::WidgetWithChildrenShortcut);
    QAction* toVt = makeAction(tr("Recurso: VT"), QKeySequence(Qt::ALT | Qt::Key_1), Qt::WidgetWithChildrenShortcut);
    QAction* toSelo1 = makeAction(tr("Recurso: Selo 1"), QKeySequence(Qt::ALT | Qt::Key_2), Qt::WidgetWithChildrenShortcut);
    QAction* toSelo2 = makeAction(tr("Recurso: Selo 2"), QKeySequence(Qt::ALT | Qt::Key_3), Qt::WidgetWithChildrenShortcut);
    this->repeatAction = makeAction(tr("Sequência em loop"), QKeySequence(Qt::CTRL | Qt::Key_L), Qt::WidgetWithChildrenShortcut);
    this->repeatAction->setCheckable(true);
    QAction* remove = makeAction(tr("Remover"), QKeySequence(QKeySequence::Delete), Qt::WidgetWithChildrenShortcut);

    // Playout sobre o selecionado (valem na janela toda).
    QAction* prepare = makeAction(tr("Preparar"), QKeySequence(Qt::Key_F7), Qt::WindowShortcut);
    QAction* play = makeAction(tr("Play"), QKeySequence(Qt::Key_F8), Qt::WindowShortcut);
    QAction* pause = makeAction(tr("Pause"), QKeySequence(Qt::Key_F10), Qt::WindowShortcut);
    QAction* stop = makeAction(tr("Stop"), QKeySequence(Qt::Key_F9), Qt::WindowShortcut);
    QAction* clear = makeAction(tr("Clear"), QKeySequence(Qt::Key_F11), Qt::WindowShortcut);
    // Tecla segurada não pode repetir: cada repetição seria um novo disparo no ar.
    for (QAction* action : { prepare, play, pause, stop, clear })
        action->setAutoRepeat(false);
    prepare->setToolTip(tr("Carrega em background (LOADBG) sem colocar no ar — F7"));
    play->setToolTip(tr("Coloca no ar o selecionado; se estiver pausado, retoma — F8"));
    pause->setToolTip(tr("Pausa o selecionado que está no ar — F10"));
    stop->setToolTip(tr("Tira do ar o selecionado — F9"));
    clear->setToolTip(tr("Limpa os layers do selecionado (no ar e preparado) — F11"));

    QAction* separatorA = new QAction(this);
    separatorA->setSeparator(true);
    QAction* separatorB = new QAction(this);
    separatorB->setSeparator(true);
    QAction* separatorC = new QAction(this);
    separatorC->setSeparator(true);

    this->editing = { addBloco, addSequencia, addConjunto, separatorA, toVt, toSelo1, toSelo2, separatorB, this->repeatAction, remove };
    this->playout = { prepare, play, pause, stop, clear };

    this->view->addActions(this->playout);
    this->view->addAction(separatorC);
    this->view->addActions(this->editing);

    QToolBar* toolbar = new QToolBar(this);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addActions(this->playout);
    toolbar->addSeparator();
    toolbar->addAction(addBloco);
    toolbar->addAction(addSequencia);
    toolbar->addAction(addConjunto);
    toolbar->addSeparator();
    toolbar->addAction(remove);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(toolbar);
    layout->addWidget(this->view, 1);

    QObject::connect(addBloco, &QAction::triggered, this, [this]() { addGroup(engine::NodeKind::Bloco); });
    QObject::connect(addSequencia, &QAction::triggered, this, [this]() { addGroup(engine::NodeKind::Sequencia); });
    QObject::connect(addConjunto, &QAction::triggered, this, [this]() { addGroup(engine::NodeKind::Conjunto); });
    QObject::connect(toVt, &QAction::triggered, this, [this]() { setResource(engine::LogicalResource::Vt); });
    QObject::connect(toSelo1, &QAction::triggered, this, [this]() { setResource(engine::LogicalResource::Selo1); });
    QObject::connect(toSelo2, &QAction::triggered, this, [this]() { setResource(engine::LogicalResource::Selo2); });
    QObject::connect(this->repeatAction, &QAction::triggered, this, [this](bool checked) {
        for (engine::NodeId id : selectedIds())
            this->model->setRepeat(id, checked ? engine::RepeatMode::Loop : engine::RepeatMode::Once);
    });
    QObject::connect(remove, &QAction::triggered, this, [this]() { this->model->removeNodes(selectedIds()); });

    QObject::connect(prepare, &QAction::triggered, this, [this]() {
        for (engine::NodeId id : selectedIds())
            this->execution->prepare(id);
    });
    QObject::connect(play, &QAction::triggered, this, [this]() {
        const engine::NodeId id = currentId();  // um só: Play em vários itens poria tudo no ar ao mesmo tempo
        if (id != engine::RootId)
            this->execution->playNode(id);
    });
    QObject::connect(pause, &QAction::triggered, this, [this]() {
        for (engine::NodeId id : selectedIds())
            this->execution->pauseNode(id);
    });
    QObject::connect(stop, &QAction::triggered, this, [this]() {
        for (engine::NodeId id : selectedIds())
            this->execution->stop(id);
    });
    QObject::connect(clear, &QAction::triggered, this, [this]() {
        for (engine::NodeId id : selectedIds())
            this->execution->clearNode(id);
    });

    QObject::connect(this->view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { updateActions(); });
    QObject::connect(model, &QAbstractItemModel::modelReset, this, [this]() {
        this->view->expandAll();
        updateActions();
    });
    QObject::connect(model, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex& parent) {
        this->view->expand(parent);
    });

    updateActions();
}

QList<engine::NodeId> RundownWidget::selectedIds() const
{
    QList<engine::NodeId> ids;
    for (engine::NodeId id : this->model->rundown().flatten())  // ordem do rundown, não da seleção
        if (this->view->selectionModel()->isRowSelected(this->model->indexOf(id).row(), this->model->indexOf(id).parent()))
            ids << id;
    return ids;
}

engine::NodeId RundownWidget::currentId() const
{
    const QModelIndex current = this->view->currentIndex();
    if (current.isValid() && this->view->selectionModel()->isSelected(current.sibling(current.row(), 0)))
        return this->model->nodeId(current);
    const QList<engine::NodeId> ids = selectedIds();
    return ids.isEmpty() ? engine::RootId : ids.first();
}

void RundownWidget::selectNode(engine::NodeId id)
{
    const QModelIndex index = this->model->indexOf(id);
    if (!index.isValid())
        return;
    this->view->expand(index.parent());
    this->view->setCurrentIndex(index);
    this->view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void RundownWidget::addGroup(engine::NodeKind kind)
{
    QString label;
    switch (kind)
    {
        case engine::NodeKind::Bloco: label = tr("Bloco"); break;
        case engine::NodeKind::Sequencia: label = tr("Sequência"); break;
        case engine::NodeKind::Conjunto: label = tr("Conjunto"); break;
        case engine::NodeKind::Item: return;
    }

    const std::optional<engine::NodeId> id = this->model->addGroup(kind, this->model->indexOf(currentId()), label);
    if (id)
    {
        selectNode(*id);
        this->view->edit(this->model->indexOf(*id));
    }
}

void RundownWidget::addMedia(const QStringList& media)
{
    QModelIndex anchor = this->model->indexOf(currentId());
    engine::NodeId last = engine::RootId;
    for (const QString& name : media)
    {
        const std::optional<engine::NodeId> id = this->model->addMedia(name, engine::LogicalResource::Vt, anchor);
        if (!id)
            continue;
        last = *id;
        anchor = this->model->indexOf(*id);  // os seguintes entram logo depois
    }
    if (last != engine::RootId)
        selectNode(last);
}

void RundownWidget::setResource(engine::LogicalResource resource)
{
    this->model->setResource(selectedIds(), resource);
}

void RundownWidget::updateActions()
{
    const QList<engine::NodeId> ids = selectedIds();
    bool anySequence = false;
    bool allLoop = true;
    for (engine::NodeId id : ids)
    {
        const engine::Node* node = this->model->rundown().find(id);
        if (node != nullptr && node->kind == engine::NodeKind::Sequencia)
        {
            anySequence = true;
            allLoop = allLoop && node->repeat == engine::RepeatMode::Loop;
        }
    }
    this->repeatAction->setEnabled(anySequence);
    this->repeatAction->setChecked(anySequence && allLoop);

    for (QAction* action : this->playout)
        action->setEnabled(!ids.isEmpty());
}
