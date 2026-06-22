
/**
 * @file ModelPart.h
 * @brief Declares the tree item and VTK actor wrapper used for individual STL parts.
 *
 * ModelPart stores both the Qt tree data and the VTK objects needed to render one
 * STL component in the desktop and VR views. It also owns per-part appearance,
 * transform, grouping, and filter state.
 */

#ifndef VIEWER_MODELPART_H
#define VIEWER_MODELPART_H

#include <QString>
#include <QList>
#include <QVariant>
#include <array>

#include <vtkSmartPointer.h>
#include <vtkPolyDataMapper.h> 
#include <vtkActor.h>
#include <vtkSTLReader.h>
#include <vtkShrinkPolyData.h>
#include <vtkAlgorithmOutput.h>
#include <vtkClipPolyData.h>
#include <vtkPlane.h>
#include <vtkPolyData.h>

/**
 * @class ModelPart
 * @brief Represents one node in the model tree and, optionally, one STL mesh.
 *
 * A ModelPart can act as a purely organisational tree node or as a renderable STL
 * part. Renderable parts own a VTK reader, mapper, actor, optional VR actor, and
 * filter pipeline. Parent and child pointers form the hierarchy consumed by
 * ModelPartList.
 */
class ModelPart {
public:
    /**
     * @struct TransformState
     * @brief Captures a part transform for undo, redo, and VR synchronisation.
     */
    struct TransformState {
        std::array<double, 3> position = { 0.0, 0.0, 0.0 }; ///< XYZ translation in model units.
        std::array<double, 3> rotation = { 0.0, 0.0, 0.0 }; ///< XYZ Euler rotation in degrees.
        double scale = 1.0;                                ///< Uniform scale factor.
    };

    /**
     * @brief Creates a tree node with column data and an optional parent.
     * @param data Column values shown in the tree view.
     * @param parent Parent tree item, or nullptr for the root item.
     */
    ModelPart(const QList<QVariant>& data, ModelPart* parent = nullptr);

    /**
     * @brief Destroys this item and all child items.
     */
    ~ModelPart();

    /**
     * @brief Appends a child item to this item.
     * @param item Child item to take ownership of.
     */
    void appendChild(ModelPart* item);

    /**
     * @brief Returns the child item at a row.
     * @param row Child row index.
     * @return Pointer to the child, or nullptr if row is invalid.
     */
    ModelPart* child(int row);

    /**
     * @brief Counts child items.
     * @return Number of child rows.
     */
    int childCount() const;

    /**
     * @brief Counts tree columns stored by this item.
     * @return Number of columns.
     */
    int columnCount() const;

    /**
     * @brief Returns data for one tree column.
     * @param column Column index.
     * @return Column value, or an invalid QVariant if out of range.
     */
    QVariant data(int column) const;

    /**
     * @brief Replaces data for one tree column.
     * @param column Column index.
     * @param value New column value.
     */
    void set(int column, const QVariant& value);

    /**
     * @brief Gets this item's parent in the tree.
     * @return Parent item pointer, or nullptr for the root.
     */
    ModelPart* parentItem();

    /**
     * @brief Gets this item's row within its parent.
     * @return Row index, or 0 for the root item.
     */
    int row() const;

    /**
     * @brief Removes and deletes a child item.
     * @param row Child row to remove.
     */
    void removeChild(int row);

    /**
     * @brief Sets the base render colour.
     * @param R Red channel in the range 0-255.
     * @param G Green channel in the range 0-255.
     * @param B Blue channel in the range 0-255.
     */
    void setColour(const unsigned char R, const unsigned char G, const unsigned char B);

    /**
     * @brief Reads the base red channel.
     * @return Red channel in the range 0-255.
     */
    unsigned char getColourR();

    /**
     * @brief Reads the base green channel.
     * @return Green channel in the range 0-255.
     */
    unsigned char getColourG();

    /**
     * @brief Reads the base blue channel.
     * @return Blue channel in the range 0-255.
     */
    unsigned char getColourB();

    /**
     * @brief Sets visibility in the tree, desktop renderer, and VR actor.
     * @param isVisible True to show the part.
     */
    void setVisible(bool isVisible);

    /**
     * @brief Reads the visibility state.
     * @return True if the part is visible.
     */
    bool visible();

    /**
     * @brief Sets the uniform scale for the desktop and VR actors.
     * @param s Uniform scale factor.
     */
    void setScale(double s);

    /**
     * @brief Reads the uniform scale factor.
     * @return Current scale factor.
     */
    double getScale() const { return m_scale; }

    /**
     * @brief Sets the part translation.
     * @param x X position in model units.
     * @param y Y position in model units.
     * @param z Z position in model units.
     */
    void setPosition(double x, double y, double z);

