//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2020 MuseScore BVBA and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================
#include "noteinputbarcustomisemodel.h"

#include <optional>

#include "log.h"
#include "translation.h"

#include "internal/abstractnoteinputbaritem.h"
#include "internal/actionnoteinputbaritem.h"
#include "internal/notationactions.h"
#include "workspace/workspacetypes.h"

#include "uicomponents/view/itemmultiselectionmodel.h"

using namespace mu::notation;
using namespace mu::workspace;
using namespace mu::actions;
using namespace mu::framework;

static const std::string NOTE_INPUT_TOOLBAR_NAME("noteInput");

static const ActionName NOTE_INPUT_ACTION_NAME("note-input");
static const ActionName NOTE_INPUT_MODE_ACTION_NAME("mode-note-input");

static const std::string SEPARATOR_LINE_TITLE("-------  Separator line  -------");
static const ActionName SEPARATOR_LINE_ACTION_NAME("");

NoteInputBarCustomiseModel::NoteInputBarCustomiseModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_selectionModel = new ItemMultiSelectionModel(this);

    connect(m_selectionModel, &ItemMultiSelectionModel::selectionChanged,
            [this](const QItemSelection& selected, const QItemSelection& deselected) {
        updateOperationsAvailability();

        QModelIndexList indexes;
        indexes << selected.indexes() << deselected.indexes();
        QSet<QModelIndex> indexesSet(indexes.begin(), indexes.end());
        for (const QModelIndex& index: indexesSet) {
            emit dataChanged(index, index, { SelectedRole });
        }
    });
}

bool NoteInputBarCustomiseModel::removeRows(int row, int count, const QModelIndex& parent)
{
    beginRemoveRows(parent, row, row + count - 1);

    for (int i = row + count - 1; i >= row; --i) {
        m_actions.removeAt(i);
    }

    endRemoveRows();

    updateOperationsAvailability();
    saveActions();

    return true;
}

bool NoteInputBarCustomiseModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent,
                                          int destinationChild)
{
    int sourceFirstRow = sourceRow;
    int sourceLastRow = sourceRow + count - 1;
    int destinationRow = (sourceLastRow > destinationChild) ? destinationChild : destinationChild + 1;

    beginMoveRows(sourceParent, sourceFirstRow, sourceLastRow, destinationParent, destinationRow);

    int increaseCount = (sourceRow > destinationChild) ? 1 : 0;
    int moveIndex = 0;
    for (int i = 0; i < count; i++) {
        m_actions.move(sourceRow + moveIndex, destinationChild + moveIndex);
        moveIndex += increaseCount;
    }

    endMoveRows();

    updateOperationsAvailability();
    saveActions();

    return true;
}

static bool containsAction(const ActionNameList& toolbarActionNames, const ActionName& actionName)
{
    return std::find(toolbarActionNames.begin(), toolbarActionNames.end(), actionName) != toolbarActionNames.end();
}

static std::optional<size_t> indexOf(const ActionList& actions, const ActionName& actionName)
{
    for (size_t i = 0; i < actions.size(); ++i) {
        if (actions[i].name == actionName) {
            return i;
        }
    }

    return std::nullopt;
}

void NoteInputBarCustomiseModel::load()
{
    m_actions.clear();

    beginResetModel();

    ActionNameList actions = customizedActions();
    if (actions.empty()) {
        actions = defaultActions();
    }

    for (const ActionName& actionName: actions) {
        Action action = actionsRegister()->action(actionName);
        bool checked = containsAction(currentWorkspaceActions(), actionName);

        m_actions << makeItem(action, checked);
    }

    endResetModel();
}

void NoteInputBarCustomiseModel::addSeparatorLine()
{
    if (!m_selectionModel->hasSelection()) {
        return;
    }

    QModelIndex selectedItemIndex = m_selectionModel->selectedIndexes().first();
    if (!selectedItemIndex.isValid()) {
        return;
    }

    QModelIndex prevItemIndex = index(selectedItemIndex.row() - 1);
    if (!prevItemIndex.isValid()) {
        return;
    }

    beginInsertRows(prevItemIndex.parent(), prevItemIndex.row() + 1, prevItemIndex.row() + 1);
    m_actions.insert(prevItemIndex.row() + 1, makeSeparatorItem());
    endInsertRows();

    updateOperationsAvailability();
    saveActions();
}

QVariant NoteInputBarCustomiseModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    switch (role) {
    case ItemRole:
        return QVariant::fromValue(qobject_cast<QObject*>(m_actions[index.row()]));
    case SelectedRole:
        return m_selectionModel->isSelected(index);
    }

    return QVariant();
}

int NoteInputBarCustomiseModel::rowCount(const QModelIndex&) const
{
    return m_actions.count();
}

