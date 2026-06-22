/**
 * @file VRRenderThread.h
 * @brief Declares the OpenVR rendering thread and VR-to-desktop synchronisation API.
 *
 * VRRenderThread owns the VTK OpenVR render window, renderer, controller actions,
 * environment map, and detached actor copies used in the headset. The class keeps
 * VTK mutations inside the VR thread and communicates back to MainWindow through
 * Qt signals.
 */

#ifndef VR_RENDER_THREAD_H
#define VR_RENDER_THREAD_H

#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkOpenVRCamera.h>
#include <vtkOpenVRRenderer.h>
#include <vtkOpenVRRenderWindow.h>
#include <vtkOpenVRRenderWindowInteractor.h>
#include <vtkPolyData.h>
#include <vtkSkybox.h>
#include <vtkSmartPointer.h>
#include <vtkTexture.h>

#include <QMutex>
#include <QThread>
#include <QVector>
#include <QWaitCondition>
#include <QString>

#include <chrono>

class vtkEventData;

/**
 * @class VRRenderThread
 * @brief Runs the VTK/OpenVR scene on a worker thread.
 *
 * The desktop renderer and the VR renderer cannot safely share live VTK pipeline
 * objects while filters are being changed. This class therefore receives actor and
 * geometry snapshots before or during VR rendering, applies queued changes inside
 * run(), and emits desktop-space transform/colour updates back to MainWindow.
 */
class VRRenderThread : public QThread
{
    Q_OBJECT

public:
    /**
     * @enum Command
     * @brief Thread-safe commands accepted by issueCommand().
     */
    enum Command {
        PAUSE_RENDER,      ///< Pause or resume continuous VR animation.
        END_RENDER,        ///< Request the VR event loop to stop.
        ROTATE_X,          ///< Set continuous X rotation speed.
        ROTATE_Y,          ///< Set continuous Y rotation speed.
        ROTATE_Z,          ///< Set continuous Z rotation speed.
        RESET_VIEW,        ///< Reset the user view to the configured inspection pose.
        SET_VIEW_MODE,     ///< Switch between standing and sitting view mode.
        SET_SHOWCASE_SPIN  ///< Enable or disable automatic showcase rotation.
    };

    /**
     * @enum ViewMode
     * @brief Starting height preset for the VR camera.
     */
    enum ViewMode {
        STANDING_VIEW = 0, ///< Standing headset height preset.
        SITTING_VIEW = 1   ///< Sitting headset height preset.
    };

    /**
     * @brief Constructs the VR rendering thread.
     * @param parent Optional QObject parent for Qt ownership.
     */
    VRRenderThread(QObject* parent = nullptr);

    /**
     * @brief Requests shutdown and waits briefly for VR resources to release.
     */
    ~VRRenderThread();

    /**
     * @brief Adds an untracked actor before the VR thread starts.
     * @param actor Actor to show in VR.
     */
    void addActorOffline(vtkActor* actor);

    /**
     * @brief Adds a tracked actor before the VR thread starts.
     * @param partId Stable ModelPart identifier used for callbacks.
     * @param actor Detached actor copy to show in VR.
     */
    void addActorOffline(const QString& partId, vtkActor* actor);

    /**
     * @brief Sets desktop-space model bounds used to scale and place the VR scene.
     * @param bounds Bounds array in VTK order: xmin, xmax, ymin, ymax, zmin, zmax.
     */
    void setSceneBounds(const double bounds[6]);

    /**
     * @brief Sets the headset height preset.
     * @param mode Standing or sitting view mode.
     */
    void setViewMode(ViewMode mode);

    /**
     * @brief Sets the yaw angle used when resetting the VR view.
     * @param yawDegrees View yaw in degrees around the car.
     */
    void setViewYawDegrees(double yawDegrees);