    /**
     * @brief Reads the part translation.
     * @param x Output X position.
     * @param y Output Y position.
     * @param z Output Z position.
     */
    void getPosition(double& x, double& y, double& z) const;

    /**
     * @brief Sets the part rotation.
     * @param x Rotation about X in degrees.
     * @param y Rotation about Y in degrees.
     * @param z Rotation about Z in degrees.
     */
    void setRotation(double x, double y, double z);

    /**
     * @brief Reads the part rotation.
     * @param x Output X rotation in degrees.
     * @param y Output Y rotation in degrees.
     * @param z Output Z rotation in degrees.
     */
    void getRotation(double& x, double& y, double& z) const;

    /**
     * @brief Replaces position, rotation, and scale in one operation.
     * @param state Transform state to apply.
     */
    void setTransformState(const TransformState& state);

    /**
     * @brief Captures the current transform.
     * @return Current position, rotation, and scale.
     */
    TransformState transformState() const;

    /**
     * @brief Gets the stable unique identifier used by VR callbacks.
     * @return UUID string for this part.
     */
    QString id() const { return m_id; }

    /**
     * @brief Sets transient hover highlighting.
     * @param highlighted True when the mouse is hovering this part.
     */
    void setHighlighted(bool highlighted);

    /**
     * @brief Reads transient hover highlighting.
     * @return True if highlighted by hover.
     */
    bool highlighted() const { return m_highlighted; }

    /**
     * @brief Sets persistent selection highlighting from the 3D view.
     * @param selected True if selected in the 3D view.
     */
    void setSelectedInView(bool selected);

    /**
     * @brief Reads persistent selection highlighting.
     * @return True if selected in the 3D view.
     */
    bool selectedInView() const { return m_selectedInView; }

    /**
     * @brief Enables or disables glow styling.
     * @param enabled True to enable glow.
     */
    void setGlowEnabled(bool enabled);

    /**
     * @brief Reads the glow enable state.
     * @return True if glow styling is enabled.
     */
    bool glowEnabled() const { return m_glowEnabled; }

    /**
     * @brief Sets the glow colour.
     * @param R Red channel in the range 0-255.
     * @param G Green channel in the range 0-255.
     * @param B Blue channel in the range 0-255.
     */
    void setGlowColour(const unsigned char R, const unsigned char G, const unsigned char B);

    /**
     * @brief Reads the glow red channel.
     * @return Red channel in the range 0-255.
     */
    unsigned char getGlowR() const { return m_glowR; }

    /**
     * @brief Reads the glow green channel.
     * @return Green channel in the range 0-255.
     */
    unsigned char getGlowG() const { return m_glowG; }

    /**
     * @brief Reads the glow blue channel.
     * @return Blue channel in the range 0-255.
     */
    unsigned char getGlowB() const { return m_glowB; }

    /**
     * @brief Legacy helper for directly toggling shrink filter output.
     * @param active True to connect the shrink filter directly to the mapper.
     */
    void setShrinkFilterActive(bool active);

    /**
     * @brief Reads the legacy shrink-active flag.
     * @return True if the direct shrink filter path is active.
     */
    bool isShrinkActive() const { return shrinkActive; }

    /**
     * @brief Stores whether the shrink filter should be used by updatePipeline().
     * @param enabled True to enable shrink filtering.
     */
    void setShrinkEnabled(bool enabled);

    /**
     * @brief Stores whether the clip filter should be used by updatePipeline().
     * @param enabled True to enable clipping.
     */
    void setClipEnabled(bool enabled);

    /**
     * @brief Stores the configured filter execution order.
     * @param order 0 for shrink-then-clip, 1 for clip-then-shrink.
     */
    void setFilterOrder(int order);

    /**
     * @brief Reads the shrink filter setting.
     * @return True if shrink filtering is enabled.
     */
    bool getShrinkEnabled() const;

    /**
     * @brief Reads the clip filter setting.
     * @return True if clipping is enabled.
     */
    bool getClipEnabled() const;

    /**
     * @brief Reads the filter execution order.
     * @return 0 for shrink-then-clip, 1 for clip-then-shrink.
     */
    int getFilterOrder() const;

    /**
     * @brief Updates the VTK mapper pipeline from the current filter settings.
     */
    void updatePipeline();

    /**
     * @brief Loads geometry from an STL file and creates the desktop actor.
     * @param fileName Path to the STL file.
     */
    void loadSTL(QString fileName);

    /**
     * @brief Checks whether this item owns renderable geometry.
     * @return True when an STL file has been loaded.
     */
    bool hasGeometry() const { return !m_filePath.isEmpty() && actor != nullptr; }

    /**
     * @brief Gets the absolute STL file path.
     * @return File path, or an empty string for non-geometry tree nodes.
     */
    QString filePath() const { return m_filePath; }