QHash<int, QByteArray> NoteInputBarCustomiseModel::roleNames() const
{
    static QHash<int, QByteArray> roles = {
        { ItemRole, "itemRole" },
        { SelectedRole, "selectedRole" }
    };

    return roles;
}

void NoteInputBarCustomiseModel::selectRow(int row)
{
    m_selectionModel->select(index(row));
}

void NoteInputBarCustomiseModel::moveSelectedRowsUp()
{
    QModelIndexList selectedIndexList = m_selectionModel->selectedIndexes();

    if (selectedIndexList.isEmpty()) {
        return;
    }

    std::sort(selectedIndexList.begin(), selectedIndexList.end(), [](QModelIndex f, QModelIndex s) -> bool {
        return f.row() < s.row();
    });

    QModelIndex sourceRowFirst = selectedIndexList.first();

    moveRows(sourceRowFirst.parent(), sourceRowFirst.row(), selectedIndexList.count(), sourceRowFirst.parent(), sourceRowFirst.row() - 1);

    updateOperationsAvailability();
}

void NoteInputBarCustomiseModel::moveSelectedRowsDown()
{
    QModelIndexList selectedIndexList = m_selectionModel->selectedIndexes();

    if (selectedIndexList.isEmpty()) {
        return;
    }

    std::sort(selectedIndexList.begin(), selectedIndexList.end(), [](QModelIndex f, QModelIndex s) -> bool {
        return f.row() < s.row();
    });

    QModelIndex sourceRowFirst = selectedIndexList.first();
    QModelIndex sourceRowLast = selectedIndexList.last();

    moveRows(sourceRowFirst.parent(), sourceRowFirst.row(), selectedIndexList.count(), sourceRowFirst.parent(), sourceRowLast.row() + 1);

    updateOperationsAvailability();
}

void NoteInputBarCustomiseModel::removeSelectedRows()
{
    if (!m_selectionModel || !m_selectionModel->hasSelection()) {
        return;
    }

    QModelIndexList selectedIndexList = m_selectionModel->selectedIndexes();

    QModelIndex parentIndex = selectedIndexList.first().parent();

    for (const QModelIndex& selectedIndex : selectedIndexList) {
        removeRows(selectedIndex.row(), 1, parentIndex);
    }
}

bool NoteInputBarCustomiseModel::isSelected(int row) const
{
    QModelIndex rowIndex = index(row);
    return m_selectionModel->isSelected(rowIndex);
}

QItemSelectionModel* NoteInputBarCustomiseModel::selectionModel() const
{
    return m_selectionModel;
}

bool NoteInputBarCustomiseModel::isMovingUpAvailable() const
{
    return m_isMovingUpAvailable;
}

bool NoteInputBarCustomiseModel::isMovingDownAvailable() const
{
    return m_isMovingDownAvailable;
}

bool NoteInputBarCustomiseModel::isRemovingAvailable() const
{
    return m_isRemovingAvailable;
}

bool NoteInputBarCustomiseModel::isAddSeparatorAvailable() const
{
    return m_isAddSeparatorAvailable;
}

void NoteInputBarCustomiseModel::setIsMovingUpAvailable(bool isMovingUpAvailable)
{
    if (m_isMovingUpAvailable == isMovingUpAvailable) {
        return;
    }

    m_isMovingUpAvailable = isMovingUpAvailable;
    emit isMovingUpAvailableChanged(m_isMovingUpAvailable);
}

void NoteInputBarCustomiseModel::setIsMovingDownAvailable(bool isMovingDownAvailable)
{
    if (m_isMovingDownAvailable == isMovingDownAvailable) {
        return;
    }

    m_isMovingDownAvailable = isMovingDownAvailable;
    emit isMovingDownAvailableChanged(m_isMovingDownAvailable);
}

void NoteInputBarCustomiseModel::setIsRemovingAvailable(bool isRemovingAvailable)
{
    if (m_isRemovingAvailable == isRemovingAvailable) {
        return;
    }

    m_isRemovingAvailable = isRemovingAvailable;
    emit isRemovingAvailableChanged(m_isRemovingAvailable);
}

void NoteInputBarCustomiseModel::setIsAddSeparatorAvailable(bool isAddSeparatorAvailable)
{
    if (m_isAddSeparatorAvailable == isAddSeparatorAvailable) {
        return;
    }

    m_isAddSeparatorAvailable = isAddSeparatorAvailable;
    emit isAddSeparatorAvailableChanged(m_isAddSeparatorAvailable);
}

AbstractNoteInputBarItem* NoteInputBarCustomiseModel::modelIndexToItem(const QModelIndex& index) const
{
    return m_actions[index.row()];
}

