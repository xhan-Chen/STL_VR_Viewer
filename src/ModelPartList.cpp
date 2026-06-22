/**
 * @file ModelPartList.cpp
 * @brief Implements the Qt model adapter for the ModelPart hierarchy.
 *
 * The model provides editable part names, visibility checkboxes, highlighting
 * roles, and insert/remove helpers for the main tree view.
 */

#include "ModelPartList.h"
#include "ModelPart.h"

#include <QColor>

ModelPartList::ModelPartList(const QString& data, QObject* parent) : QAbstractItemModel(parent) {
    /* Define column headers for the tree view via the root item */
    rootItem = new ModelPart({ tr("Part"), tr("Visible?") });
}

ModelPartList::~ModelPartList() {
    delete rootItem;
}

int ModelPartList::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return rootItem->columnCount();
}

QVariant ModelPartList::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return QVariant();

    ModelPart* item = static_cast<ModelPart*>(index.internalPointer());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (index.column() == 1) {
            return item->visible() ? tr("Visible") : tr("Hidden");
        }
        return item->data(index.column());
    }

    if (role == Qt::CheckStateRole && index.column() == 1) {
        return item->visible() ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::ToolTipRole) {
        return item->summary();
    }

    if (role == Qt::ForegroundRole && !item->visible()) {
        return QColor(130, 138, 145);
    }

    if (role == Qt::BackgroundRole) {
        if (item->selectedInView()) {
            return QColor(37, 99, 235, 95);
        }
        if (item->highlighted()) {
            return QColor(245, 158, 11, 95);
        }
        if (item->glowEnabled()) {
            return QColor(item->getGlowR(), item->getGlowG(), item->getGlowB(), 50);
        }
    }

    return QVariant();
}

bool ModelPartList::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid()) return false;

    ModelPart* item = static_cast<ModelPart*>(index.internalPointer());

    if (role == Qt::CheckStateRole && index.column() == 1) {
        item->setVisible(value.toInt() == Qt::Checked);
        emit dataChanged(index.siblingAtColumn(0), index.siblingAtColumn(1),
                         { Qt::DisplayRole, Qt::CheckStateRole, Qt::ForegroundRole });
        return true;
    }

    if (role == Qt::EditRole && index.column() == 0) {
        item->set(0, value.toString());
        emit dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });
        return true;
    }

    return false;
}

Qt::ItemFlags ModelPartList::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags result = QAbstractItemModel::flags(index);
    if (index.column() == 0) {
        result |= Qt::ItemIsEditable;
    }
    if (index.column() == 1) {
        result |= Qt::ItemIsUserCheckable;
    }
    return result;
}

QVariant ModelPartList::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return rootItem->data(section);

    return QVariant();
}

QModelIndex ModelPartList::index(int row, int column, const QModelIndex& parent) const {
    /* Validate indices within range before processing */
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    ModelPart* parentItem;
    if (!parent.isValid()) {
        parentItem = rootItem;
    }
    else {
        parentItem = static_cast<ModelPart*>(parent.internalPointer());
    }

    ModelPart* childItem = parentItem->child(row);
    if (childItem) {
        return createIndex(row, column, childItem);
    }

    return QModelIndex();
}

QModelIndex ModelPartList::parent(const QModelIndex& index) const {
    if (!index.isValid())
        return QModelIndex();

    ModelPart* childItem = static_cast<ModelPart*>(index.internalPointer());
    ModelPart* parentItem = childItem->parentItem();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int ModelPartList::rowCount(const QModelIndex& parent) const {
    ModelPart* parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<ModelPart*>(parent.internalPointer());

    return parentItem->childCount();
}

ModelPart* ModelPartList::getRootItem() {
    return rootItem;
}

QModelIndex ModelPartList::appendChild(const QModelIndex& parent, const QList<QVariant>& data) {
    /* Normalize parent index to column 0 */
    QModelIndex safeParent = parent;
    if (safeParent.isValid() && safeParent.column() > 0) {
        safeParent = safeParent.siblingAtColumn(0);
    }

    ModelPart* parentPart;
    if (safeParent.isValid()) {
        parentPart = static_cast<ModelPart*>(safeParent.internalPointer());
    }
    else {
        parentPart = rootItem;
    }

    int newRow = parentPart->childCount();

    /* Notify attached views that rows are about to be inserted */
    beginInsertRows(safeParent, newRow, newRow);

    ModelPart* childPart = new ModelPart(data, parentPart);
    parentPart->appendChild(childPart);

    QModelIndex child = createIndex(newRow, 0, childPart);

    endInsertRows();

    return child;
}

bool ModelPartList::removePart(const QModelIndex& index) {
    if (!index.isValid()) return false;

    /* Ensure indices are aligned to column 0 for internal logic */
    QModelIndex cleanIndex = index;
    if (cleanIndex.column() > 0) {
        cleanIndex = cleanIndex.siblingAtColumn(0);
    }

    ModelPart* item = static_cast<ModelPart*>(cleanIndex.internalPointer());
    ModelPart* parentItem = item->parentItem();

    if (!parentItem) return false;

    int row = cleanIndex.row();

    /* Notify views before actual deletion from the data structure */
    beginRemoveRows(cleanIndex.parent(), row, row);

    parentItem->removeChild(row);

    endRemoveRows();
    return true;
}