    /**
     * @brief Gets the stored triangle count.
     * @return Number of STL polygons counted at load time.
     */
    qint64 triangleCount() const { return m_triangleCount; }

    /**
     * @brief Builds a short status string for tooltips and the status bar.
     * @return Human-readable part summary.
     */
    QString summary() const;

    /**
     * @brief Returns the desktop VTK actor associated with this part.
     * @return Smart pointer to the actor, or nullptr if no STL is loaded.
     */
    vtkSmartPointer<vtkActor> getActor() { return actor; }

    /**
     * @brief Creates a detached actor for the VR renderer.
     * @return Raw VTK actor pointer owned by this ModelPart until detachVrActor().
     */
    vtkActor* getNewActor();

    /**
     * @brief Releases this part's reference to the VR actor after transfer.
     */
    void detachVrActor();

    /**
     * @brief Deep-copies the current filtered geometry for safe cross-thread VR use.
     * @return New polydata snapshot of the current mapper input.
     */
    vtkSmartPointer<vtkPolyData> createGeometrySnapshot();

    /**
     * @brief Assigns this part to a named group.
     * @param name Group name, or an empty string to clear grouping.
     */
    void setGroupName(const QString& name);

    /**
     * @brief Gets the current group name.
     * @return Group name, or an empty string if ungrouped.
     */
    QString getGroupName() const;

    /**
     * @brief Checks whether the part is locked into a group.
     * @return True if the group name is not empty.
     */
    bool isLockedInGroup() const;

private:
    QList<ModelPart*>           m_childItems;     ///< Owned child items in the tree hierarchy.
    QList<QVariant>             m_itemData;       ///< Column data displayed by ModelPartList.
    ModelPart*                  m_parentItem;     ///< Parent item; nullptr only for the root item.

    bool                        isVisible;        ///< Visibility state shared by tree and actors.
    unsigned char               m_r = 255;        ///< Base red colour channel.
    unsigned char               m_g = 255;        ///< Base green colour channel.
    unsigned char               m_b = 255;        ///< Base blue colour channel.
    double                      m_scale = 1.0;    ///< Uniform render scale.
    double                      m_position[3] = { 0.0, 0.0, 0.0 }; ///< XYZ translation.
    double                      m_rotation[3] = { 0.0, 0.0, 0.0 }; ///< XYZ Euler rotation in degrees.
    QString                     m_filePath;       ///< Absolute path to the loaded STL file.
    QString                     m_id;             ///< Stable UUID used to connect desktop and VR updates.
    qint64                      m_triangleCount = 0; ///< Number of polygons in the STL file.
    double                      m_bounds[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }; ///< STL bounds in VTK order.
    bool                        m_highlighted = false; ///< True while the mouse is hovering this part.
    bool                        m_selectedInView = false; ///< True when selected from the 3D viewport.
    bool                        m_glowEnabled = false; ///< True when glow material settings are active.
    unsigned char               m_glowR = 0;      ///< Glow red colour channel.
    unsigned char               m_glowG = 180;    ///< Glow green colour channel.
    unsigned char               m_glowB = 255;    ///< Glow blue colour channel.

    vtkSmartPointer<vtkSTLReader>       file;     ///< STL reader that owns the source mesh pipeline.
    vtkSmartPointer<vtkPolyDataMapper>  mapper;   ///< Desktop mapper connected to the active filter pipeline.
    vtkSmartPointer<vtkActor>           actor;    ///< Desktop VTK actor displayed in the main renderer.
    vtkSmartPointer<vtkActor>           vrActor;  ///< Temporary VR actor before ownership is transferred.

    vtkSmartPointer<vtkShrinkPolyData>  shrinkFilter; ///< Optional shrink filter used for exploded-style previews.
    vtkSmartPointer<vtkClipPolyData>    clipFilter;   ///< Optional clipping filter used to inspect part interiors.
    vtkSmartPointer<vtkPlane>           clipPlane;    ///< Clip plane positioned through the part bounds.
    vtkSmartPointer<vtkAlgorithmOutput> originalDataPort; ///< Original STL reader output port.

    bool shrinkActive = false;          ///< Legacy direct shrink filter state.
    bool shrinkEnabled = false;         ///< True when updatePipeline() should include shrinkFilter.
    bool clipEnabled = false;           ///< True when updatePipeline() should include clipFilter.
    int  filterOrder = 0;               ///< Filter order: 0 shrink then clip, 1 clip then shrink.

    QString m_groupName = "";          ///< Name of the locked movement group, or empty when ungrouped.

    /**
     * @brief Applies stored transform values to desktop and VR actors.
     */
    void applyTransformToActors();

    /**
     * @brief Rebuilds VTK actor material properties from colour, glow, and selection state.
     */
    void refreshAppearance();
};


#endif