void NoteInputBarCustomiseModel::updateOperationsAvailability()
{
    updateRearrangementAvailability();
    updateRemovingAvailability();
    updateAddSeparatorAvailability();
}

void NoteInputBarCustomiseModel::updateRearrangementAvailability()
{
    QModelIndexList selectedIndexList = m_selectionModel->selectedIndexes();

    if (selectedIndexList.isEmpty()) {
        updateMovingUpAvailability();
        updateMovingDownAvailability();
        return;
    }

    QModelIndex selectedIndex = selectedIndexList.first();
    updateMovingUpAvailability(selectedIndex);
    updateMovingDownAvailability(selectedIndex);
}

void NoteInputBarCustomiseModel::updateMovingUpAvailability(const QModelIndex& selectedRowIndex)
{
    bool isRowInBoundaries = selectedRowIndex.isValid() ? selectedRowIndex.row() > 0 : false;
    setIsMovingUpAvailable(isRowInBoundaries);
}

void NoteInputBarCustomiseModel::updateMovingDownAvailability(const QModelIndex& selectedRowIndex)
{
    bool isRowInBoundaries = selectedRowIndex.isValid() ? selectedRowIndex.row() < rowCount() - 1 : false;
    setIsMovingDownAvailable(isRowInBoundaries);
}

void NoteInputBarCustomiseModel::updateRemovingAvailability()
{
    auto hasActionInSelection = [this](const QModelIndexList& selectedIndexes) {
        for (const QModelIndex& index : selectedIndexes) {
            AbstractNoteInputBarItem* item = modelIndexToItem(index);
            if (item && item->type() == AbstractNoteInputBarItem::ACTION) {
                return true;
            }
        }

        return false;
    };

    QModelIndexList selectedIndexes = m_selectionModel->selectedIndexes();
    bool removingAvailable = !selectedIndexes.empty();

    if (removingAvailable) {
        removingAvailable = !hasActionInSelection(selectedIndexes);
    }

    setIsRemovingAvailable(removingAvailable);
}

void NoteInputBarCustomiseModel::updateAddSeparatorAvailability()
{
    bool addingAvailable = !m_selectionModel->selectedIndexes().empty();
    if (!addingAvailable) {
        setIsAddSeparatorAvailable(addingAvailable);
        return;
    }

    addingAvailable = m_selectionModel->selectedIndexes().count() == 1;
    if (!addingAvailable) {
        setIsAddSeparatorAvailable(addingAvailable);
        return;
    }

    QModelIndex selectedItemIndex = m_selectionModel->selectedIndexes().first();

    AbstractNoteInputBarItem* selectedItem = modelIndexToItem(selectedItemIndex);
    addingAvailable = selectedItem && selectedItem->type() == AbstractNoteInputBarItem::ACTION;

    if (!addingAvailable) {
        setIsAddSeparatorAvailable(addingAvailable);
        return;
    }

    QModelIndex prevItemIndex = index(selectedItemIndex.row() - 1);
    addingAvailable = prevItemIndex.isValid();
    if (addingAvailable) {
        AbstractNoteInputBarItem* prevItem = modelIndexToItem(prevItemIndex);
        addingAvailable = prevItem && prevItem->type() == AbstractNoteInputBarItem::ACTION;
    }

    setIsAddSeparatorAvailable(addingAvailable);
}

AbstractNoteInputBarItem* NoteInputBarCustomiseModel::makeItem(const mu::actions::Action& action, bool checked)
{
    if (action.name.empty()) {
        return makeSeparatorItem();
    }

    ActionNoteInputBarItem* item = new ActionNoteInputBarItem(AbstractNoteInputBarItem::ItemType::ACTION);
    item->setId(QString::fromStdString(action.name));
    item->setTitle(qtrc("notation", action.title.c_str()));
    item->setIcon(action.iconCode);
    item->setChecked(checked);

    connect(item, &ActionNoteInputBarItem::checkedChanged, this, [this](bool) {
        saveActions();
    });

    return item;
}

AbstractNoteInputBarItem* NoteInputBarCustomiseModel::makeSeparatorItem() const
{
    AbstractNoteInputBarItem* item = new AbstractNoteInputBarItem(AbstractNoteInputBarItem::ItemType::SEPARATOR);
    item->setTitle(qtrc("notation", SEPARATOR_LINE_TITLE.c_str()));
    return item;
}