    /**
     * @brief Sets user-adjustable VR inspection scale and offset values.
     * @param modelSizeMeters Desired largest model dimension in metres.
     * @param distanceMeters Extra distance from the model after reset.
     * @param heightOffsetMeters Vertical headset adjustment in metres.
     */
    void setViewTuning(double modelSizeMeters, double distanceMeters, double heightOffsetMeters);

    /**
     * @brief Sets the HDR environment path used by the VR renderer.
     * @param path Absolute or application-relative HDR file path; empty uses gradient background.
     */
    void setEnvironmentPath(const QString& path);

    /**
     * @brief Sets HDR orientation controls.
     * @param tiltXDegrees X-axis tilt in degrees.
     * @param tiltYDegrees Y-axis tilt in degrees.
     * @param headingDegrees Heading rotation in degrees.
     */
    void setEnvironmentOrientation(double tiltXDegrees, double tiltYDegrees, double headingDegrees);

    /**
     * @brief Queues a safe appearance update for a tracked VR actor.
     * @param partId Stable ModelPart identifier.
     * @param r Base colour red channel.
     * @param g Base colour green channel.
     * @param b Base colour blue channel.
     * @param visible True to show the actor.
     * @param glowEnabled True to apply glow material settings.
     * @param glowR Glow red channel.
     * @param glowG Glow green channel.
     * @param glowB Glow blue channel.
     */
    void setActorAppearance(const QString& partId, int r, int g, int b,
                            bool visible, bool glowEnabled, int glowR, int glowG, int glowB);

    /**
     * @brief Queues a safe geometry update for a tracked VR actor.
     * @param partId Stable ModelPart identifier.
     * @param geometry Deep-copied polydata snapshot to install in the VR mapper.
     */
    void setActorGeometry(const QString& partId, vtkPolyData* geometry);

    /**
     * @brief Queues a control command for the VR event loop.
     * @param cmd Command value from Command.
     * @param value Numeric command payload, such as speed or boolean state.
     */
    void issueCommand(int cmd, double value);

signals:
    /**
     * @brief Emitted when a tracked VR actor has been moved in the headset.
     * @param partId Stable ModelPart identifier.
     * @param px Desktop-space X position.
     * @param py Desktop-space Y position.
     * @param pz Desktop-space Z position.
     * @param rx Desktop-space X rotation in degrees.
     * @param ry Desktop-space Y rotation in degrees.
     * @param rz Desktop-space Z rotation in degrees.
     * @param scale Desktop-space uniform scale.
     */
    void actorTransformChanged(const QString& partId, double px, double py, double pz,
                               double rx, double ry, double rz, double scale);

    /**
     * @brief Emitted when a controller action changes a tracked actor colour.
     * @param partId Stable ModelPart identifier.
     * @param r Red channel in the range 0-255.
     * @param g Green channel in the range 0-255.
     * @param b Blue channel in the range 0-255.
     */
    void actorColorChanged(const QString& partId, int r, int g, int b);

protected:
    /**
     * @brief Builds the OpenVR scene and runs the VR event loop.
     */
    void run() override;

private:
    vtkSmartPointer<vtkOpenVRRenderWindow>              window; ///< OpenVR render window owned by the worker thread.
    vtkSmartPointer<vtkOpenVRRenderWindowInteractor>    interactor; ///< OpenVR interactor processing headset/controller events.
    vtkSmartPointer<vtkOpenVRRenderer>                  renderer; ///< VR renderer for actors, skybox, lights, and camera.
    vtkSmartPointer<vtkOpenVRCamera>                    camera; ///< Active OpenVR camera.
    vtkSmartPointer<vtkSkybox>                          environmentSkybox; ///< Spherical skybox actor for HDR backgrounds.
    vtkSmartPointer<vtkTexture>                         environmentTexture; ///< Texture used for image-based lighting and skybox.
    vtkSmartPointer<vtkImageData>                       environmentBaseImage; ///< Tonemapped HDR image before orientation adjustment.
    vtkSmartPointer<vtkImageData>                       environmentImage; ///< Oriented HDR image currently bound to the texture.

