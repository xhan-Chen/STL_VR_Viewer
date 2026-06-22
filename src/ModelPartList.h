/**
 * @file ModelPartList.h
 * @brief Declares the Qt item model that exposes ModelPart objects to the tree view.
 *
 * ModelPartList adapts the custom ModelPart hierarchy to Qt's model/view API so
 * the application can show, edit, check, and remove STL parts in QTreeView.
 */
  
#ifndef VIEWER_MODELPARTLIST_H
#define VIEWER_MODELPARTLIST_H


#include "ModelPart.h"

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QString>
#include <QList>

class ModelPart;

/**
 * @class ModelPartList
 * @brief QAbstractItemModel implementation backed by a ModelPart tree.
 *
 * The model provides two columns: the part name and its visibility state. It also
 * exposes editing, check-box visibility toggles, hover/selection colours, and
 * helper methods for inserting and removing tree items.
 */
class ModelPartList : public QAbstractItemModel {
    Q_OBJECT
public:
    /**
     * @brief Constructs the model and creates the invisible root item.
     * @param data Unused legacy parameter retained for compatibility.
     * @param parent Optional QObject parent for Qt ownership.
     */
    ModelPartList( const QString& data, QObject* parent = NULL );

    /**
     * @brief Destroys the root item and therefore all child ModelPart objects.
     */
    ~ModelPartList();

    /**
     * @brief Reports how many columns the tree model exposes.
     * @param parent Parent index requested by Qt; unused because all rows share the same columns.
     * @return Number of columns.
     */
    int columnCount( const QModelIndex& parent ) const;

    /**
     * @brief Returns data requested by Qt views and delegates.
     * @param index Model index identifying the row and column.
     * @param role Qt item data role, such as display, edit, tooltip, or background.
     * @return Data for the requested role, or an invalid QVariant when unavailable.
     */
    QVariant data( const QModelIndex& index, int role ) const;

    /**
     * @brief Applies edited names or check-box visibility changes.
     * @param index Model index being edited.
     * @param value New value supplied by the view/delegate.
     * @param role Role being written, usually Qt::EditRole or Qt::CheckStateRole.
     * @return True when the edit was accepted.
     */
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    /**
     * @brief Reports item capabilities such as editing and checkability.
     * @param index Model index being queried.
     * @return Qt item flags for the index.
     */
    Qt::ItemFlags flags( const QModelIndex& index ) const;


    /**
     * @brief Returns header text for the tree columns.
     * @param section Header section index.
     * @param orientation Header orientation.
     * @param role Requested Qt role.
     * @return Header label for display role, otherwise an invalid QVariant.
     */
    QVariant headerData( int section, Qt::Orientation orientation, int role ) const;


    /**
     * @brief Creates a QModelIndex for a child item.
     * @param row Child row under parent.
     * @param column Column requested by Qt.
     * @param parent Parent model index; invalid means the root item.
     * @return Valid index for the child, or an invalid QModelIndex if out of range.
     */
    QModelIndex index( int row, int column, const QModelIndex& parent ) const;


    /**
     * @brief Gets the parent index of a model item.
     * @param index Child index whose parent is requested.
     * @return Parent index, or invalid QModelIndex for top-level items.
     */
    QModelIndex parent( const QModelIndex& index ) const;

    /**
     * @brief Counts rows below a parent item.
     * @param parent Parent model index; invalid means the root item.
     * @return Number of child rows.
     */
    int rowCount( const QModelIndex& parent ) const;

    /**
     * @brief Gets the invisible root ModelPart.
     * @return Root item pointer owned by this model.
     */
    ModelPart* getRootItem();

    /**
     * @brief Appends a new child item under a parent.
     * @param parent Parent index; invalid creates a top-level item.
     * @param data Column data for the new ModelPart.
     * @return Model index for the newly inserted child.
     */
    QModelIndex appendChild(const QModelIndex& parent, const QList<QVariant>& data);

    /**
     * @brief Removes and deletes a part from the model.
     * @param index Index of the part to remove; any column is normalised to column 0.
     * @return True when a valid item was removed.
     */
    bool removePart(const QModelIndex& index);

private:
    ModelPart *rootItem; ///< Invisible root node that owns the entire ModelPart tree.
};
#endif
