/**
 * @file mainwindow.h
 * @brief Declares the main Qt window for the STL assembly and VR viewer.
 *
 * MainWindow coordinates the user interface, Qt item model, VTK desktop renderer,
 * HDR environment controls, undo/redo stack, grouping tools, and VR render thread.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <vtkAssembly.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkHDRReader.h>
#include <vtkImageData.h>
#include <vtkLight.h>
#include <vtkLightCollection.h>
#include <vtkRenderer.h>
#include <vtkSkybox.h>
#include <vtkSmartPointer.h>
#include <vtkTexture.h>

#include <QElapsedTimer>
#include <QHash>
#include <QMainWindow>
#include <QMap>
#include <QVector>

#include "ModelPart.h"
#include "ModelPartList.h"

class VRRenderThread;
class QLabel;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QSplitter;
class QTimer;
class QUndoStack;
class QAction;
class vtkActor;
class vtkCallbackCommand;
class vtkOrientationMarkerWidget;
class vtkPropPicker;

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

/**
 * @class MainWindow
 * @brief Primary application window for loading, editing, rendering, and viewing STL assemblies in VR.
 *
 * The class owns the desktop VTK scene and the Qt controls around it. It loads
 * STL files into a ModelPartList tree, applies per-part materials and filters,
 * manages grouping and transforms, and starts/stops VRRenderThread for the
 * headset view.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Creates the application window, model, renderer, and custom controls.
     * @param parent Optional parent widget.
     */
    MainWindow(QWidget* parent = nullptr);

    /**
     * @brief Stops VR if needed and releases UI/model resources.
     */
    ~MainWindow();

public slots:
    /**
     * @brief Adds a new child item under the selected tree item.
     */
    void handleButton();

    /**
     * @brief Handles tree selection changes and synchronises the 3D selection state.
     */
    void handleTreeClicked();

    /**
     * @brief Opens one STL file and loads it into the selected or new tree item.
     */
    void on_actionOpen_File_triggered();

    /**
     * @brief Opens a folder and loads all discovered STL files into the tree.
     */
    void on_actionOpen_Folder_triggered();

    /**
     * @brief Opens the Part Options dialog for the selected part.
     */
    void on_actionItemOptions_triggered();

    /**
     * @brief Adds an empty top-level tree item.
     */
    void on_actionAdd_Level_triggered();

    /**
     * @brief Adds an empty child item under the current selection.
     */
    void on_actionAdd_Item_triggered();

    /**
     * @brief Deletes the selected tree item after confirmation.
     */
    void on_actionDelete_Item_triggered();

signals:
    /**
     * @brief Requests a status-bar message.
     * @param message Text to display.
     * @param timeout Duration in milliseconds; zero keeps the message until replaced.
     */
    void statusUpdateMessage(const QString& message, int timeout);

private slots:
    /**
     * @brief Rebuilds the desktop scene without resetting the camera.
     */
    void updateRender();

    /**
     * @brief Recursively adds visible actors from the tree into the VTK scene.
     * @param index Current tree index being visited.
     * @param assemblies Map of group names to VTK assemblies under construction.
     */
    void updateRenderFromTree(const QModelIndex& index, QMap<QString, vtkSmartPointer<vtkAssembly>>& assemblies);

    /**
     * @brief Toggles whether hidden parts are removed from the renderer.
     */
    void on_actionToggle_Filter_triggered();

    /**
     * @brief Updates the scene light intensity from the UI slider.
     * @param value Slider value.
     */
    void on_lightSlider_valueChanged(int value);

    /**
     * @brief Updates the desktop camera zoom from the UI slider.
     * @param value Slider value interpreted as percentage.
     */
    void on_zoomSlider_valueChanged(int value);

    /**
     * @brief Updates selected part or group scale from the UI slider.
     * @param value Slider value interpreted as percentage.
     */
    void on_itemScaleSlider_valueChanged(int value);

    /**
     * @brief Filters the tree view by search text.
     * @param text Search string typed by the user.
     */
    void handleSearchTextChanged(const QString& text);

    /**
     * @brief Applies transform spin-box values to selected parts.
     */
    void applyTransformFromControls();

    /**
     * @brief Resets selected part transforms to identity.
     */
    void resetSelectedTransform();

    /**
     * @brief Applies the currently selected HDR environment.
     * @param index Combo-box index of the selected environment.
     */
    void applyEnvironmentSelection(int index);

    /**
     * @brief Starts the VR render thread and transfers actor snapshots.
     */
    void on_actionStart_VR_triggered();

    /**
     * @brief Stops the VR render thread.
     */
    void on_actionStop_VR_triggered();

    /**
     * @brief Pauses or resumes VR animation.
     */
    void on_actionPause_VR_triggered();

    /**
     * @brief Locks selected parts into a new movement group.
     */
    void on_actionLock_to_New_Group_triggered();

    /**
     * @brief Removes selected parts from their movement group.
     */
    void on_actionUnlock_Item_triggered();

    /**
     * @brief Removes group tree entries that no longer contain any parts.
     */
    void cleanupEmptyGroups();