    QMutex                                              mutex; ///< Protects command flags and queued updates.
    QWaitCondition                                      condition; ///< Wakes the render loop after queued state changes.
    vtkSmartPointer<vtkActorCollection>                 actors; ///< Actors added before the VR renderer starts.

    /**
     * @struct TrackedActor
     * @brief Stores one VR actor and its desktop/VR transform mapping state.
     */
    struct TrackedActor {
        QString partId;                              ///< Stable ModelPart identifier.
        vtkSmartPointer<vtkActor> actor;             ///< Detached actor rendered in VR.
        double desktopPosition[3] = { 0.0, 0.0, 0.0 }; ///< Last desktop-space position.
        double desktopOrientation[3] = { 0.0, 0.0, 0.0 }; ///< Last desktop-space orientation.
        double desktopScale = 1.0;                   ///< Last desktop-space uniform scale.
        double lastPosition[3] = { 0.0, 0.0, 0.0 };  ///< Last observed VR position.
        double lastOrientation[3] = { 0.0, 0.0, 0.0 }; ///< Last observed VR orientation.
        double lastScale = 1.0;                      ///< Last observed VR scale.
        bool desktopTransformValid = false;          ///< True once desktop transform values are initialised.
        bool vrTransformApplied = false;             ///< True after desktop transform has been mapped into VR space.
        bool initialized = false;                    ///< True once change detection has a baseline sample.
    };
    QVector<TrackedActor> trackedActors;             ///< Tracked actor records for transform and colour callbacks.

    /**
     * @struct PendingAppearance
     * @brief Queued actor appearance update applied inside the VR thread.
     */
    struct PendingAppearance {
        QString partId;                              ///< Target ModelPart identifier.
        int r = 255;                                 ///< Base red channel.
        int g = 255;                                 ///< Base green channel.
        int b = 255;                                 ///< Base blue channel.
        bool visible = true;                         ///< Target actor visibility.
        bool glowEnabled = false;                    ///< Whether glow material should be applied.
        int glowR = 0;                               ///< Glow red channel.
        int glowG = 180;                             ///< Glow green channel.
        int glowB = 255;                             ///< Glow blue channel.
    };
    QVector<PendingAppearance> pendingAppearances;   ///< Appearance updates waiting for the VR loop.

    /**
     * @struct PendingGeometry
     * @brief Queued mesh update applied inside the VR thread.
     */
    struct PendingGeometry {
        QString partId;                              ///< Target ModelPart identifier.
        vtkSmartPointer<vtkPolyData> geometry;       ///< Deep-copied geometry snapshot.
    };
    QVector<PendingGeometry> pendingGeometries;      ///< Geometry updates waiting for the VR loop.