ActionNameList NoteInputBarCustomiseModel::customizedActions() const
{
    ActionNameList actionsFromConfiguration = configuration()->toolbarActions(NOTE_INPUT_TOOLBAR_NAME);

    if (actionsFromConfiguration.empty()) {
        return {};
    }

    ActionNameList result;
    for (const ActionName& actionName: actionsFromConfiguration) {
        if (containsAction(result, actionName) && actionName != SEPARATOR_LINE_ACTION_NAME) {
            continue;
        }

        result.push_back(actionName);
    }

    actions::ActionList allNoteInputActions = NotationActions::defaultNoteInputActions();
    for (const Action& action: allNoteInputActions) {
        if (actionFromNoteInputModes(action.name)) {
            continue;
        }

        if (containsAction(result, action.name)) {
            continue;
        }

        result.push_back(action.name);
    }

    return result;
}

ActionNameList NoteInputBarCustomiseModel::defaultActions() const
{
    ActionList allNoteInputActions = NotationActions::defaultNoteInputActions();
    ActionNameList currentWorkspaceNoteInputActions = currentWorkspaceActions();

    bool noteInputModeActionExists = false;

    ActionNameList result;
    auto canAppendAction = [&](const ActionName& actionName) {
        if (actionFromNoteInputModes(actionName)) {
            if (noteInputModeActionExists) {
                return false;
            }

            noteInputModeActionExists = true;
        }

        return true;
    };

    auto appendRelatedActions = [&](size_t startActionIndex) {
        ActionNameList actions;
        for (size_t i = startActionIndex; i < allNoteInputActions.size(); ++i) {
            ActionName actionName = allNoteInputActions[i].name;
            if (containsAction(currentWorkspaceNoteInputActions, actionName)) {
                break;
            }

            if (!canAppendAction(actionName)) {
                continue;
            }

            actions.push_back(actionName);
        }

        if (!actions.empty()) {
            result.insert(result.end(), actions.begin(), actions.end());
        }
    };

    //! NOTE: if there are actions at the beginning of the all note input actions,
    //!       but not at the beginning of the current workspace
    appendRelatedActions(0);

    for (const ActionName& actionName: currentWorkspaceNoteInputActions) {
        if (!canAppendAction(actionName)) {
            continue;
        }

        result.push_back(actionName);

        std::optional<size_t> indexInDefaultActions = indexOf(allNoteInputActions, actionName);
        if (indexInDefaultActions) {
            appendRelatedActions(indexInDefaultActions.value() + 1);
        }
    }

    if (!noteInputModeActionExists) {
        result.insert(result.begin(), NOTE_INPUT_ACTION_NAME);
    }

    return result;
}

ActionNameList NoteInputBarCustomiseModel::currentActions() const
{
    ActionNameList result;

    for (const AbstractNoteInputBarItem* item: m_actions) {
        if (item->type() == AbstractNoteInputBarItem::SEPARATOR) {
            result.push_back(SEPARATOR_LINE_ACTION_NAME);
            continue;
        }

        const ActionNoteInputBarItem* actionItem = dynamic_cast<const ActionNoteInputBarItem*>(item);
        if (actionItem && actionItem->checked()) {
            result.push_back(actionItem->id().toStdString());
        }
    }

    return result;
}

ActionNameList NoteInputBarCustomiseModel::currentWorkspaceActions() const
{
    RetValCh<IWorkspacePtr> workspace = workspaceManager()->currentWorkspace();
    if (!workspace.ret) {
        LOGE() << workspace.ret.toString();
        return {};
    }

    AbstractDataPtr abstractData = workspace.val->data(WorkspaceTag::Toolbar, NOTE_INPUT_TOOLBAR_NAME);
    ToolbarDataPtr toolbarData = std::dynamic_pointer_cast<ToolbarData>(abstractData);
    if (!toolbarData) {
        LOGE() << "Failed to get data of actions for " << NOTE_INPUT_TOOLBAR_NAME;
        return {};
    }

    return toolbarData->actions;
}

bool NoteInputBarCustomiseModel::actionFromNoteInputModes(const ActionName& actionName) const
{
    return QString::fromStdString(actionName).startsWith(QString::fromStdString(NOTE_INPUT_ACTION_NAME));
}

void NoteInputBarCustomiseModel::saveActions()
{
    RetValCh<IWorkspacePtr> workspace = workspaceManager()->currentWorkspace();
    if (!workspace.ret) {
        LOGW() << workspace.ret.toString();
        return;
    }

    ActionNameList currentActions = this->currentActions();

    ToolbarDataPtr toolbarData = std::make_shared<ToolbarData>();
    toolbarData->tag = WorkspaceTag::Toolbar;
    toolbarData->name = NOTE_INPUT_TOOLBAR_NAME;
    toolbarData->actions = currentActions;
    workspace.val->addData(toolbarData);

    ActionNameList items;
    for (const AbstractNoteInputBarItem* actionItem: m_actions) {
        items.push_back(actionItem->id().toStdString());
    }

    configuration()->setToolbarActions(NOTE_INPUT_TOOLBAR_NAME, items);
}