private:
    Ui::MainWindow* ui;                    ///< Generated widget tree from mainwindow.ui.
    ModelPartList* partList;               ///< Qt model containing all loaded ModelPart items.
    QLineEdit* searchEdit = nullptr;       ///< Search box used to filter the tree view.
    QComboBox* hdrComboBox = nullptr;      ///< Environment selector for built-in and imported HDR files.
    QPushButton* importHdrButton = nullptr; ///< Button that imports a custom HDR file.
    QComboBox* vrViewModeCombo = nullptr;  ///< Standing/sitting VR mode selector.
    QComboBox* vrViewAngleCombo = nullptr; ///< Preset camera yaw selector for VR reset.
    QDoubleSpinBox* hdrTiltSpin = nullptr; ///< HDR X tilt control in degrees.
    QDoubleSpinBox* hdrTiltYSpin = nullptr; ///< HDR Y tilt control in degrees.
    QDoubleSpinBox* hdrHeadingSpin = nullptr; ///< HDR heading control in degrees.
    QDoubleSpinBox* vrModelSizeSpin = nullptr; ///< Target model size in VR metres.
    QDoubleSpinBox* vrDistanceSpin = nullptr; ///< VR reset-view standoff distance.
    QDoubleSpinBox* vrHeightOffsetSpin = nullptr; ///< VR headset height fine-tuning offset.
    QLabel* sceneStatsLabel = nullptr;     ///< Status bar label showing scene totals.
    QLabel* selectionDetailsLabel = nullptr; ///< Status bar label showing selected part details.
    QProgressBar* loadProgressBar = nullptr; ///< Progress indicator for bulk STL loading.
    QTimer* vrTransformCommitTimer = nullptr; ///< Debounces VR move updates into one undo command.
    QSlider* treeScaleSlider = nullptr;    ///< UI control that scales the tree view font and row size.
    QDoubleSpinBox* positionSpin[3] = { nullptr, nullptr, nullptr }; ///< XYZ position controls.
    QDoubleSpinBox* rotationSpin[3] = { nullptr, nullptr, nullptr }; ///< XYZ rotation controls.
    QDoubleSpinBox* transformScaleSpin = nullptr; ///< Uniform transform scale control.
    QPushButton* applyTransformButton = nullptr; ///< Button that applies transform spin-box values.
    QPushButton* resetTransformButton = nullptr; ///< Button that resets selected transform values.
    QPushButton* resetVrViewButton = nullptr; ///< Button that resets the headset view pose.
    QSplitter* mainSplitter = nullptr;      ///< Main left/view/right splitter that makes panels resizable.
    QUndoStack* undoStack = nullptr;        ///< Undo/redo stack for transforms and VR moves.
    QAction* gridAction = nullptr;          ///< Toolbar action that toggles the floor grid.
    QAction* performanceAction = nullptr;   ///< Toolbar action that toggles high-performance rendering mode.
    QAction* showcaseSpinAction = nullptr;  ///< Toolbar action that toggles automatic showcase rotation.

    bool visibilityFilterOn = false;       ///< True when hidden parts are excluded from the renderer.
    bool bulkLoading = false;              ///< True while loading many STL files to suppress expensive updates.
    int hoverPickIntervalMs = 55;          ///< Minimum delay between hover-picking operations.
    QElapsedTimer hoverPickTimer;          ///< Timer used to throttle desktop hover picking.
    QElapsedTimer vrTransformRenderTimer;  ///< Timer used to throttle desktop redraws from VR movement.
    QHash<QString, ModelPart::TransformState> pendingVrTransformStartStates; ///< Original transforms for pending VR undo commands.
    QHash<QString, ModelPart::TransformState> pendingVrTransformLatestStates; ///< Latest transforms received from VR before commit.

    vtkSmartPointer<vtkRenderer>                 renderer; ///< Desktop VTK renderer.
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow; ///< Qt-compatible OpenGL render window.
    vtkSmartPointer<vtkSkybox>                   mySkybox; ///< Desktop skybox actor for HDR environments.
    vtkSmartPointer<vtkOrientationMarkerWidget>  orientationMarker; ///< Small orientation axes widget.
    vtkSmartPointer<vtkActor>                    gridActor; ///< Floor grid actor.
    vtkSmartPointer<vtkActor>                    environmentActor; ///< Legacy environment actor placeholder.
    vtkSmartPointer<vtkTexture>                  environmentTexture; ///< Desktop HDR environment texture.
    vtkSmartPointer<vtkImageData>                environmentBaseImage; ///< Tonemapped HDR image before orientation.
    vtkSmartPointer<vtkImageData>                environmentImage; ///< Oriented HDR image currently displayed.
    vtkSmartPointer<vtkPropPicker>               propPicker; ///< Picker used for hover and click selection.
    vtkSmartPointer<vtkCallbackCommand>          mouseMoveCallback; ///< VTK callback for hover events.
    vtkSmartPointer<vtkCallbackCommand>          leftClickCallback; ///< VTK callback for click selection.

    QMap<QString, vtkSmartPointer<vtkAssembly>> groupAssemblies; ///< Group-name to VTK assembly map.
    QHash<vtkActor*, ModelPart*> actorPartMap; ///< Desktop actor to ModelPart lookup table.
    ModelPart* hoveredPart = nullptr;          ///< Part currently highlighted by hover.
    ModelPart* selectedViewPart = nullptr;     ///< Part currently selected in the 3D view.

    /**
     * @brief Finds all parts assigned to a group.
     * @param parentIndex Tree index from which to start recursion.
     * @param groupName Group name to match.
     * @return List of matching parts.
     */
    QList<ModelPart*> getGroupMembers(const QModelIndex& parentIndex, const QString& groupName);

    /**
     * @brief Converts any tree index to its column-0 equivalent.
     * @param index Original index.
     * @return Column-0 index for the same row, or invalid if input is invalid.
     */
    QModelIndex normalizedTreeIndex(const QModelIndex& index) const;

    /**
     * @brief Builds the professional dark UI around the generated widgets.
     */
    void setupProfessionalUi();

    /**
     * @brief Creates the desktop VTK renderer, render window, pickers, and callbacks.
     */
    void setupRenderingPipeline();

    /**
     * @brief Configures toolbar/menu text, icons, and action states.
     */
    void configureActions();

    /**
     * @brief Rebuilds all desktop scene props from the current model state.
     * @param resetCamera True to refit the camera after rebuilding.
     */
    void rebuildScene(bool resetCamera);

    /**
     * @brief Fits the desktop camera to the loaded scene.
     */
    void fitCameraToScene();

    /**
     * @brief Adds grid, skybox, and orientation props that are independent of model parts.
     */
    void addPersistentSceneProps();

    /**
     * @brief Creates or recreates the floor grid actor.
     */
    void createGridActor();

    /**
     * @brief Populates the HDR combo box with built-in and remembered environments.
     */
    void populateHdrOptions();

    /**
     * @brief Imports a user-selected HDR file into the environment selector.
     */
    void importHdrFile();

    /**
     * @brief Applies a scaling value to the tree view UI.
     * @param value Slider value interpreted as percentage.
     */
    void applyTreeScale(int value);

    /**
     * @brief Applies rendering settings for high-performance or higher-quality mode.
     * @param enabled True to prefer performance.
     */
    void applyPerformanceMode(bool enabled);

    /**
     * @brief Sends current VR tuning values to an active VR thread.
     */
    void applyVrTuningToThread();

    /**
     * @brief Applies HDR orientation controls to desktop and VR environments.
     */
    void applyEnvironmentOrientation();

    /**
     * @brief Recursively filters tree rows by text.
     * @param parentIndex Parent index to search below.
     * @param filterText Lowercase text to match.
     * @return True when parent or any child should remain visible.
     */
    bool applyTreeFilter(const QModelIndex& parentIndex, const QString& filterText);

    /**
     * @brief Recalculates scene statistics and updates the status label.
     */
    void updateSceneSummary();

    /**
     * @brief Recursively counts loaded parts, visible parts, and triangles.
     * @param parentIndex Parent index to count below.
     * @param loadedParts Output count of parts with geometry.
     * @param visibleParts Output count of visible geometry parts.
     * @param triangles Output total triangle count.
     */
    void collectSceneStats(const QModelIndex& parentIndex, int& loadedParts, int& visibleParts, qint64& triangles) const;

    /**
     * @brief Recursively calculates combined model bounds.
     * @param parentIndex Parent index to measure below.
     * @param bounds Output VTK bounds array.
     * @return True if at least one geometry part contributed bounds.
     */
    bool collectModelBounds(const QModelIndex& parentIndex, double bounds[6]) const;

    /**
     * @brief Translates a set of model parts so their combined centre sits at the origin.
     * @param parentIndex Parent index defining the subtree to centre.
     */
    void centerModelAtOrigin(const QModelIndex& parentIndex);

    /**
     * @brief Recursively translates model parts by a delta.
     * @param parentIndex Parent index defining the subtree to translate.
     * @param dx X delta in model units.
     * @param dy Y delta in model units.
     * @param dz Z delta in model units.
     */
    void translateModelParts(const QModelIndex& parentIndex, double dx, double dy, double dz);

    /**
     * @brief Safely stops and deletes the VR thread if it exists.
     */
    void stopVRThread();

    /**
     * @brief Gets the current transform targets, expanding groups when needed.
     * @return List of parts affected by transform controls.
     */
    QList<ModelPart*> selectedTransformTargets() const;

    /**
     * @brief Copies a part transform into the transform spin boxes.
     * @param part Part whose transform should be displayed, or nullptr to clear state.
     */
    void syncTransformControls(ModelPart* part);

    /**
     * @brief Applies a list of transform states to matching parts.
     * @param parts Parts to update.
     * @param states Transform states aligned with parts.
     */
    void applyTransformStates(const QList<ModelPart*>& parts, const QVector<ModelPart::TransformState>& states);

    /**
     * @brief Updates hover highlighting in the tree and 3D view.
     * @param part Part under the cursor, or nullptr to clear hover.
     */
    void setHoveredPart(ModelPart* part);

    /**
     * @brief Selects a part from the desktop 3D view and synchronises the tree.
     * @param part Selected part, or nullptr to clear selection.
     */
    void selectViewPart(ModelPart* part);

    /**
     * @brief Finds the tree index for a ModelPart pointer.
     * @param part Part to locate.
     * @return Model index for the part, or invalid if not found.
     */
    QModelIndex findIndexForPart(ModelPart* part) const;

    /**
     * @brief Recursive implementation of findIndexForPart().
     * @param parentIndex Parent index to search below.
     * @param part Part to locate.
     * @return Model index for the part, or invalid if not found.
     */
    QModelIndex findIndexForPartRecursive(const QModelIndex& parentIndex, ModelPart* part) const;

    /**
     * @brief Finds a part by its stable id.
     * @param id UUID string assigned by ModelPart.
     * @return Part pointer, or nullptr if not found.
     */
    ModelPart* findPartById(const QString& id) const;

    /**
     * @brief Recursive implementation of findPartById().
     * @param parentIndex Parent index to search below.
     * @param id UUID string assigned by ModelPart.
     * @return Part pointer, or nullptr if not found.
     */
    ModelPart* findPartByIdRecursive(const QModelIndex& parentIndex, const QString& id) const;

    /**
     * @brief Handles a VTK mouse-move callback and updates hover highlighting.
     */
    void handleVtkHoverEvent();

    /**
     * @brief Handles a VTK left-click callback and selects the picked part.
     */
    void handleVtkClickEvent();

    /**
     * @brief Applies a transform update received from VR.
     * @param partId Stable ModelPart identifier.
     * @param px New X position.
     * @param py New Y position.
     * @param pz New Z position.
     * @param rx New X rotation in degrees.
     * @param ry New Y rotation in degrees.
     * @param rz New Z rotation in degrees.
     * @param scale New uniform scale.
     */
    void applyVrTransformUpdate(const QString& partId, double px, double py, double pz,
                                double rx, double ry, double rz, double scale);

    /**
     * @brief Applies a colour update received from a VR controller action.
     * @param partId Stable ModelPart identifier.
     * @param r Red channel.
     * @param g Green channel.
     * @param b Blue channel.
     */
    void applyVrColorUpdate(const QString& partId, int r, int g, int b);

    /**
     * @brief Converts debounced VR movement into undo-stack commands.
     */
    void commitPendingVrTransforms();

    /**
     * @brief Resets the active VR view using current UI tuning controls.
     */
    void resetVrView();

    VRRenderThread* vrThread = nullptr; ///< Active VR worker thread, or nullptr when VR is stopped.

    /**
     * @brief Recursively collects detached actor snapshots for the VR renderer.
     * @param index Tree index at which to start recursion.
     */
    void collectActorsForVR(const QModelIndex& index);
};

#endif // MAINWINDOW_H