    std::chrono::time_point<std::chrono::steady_clock> t_last; ///< Time of the previous animation update.
    bool endRender = false;                          ///< True when the render loop should stop.
    double rotateX = 0.0;                            ///< Continuous X rotation per animation step.
    double rotateY = 0.0;                            ///< Continuous Y rotation per animation step.
    double rotateZ = 0.0;                            ///< Continuous Z rotation per animation step.
    bool isPaused = false;                           ///< True when automatic animation is paused.
    bool viewResetPending = false;                   ///< True when applyViewPose() should run.
    bool vrRemapPending = false;                     ///< True when desktop bounds/scale changed.
    bool environmentChangedPending = false;          ///< True when HDR path changed.
    bool environmentOrientationPending = false;      ///< True when HDR orientation changed.
    bool sceneBoundsValid = false;                   ///< True after valid model bounds are supplied.
    bool showcaseSpin = false;                       ///< True when showcase spin animation is active.
    QString environmentPath;                         ///< Current HDR file path, or empty for gradient background.
    double environmentTiltDegrees = 0.0;             ///< HDR X tilt in degrees.
    double environmentTiltYDegrees = 0.0;            ///< HDR Y tilt in degrees.
    double environmentHeadingDegrees = 0.0;          ///< HDR heading rotation in degrees.
    ViewMode viewMode = STANDING_VIEW;               ///< Active standing/sitting view preset.
    double viewYawDegrees = 28.0;                    ///< Reset-view yaw angle around the model.
    double viewModelSizeMeters = 2.4;                ///< Target largest model dimension in VR metres.
    double viewDistanceMeters = 1.1;                 ///< Extra standoff distance from the model.
    double viewHeightOffsetMeters = 0.0;             ///< User height adjustment in metres.
    double sceneBounds[6] = { -500.0, 500.0, -500.0, 500.0, 0.0, 1000.0 }; ///< Desktop-space model bounds.
    double desktopCenter[3] = { 0.0, 0.0, 0.0 };     ///< Desktop-space centre used for VR mapping.
    double vrBaseCenter[3] = { 0.0, 2.25, 0.0 };     ///< VR-space target centre for the model.
    double vrSceneBounds[6] = { -1.2, 1.2, 1.05, 3.45, 0.0, 1.2 }; ///< VR-space mapped scene bounds.
    double vrWorldScale = 0.001;                     ///< Conversion scale from desktop units to VR metres.

    /**
     * @brief Detects actor motion in VR and emits desktop transform updates.
     */
    void detectActorTransformChanges();

    /**
     * @brief Resets the physical VR camera pose using current view settings.
     */
    void applyViewPose();

    /**
     * @brief Loads or clears the VR HDR environment.
     */
    void configureEnvironment();

    /**
     * @brief Applies HDR tilt and heading to the active environment texture.
     */
    void applyEnvironmentOrientation();

    /**
     * @brief Applies queued appearance updates to tracked actors.
     * @param appearances Updates copied from the protected queue.
     */
    void applyPendingAppearances(const QVector<PendingAppearance>& appearances);

    /**
     * @brief Applies queued geometry snapshots to tracked actor mappers.
     * @param geometries Geometry updates copied from the protected queue.
     */
    void applyPendingGeometries(const QVector<PendingGeometry>& geometries);

    /**
     * @brief Recomputes desktop-to-VR scale and mapped scene bounds.
     */
    void updateVrInspectionTransform();

    /**
     * @brief Stores an actor transform in desktop space.
     * @param tracked Actor record to update.
     */
    void captureDesktopTransform(TrackedActor& tracked);

    /**
     * @brief Stores the current VR-space actor transform as the change-detection baseline.
     * @param tracked Actor record to update.
     */
    void rememberCurrentVrTransform(TrackedActor& tracked);

    /**
     * @brief Maps a tracked desktop transform into VR space and applies it to the actor.
     * @param tracked Actor record to update.
     */
    void applyVrInspectionTransform(TrackedActor& tracked);

    /**
     * @brief Converts a VR-space transform back into desktop-space values.
     * @param vrPosition VR-space XYZ position.
     * @param vrOrientation VR-space XYZ orientation in degrees.
     * @param vrScale VR-space uniform scale.
     * @param desktopPosition Output desktop-space position.
     * @param desktopOrientation Output desktop-space orientation.
     * @param desktopScale Output desktop-space uniform scale.
     */
    void mapVrTransformToDesktop(const double vrPosition[3], const double vrOrientation[3],
                                 double vrScale, double desktopPosition[3],
                                 double desktopOrientation[3], double& desktopScale) const;

    /**
     * @brief Handles controller colour-cycling for the currently picked actor.
     * @param eventData VTK controller event data from OpenVR.
     */
    void handleControllerColorAction(vtkEventData* eventData);

    /**
     * @brief Looks up the tracked ModelPart id for a VR actor.
     * @param actor Actor selected by the VR picker.
     * @return Part id, or an empty string when actor is not tracked.
     */
    QString partIdForActor(vtkActor* actor) const;
};

#endif
