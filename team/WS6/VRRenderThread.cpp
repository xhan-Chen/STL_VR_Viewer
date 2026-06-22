/**
 * @file VRRenderThread.cpp
 * @brief Implements the OpenVR render loop, controller actions, and VR scene mapping.
 *
 * This file keeps headset rendering and VTK actor mutations inside the VR thread.
 * It also maps transforms between desktop model units and VR metre units.
 */

#include "VRRenderThread.h"

 /* VTK Headers */
#include <vtkActor.h>
#include <vtkOpenVRRenderWindow.h>				
#include <vtkOpenVRRenderWindowInteractor.h>	
#include <vtkOpenVRRenderer.h>					
#include <vtkOpenVRCamera.h>	
#include <vtkOpenVRInteractorStyle.h>
#include <vtkVRInteractorStyle.h>
#include <vtkEventData.h>
#include <vtkCamera.h>
#include <vtkPropPicker.h>
#include <vtkLight.h>
#include <vtkMath.h>
#include <vtkTransform.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkNamedColors.h>
#include <vtkCylinderSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkSTLReader.h>
#include <vtkDataSetmapper.h>
#include <vtkCallbackCommand.h>
#include <vtkHDRReader.h>
#include <vtkImageData.h>
#include <vtkSkybox.h>
#include <vtkTexture.h>

#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QMutexLocker>

#include <cmath>
#include <algorithm>

namespace {
constexpr double kHdrUprightRollDegrees = -90.0;

vtkSmartPointer<vtkImageData> toneMapHdrForVrTexture(vtkImageData* hdrImage)
{
	vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
	if (!hdrImage) {
		return image;
	}

	int dims[3] = { 0, 0, 0 };
	hdrImage->GetDimensions(dims);
	image->SetDimensions(dims);
	image->SetOrigin(hdrImage->GetOrigin());
	image->SetSpacing(hdrImage->GetSpacing());
	image->AllocateScalars(VTK_UNSIGNED_CHAR, 3);

	const int components = std::max(1, hdrImage->GetNumberOfScalarComponents());
	constexpr double exposure = 0.75;
	constexpr double gamma = 1.0 / 2.2;

	for (int z = 0; z < std::max(1, dims[2]); ++z) {
		for (int y = 0; y < dims[1]; ++y) {
			for (int x = 0; x < dims[0]; ++x) {
				auto* output = static_cast<unsigned char*>(image->GetScalarPointer(x, y, z));
				for (int c = 0; c < 3; ++c) {
					const int sourceComponent = std::min(c, components - 1);
					const double source = std::max(0.0, hdrImage->GetScalarComponentAsDouble(x, y, z, sourceComponent));
					const double mapped = std::pow(std::clamp(1.0 - std::exp(-source * exposure), 0.0, 1.0), gamma);
					output[c] = static_cast<unsigned char>(std::lround(mapped * 255.0));
				}
			}
		}
	}

	return image;
}

double wrapUnit(double value)
{
	value -= std::floor(value);
	return value < 0.0 ? value + 1.0 : value;
}

void sampleEnvironmentPixel(vtkImageData* image, double u, double v, unsigned char rgb[3])
{
	int dims[3] = { 0, 0, 0 };
	image->GetDimensions(dims);
	const int width = std::max(1, dims[0]);
	const int height = std::max(1, dims[1]);

	u = wrapUnit(u);
	v = std::clamp(v, 0.0, 1.0);

	const double sx = u * width - 0.5;
	const double sy = v * height - 0.5;
	const int x0 = static_cast<int>(std::floor(sx));
	const int y0 = std::clamp(static_cast<int>(std::floor(sy)), 0, height - 1);
	const int x1 = x0 + 1;
	const int y1 = std::clamp(y0 + 1, 0, height - 1);
	const double tx = sx - std::floor(sx);
	const double ty = sy - std::floor(sy);
	const int wx0 = (x0 % width + width) % width;
	const int wx1 = (x1 % width + width) % width;

	auto* p00 = static_cast<unsigned char*>(image->GetScalarPointer(wx0, y0, 0));
	auto* p10 = static_cast<unsigned char*>(image->GetScalarPointer(wx1, y0, 0));
	auto* p01 = static_cast<unsigned char*>(image->GetScalarPointer(wx0, y1, 0));
	auto* p11 = static_cast<unsigned char*>(image->GetScalarPointer(wx1, y1, 0));

	for (int c = 0; c < 3; ++c) {
		const double top = p00[c] * (1.0 - tx) + p10[c] * tx;
		const double bottom = p01[c] * (1.0 - tx) + p11[c] * tx;
		rgb[c] = static_cast<unsigned char>(std::lround(std::clamp(top * (1.0 - ty) + bottom * ty, 0.0, 255.0)));
	}
}

vtkSmartPointer<vtkImageData> orientedEnvironmentImage(vtkImageData* sourceImage,
	double rollDegrees,
	double pitchDegrees,
	double headingDegrees)
{
	vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
	if (!sourceImage) return image;

	int dims[3] = { 0, 0, 0 };
	sourceImage->GetDimensions(dims);
	image->SetDimensions(dims);
	image->SetOrigin(sourceImage->GetOrigin());
	image->SetSpacing(sourceImage->GetSpacing());
	image->AllocateScalars(VTK_UNSIGNED_CHAR, 3);

	const int width = std::max(1, dims[0]);
	const int height = std::max(1, dims[1]);
	constexpr double pi = 3.14159265358979323846;
	const double inverseRoll = vtkMath::RadiansFromDegrees(-rollDegrees);
	const double inversePitch = vtkMath::RadiansFromDegrees(-pitchDegrees);
	const double inverseHeading = vtkMath::RadiansFromDegrees(-headingDegrees);
	const double cosRoll = std::cos(inverseRoll);
	const double sinRoll = std::sin(inverseRoll);
	const double cosPitch = std::cos(inversePitch);
	const double sinPitch = std::sin(inversePitch);
	const double cosHeading = std::cos(inverseHeading);
	const double sinHeading = std::sin(inverseHeading);

	for (int y = 0; y < height; ++y) {
		const double v = (y + 0.5) / height;
		const double latitude = (0.5 - v) * pi;
		const double cosLatitude = std::cos(latitude);
		for (int x = 0; x < width; ++x) {
			const double u = (x + 0.5) / width;
			const double longitude = (u - 0.5) * 2.0 * pi;

			double dx = cosLatitude * std::sin(longitude);
			double dy = std::sin(latitude);
			double dz = cosLatitude * std::cos(longitude);

			const double rolledX = cosRoll * dx - sinRoll * dy;
			const double rolledY = sinRoll * dx + cosRoll * dy;
			const double rolledZ = dz;

			const double pitchedX = rolledX;
			const double pitchedY = cosPitch * rolledY - sinPitch * rolledZ;
			const double pitchedZ = sinPitch * rolledY + cosPitch * rolledZ;

			const double sourceX = cosHeading * pitchedX + sinHeading * pitchedZ;
			const double sourceY = pitchedY;
			const double sourceZ = -sinHeading * pitchedX + cosHeading * pitchedZ;

			const double sourceLongitude = std::atan2(sourceX, sourceZ);
			const double sourceLatitude = std::asin(std::clamp(sourceY, -1.0, 1.0));
			const double sourceU = sourceLongitude / (2.0 * pi) + 0.5;
			const double sourceV = 0.5 - sourceLatitude / pi;

			unsigned char rgb[3] = { 0, 0, 0 };
			sampleEnvironmentPixel(sourceImage, sourceU, sourceV, rgb);
			auto* output = static_cast<unsigned char*>(image->GetScalarPointer(x, y, 0));
			output[0] = rgb[0];
			output[1] = rgb[1];
			output[2] = rgb[2];
		}
	}

	return image;
}
}

/**
 * VRRenderThread constructor.
 * Initializes common state variables. Note: The VR Interactor loop blocks the thread,
 * hence it must run in this dedicated thread rather than the GUI thread.
 */
VRRenderThread::VRRenderThread(QObject* parent)
	: QThread(parent) {
	actors = vtkSmartPointer<vtkActorCollection>::New();
}

/**
 * Destructor.
 * Ensures proper cleanup of resources to prevent memory leaks during start/stop cycles.
 */
VRRenderThread::~VRRenderThread() {
	issueCommand(END_RENDER, 0.0);
	wait(3000);
}

/**
 * Adds an actor to the internal collection while the thread is not yet running.
 */
void VRRenderThread::addActorOffline(vtkActor* actor) {
	addActorOffline(QString(), actor);
}

void VRRenderThread::addActorOffline(const QString& partId, vtkActor* actor) {
	if (!this->isRunning()) {
		actor->DragableOn();
		actors->AddItem(actor);
		TrackedActor tracked;
		tracked.partId = partId;
		tracked.actor = actor;
		captureDesktopTransform(tracked);
		trackedActors.append(tracked);
	}
}

void VRRenderThread::setSceneBounds(const double bounds[6])
{
	QMutexLocker locker(&mutex);
	for (int i = 0; i < 6; ++i) {
		sceneBounds[i] = bounds[i];
	}
	sceneBoundsValid = true;
	if (isRunning()) {
		vrRemapPending = true;
	}
	else {
		for (TrackedActor& tracked : trackedActors) {
			captureDesktopTransform(tracked);
		}
		updateVrInspectionTransform();
		for (TrackedActor& tracked : trackedActors) {
			applyVrInspectionTransform(tracked);
		}
	}
	viewResetPending = true;
	condition.wakeAll();
}

void VRRenderThread::setViewMode(ViewMode mode)
{
	QMutexLocker locker(&mutex);
	viewMode = mode;
	viewResetPending = true;
	condition.wakeAll();
}

void VRRenderThread::setViewYawDegrees(double yawDegrees)
{
	QMutexLocker locker(&mutex);
	viewYawDegrees = yawDegrees;
	viewResetPending = true;
	condition.wakeAll();
}

void VRRenderThread::setViewTuning(double modelSizeMeters, double distanceMeters, double heightOffsetMeters)
{
	QMutexLocker locker(&mutex);
	viewModelSizeMeters = std::clamp(modelSizeMeters, 0.25, 8.0);
	viewDistanceMeters = std::clamp(distanceMeters, 0.4, 8.0);
	viewHeightOffsetMeters = std::clamp(heightOffsetMeters, -2.0, 2.0);
	if (isRunning()) {
		vrRemapPending = true;
	}
	else {
		for (TrackedActor& tracked : trackedActors) {
			captureDesktopTransform(tracked);
		}
		updateVrInspectionTransform();
		for (TrackedActor& tracked : trackedActors) {
			applyVrInspectionTransform(tracked);
		}
	}
	viewResetPending = true;
	condition.wakeAll();
}

void VRRenderThread::setEnvironmentPath(const QString& path)
{
	QMutexLocker locker(&mutex);
	environmentPath = path;
	environmentChangedPending = true;
	condition.wakeAll();
}

void VRRenderThread::setEnvironmentOrientation(double tiltXDegrees, double tiltYDegrees, double headingDegrees)
{
	QMutexLocker locker(&mutex);
	environmentTiltDegrees = std::clamp(tiltXDegrees, -180.0, 180.0);
	environmentTiltYDegrees = std::clamp(tiltYDegrees, -180.0, 180.0);
	environmentHeadingDegrees = std::clamp(headingDegrees, -180.0, 180.0);
	environmentOrientationPending = true;
	condition.wakeAll();
}

void VRRenderThread::setActorAppearance(const QString& partId, int r, int g, int b,
	bool visible, bool glowEnabled, int glowR, int glowG, int glowB)
{
	if (partId.isEmpty()) {
		return;
	}

	QMutexLocker locker(&mutex);
	PendingAppearance appearance;
	appearance.partId = partId;
	appearance.r = std::clamp(r, 0, 255);
	appearance.g = std::clamp(g, 0, 255);
	appearance.b = std::clamp(b, 0, 255);
	appearance.visible = visible;
	appearance.glowEnabled = glowEnabled;
	appearance.glowR = std::clamp(glowR, 0, 255);
	appearance.glowG = std::clamp(glowG, 0, 255);
	appearance.glowB = std::clamp(glowB, 0, 255);
	pendingAppearances.append(appearance);
	condition.wakeAll();
}

void VRRenderThread::setActorGeometry(const QString& partId, vtkPolyData* geometry)
{
	if (partId.isEmpty() || !geometry) {
		return;
	}

	QMutexLocker locker(&mutex);
	PendingGeometry update;
	update.partId = partId;
	update.geometry = geometry;
	pendingGeometries.append(update);
	condition.wakeAll();
}

/**
 * Interface to update rendering state or transformation parameters from the GUI thread.
 */
void VRRenderThread::issueCommand(int cmd, double value) {
	QMutexLocker locker(&mutex);
	switch (cmd) {
	case END_RENDER:
		this->endRender = true;
		break;

	case PAUSE_RENDER:
		this->isPaused = (value > 0.0);
		break;

	case ROTATE_X:
		this->rotateX = value;
		break;

	case ROTATE_Y:
		this->rotateY = value;
		break;

	case ROTATE_Z:
		this->rotateZ = value;
		break;

	case RESET_VIEW:
		this->viewResetPending = true;
		break;

	case SET_VIEW_MODE:
		this->viewMode = (value >= 0.5) ? SITTING_VIEW : STANDING_VIEW;
		this->viewResetPending = true;
		break;

	case SET_SHOWCASE_SPIN:
		this->showcaseSpin = (value > 0.0);
		this->rotateZ = this->showcaseSpin ? 0.18 : 0.0;
		break;
	}
	condition.wakeAll();
}

/**
 * Main execution loop for the VR thread.
 * Triggered by VRRenderThread::start().
 */
void VRRenderThread::run() {
	vtkNew<vtkNamedColors> colors;

	/* Set environment background color */
	std::array<unsigned char, 4> bkg{ {26, 51, 102, 255} };
	colors->SetColor("BkgColor", bkg.data());

	renderer = vtkSmartPointer<vtkOpenVRRenderer>::New();
	renderer->SetBackground(colors->GetColor3d("BkgColor").GetData());
	configureEnvironment();

	/* Populate scene with provided actors */
	qDebug() << "VR Thread starting. Actor count:" << actors->GetNumberOfItems();

	for (TrackedActor& tracked : trackedActors) {
		applyVrInspectionTransform(tracked);
		if (tracked.actor) {
			renderer->AddActor(tracked.actor);
		}
	}

	/* Setup illumination */
	vtkSmartPointer<vtkLight> mainLight = vtkSmartPointer<vtkLight>::New();
	mainLight->SetPosition(100, 100, 100);
	mainLight->SetFocalPoint(0, 0, 0);
	mainLight->SetIntensity(1.0);
	renderer->AddLight(mainLight);

	vtkSmartPointer<vtkLight> fillLight = vtkSmartPointer<vtkLight>::New();
	fillLight->SetPosition(-100, -100, -100);
	fillLight->SetFocalPoint(0, 0, 0);
	fillLight->SetIntensity(0.4);
	renderer->AddLight(fillLight);

	/* Initialize OpenVR window and camera */
	window = vtkSmartPointer<vtkOpenVRRenderWindow>::New();
	window->Initialize();
	window->AddRenderer(renderer);

	camera = vtkSmartPointer<vtkOpenVRCamera>::New();
	renderer->SetActiveCamera(camera);

	interactor = vtkSmartPointer<vtkOpenVRRenderWindowInteractor>::New();
	interactor->SetRenderWindow(window);
	QString manifestDirectory = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("vrbindings");
	if (!QDir(manifestDirectory).exists()) {
		manifestDirectory = QCoreApplication::applicationDirPath();
	}
	interactor->SetActionManifestDirectory((QDir::toNativeSeparators(manifestDirectory) + QDir::separator()).toStdString());

	vtkSmartPointer<vtkOpenVRInteractorStyle> style = vtkSmartPointer<vtkOpenVRInteractorStyle>::New();
	style->SetDefaultRenderer(renderer);
	style->SetStyle(vtkVRInteractorStyle::GROUNDED_STYLE);
	style->GrabWithRayOn();
	style->HoverPickOn();
	style->AddTooltipForInput(vtkEventDataDevice::RightController, vtkEventDataDeviceInput::Trigger, "Grab and move a car part");
	style->AddTooltipForInput(vtkEventDataDevice::RightController, vtkEventDataDeviceInput::Grip, "Change picked part colour");
	style->AddTooltipForInput(vtkEventDataDevice::RightController, vtkEventDataDeviceInput::ApplicationMenu, "Reset view");
	style->AddTooltipForInput(vtkEventDataDevice::LeftController, vtkEventDataDeviceInput::Trigger, "Reset the VR view");
	style->AddTooltipForInput(vtkEventDataDevice::LeftController, vtkEventDataDeviceInput::Grip, "Reset the VR view");
	interactor->SetInteractorStyle(style);
	interactor->AddAction("/actions/vtk/in/RightGripAction", false,
		[this](vtkEventData* eventData) { this->handleControllerColorAction(eventData); });
	interactor->AddAction("/actions/vtk/in/LeftGripAction", false,
		[this](vtkEventData* eventData) {
			vtkEventDataForDevice* deviceEvent = eventData ? eventData->GetAsEventDataForDevice() : nullptr;
			if (deviceEvent && deviceEvent->GetAction() == vtkEventDataAction::Press) {
				QMutexLocker locker(&mutex);
				this->viewResetPending = true;
			}
		});
	interactor->AddAction("/actions/vtk/in/NextCameraPose", false,
		[this](vtkEventData* eventData) {
			vtkEventDataForDevice* deviceEvent = eventData ? eventData->GetAsEventDataForDevice() : nullptr;
			if (deviceEvent && deviceEvent->GetAction() == vtkEventDataAction::Press) {
				QMutexLocker locker(&mutex);
				this->viewResetPending = true;
			}
		});
	interactor->AddAction("/actions/vtk/in/ShowMenu", false,
		[this](vtkEventData* eventData) {
			vtkEventDataForDevice* deviceEvent = eventData ? eventData->GetAsEventDataForDevice() : nullptr;
			if (deviceEvent && deviceEvent->GetAction() == vtkEventDataAction::Press) {
				QMutexLocker locker(&mutex);
				this->viewResetPending = true;
			}
		});
	window->Render();
	interactor->Initialize();
	window->Render();
	applyViewPose();
	window->Render();

	{
		QMutexLocker locker(&mutex);
		endRender = false;
	}
	t_last = std::chrono::steady_clock::now();

	/* Execute main render and event loop */
	while (!interactor->GetDone()) {
		double stepX = 0.0;
		double stepY = 0.0;
		double stepZ = 0.0;
		bool paused = false;
		bool spinEnabled = false;
		{
			QMutexLocker locker(&mutex);
			if (this->endRender) {
				break;
			}
			paused = this->isPaused;
			stepX = this->rotateX;
			stepY = this->rotateY;
			stepZ = this->rotateZ;
			spinEnabled = this->showcaseSpin;
		}

		interactor->DoOneEvent(window, renderer);

		bool shouldUpdateEnvironment = false;
		bool shouldUpdateEnvironmentOrientation = false;
		bool shouldRemapScene = false;
		bool shouldResetView = false;
		QVector<PendingAppearance> appearanceUpdates;
		QVector<PendingGeometry> geometryUpdates;
		{
			QMutexLocker locker(&mutex);
			shouldUpdateEnvironment = this->environmentChangedPending;
			this->environmentChangedPending = false;
			shouldUpdateEnvironmentOrientation = this->environmentOrientationPending;
			this->environmentOrientationPending = false;
			shouldRemapScene = this->vrRemapPending;
			this->vrRemapPending = false;
			shouldResetView = this->viewResetPending;
			this->viewResetPending = false;
			appearanceUpdates = this->pendingAppearances;
			this->pendingAppearances.clear();
			geometryUpdates = this->pendingGeometries;
			this->pendingGeometries.clear();
		}
		if (shouldUpdateEnvironment) {
			configureEnvironment();
		}
		else if (shouldUpdateEnvironmentOrientation) {
			applyEnvironmentOrientation();
		}
		if (shouldRemapScene) {
			for (TrackedActor& tracked : trackedActors) {
				captureDesktopTransform(tracked);
			}
			{
				QMutexLocker locker(&mutex);
				updateVrInspectionTransform();
			}
			for (TrackedActor& tracked : trackedActors) {
				applyVrInspectionTransform(tracked);
			}
			shouldResetView = true;
		}
		if (shouldResetView) {
			applyViewPose();
		}
		if (!geometryUpdates.isEmpty()) {
			applyPendingGeometries(geometryUpdates);
		}
		if (!appearanceUpdates.isEmpty()) {
			applyPendingAppearances(appearanceUpdates);
		}

		/* Periodic update of actor transformations if not paused (~50 FPS) */
		if (!paused && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t_last).count() > 20) {

			vtkActorCollection* actorList = renderer->GetActors();
			vtkActor* act;

			/* Apply incremental rotations to all actors in scene */
			actorList->InitTraversal();
			while ((act = (vtkActor*)actorList->GetNextActor())) {
				act->RotateX(stepX);
				act->RotateY(stepY);
				act->RotateZ(stepZ);
				if (spinEnabled) {
					act->Modified();
				}
			}

			t_last = std::chrono::steady_clock::now();
		}

		detectActorTransformChanges();
	}

	interactor = nullptr;
	camera = nullptr;
	window = nullptr;
	renderer = nullptr;
}

void VRRenderThread::applyViewPose()
{
	if (!window || !renderer || !camera) {
		return;
	}

	ViewMode mode = STANDING_VIEW;
	double yawDegrees = 28.0;
	double distanceMeters = 1.1;
	double heightOffsetMeters = 0.0;
	double bounds[6];
	{
		QMutexLocker locker(&mutex);
		mode = viewMode;
		yawDegrees = viewYawDegrees;
		distanceMeters = viewDistanceMeters;
		heightOffsetMeters = viewHeightOffsetMeters;
		for (int i = 0; i < 6; ++i) {
			bounds[i] = vrSceneBounds[i];
		}
	}

	const double width = std::max(1.0, bounds[1] - bounds[0]);
	const double depth = std::max(1.0, bounds[3] - bounds[2]);
	const double height = std::max(1.0, bounds[5] - bounds[4]);
	const double largest = std::max({ width, depth, height });
	const double eyeHeight = (mode == SITTING_VIEW ? 1.15 : 1.65);

	const double yawRadians = vtkMath::RadiansFromDegrees(yawDegrees);
	double viewDirection[3] = { std::sin(yawRadians), std::cos(yawRadians), 0.0 };
	vtkMath::Normalize(viewDirection);
	double viewUp[3] = { 0.0, 0.0, 1.0 };

	const double halfExtentTowardViewer =
		std::abs(viewDirection[0]) * width * 0.5 +
		std::abs(viewDirection[1]) * depth * 0.5;
	const double halfHeight = height * 0.5;
	const double standOff = std::max(
		std::max(halfExtentTowardViewer * 1.05, halfHeight * 1.1) + distanceMeters,
		distanceMeters + 0.55);
	const double target[3] = {
		(bounds[0] + bounds[1]) * 0.5,
		(bounds[2] + bounds[3]) * 0.5,
		bounds[4] + std::min(height * 0.52, eyeHeight * 0.75)
	};
	double desiredEye[3] = {
		target[0] - viewDirection[0] * standOff,
		target[1] - viewDirection[1] * standOff,
		bounds[4] + eyeHeight + heightOffsetMeters
	};
	double physicalTranslation[3] = {
		desiredEye[0] - viewUp[0] * eyeHeight,
		desiredEye[1] - viewUp[1] * eyeHeight,
		desiredEye[2] - viewUp[2] * eyeHeight
	};

	window->SetPhysicalScale(1.0);
	window->SetPhysicalViewUp(viewUp);
	window->SetPhysicalViewDirection(viewDirection);
	window->SetPhysicalTranslation(physicalTranslation);

	vtkNew<vtkCamera> sourceCamera;
	sourceCamera->SetViewUp(viewUp);
	sourceCamera->SetPosition(desiredEye);
	sourceCamera->SetFocalPoint(target);
	sourceCamera->SetViewAngle(42.0);
	window->InitializeViewFromCamera(sourceCamera);
	window->SetPhysicalScale(1.0);
	window->SetPhysicalViewUp(viewUp);
	window->SetPhysicalViewDirection(viewDirection);
	window->SetPhysicalTranslation(physicalTranslation);

	camera->SetViewUp(viewUp);
	camera->SetPosition(desiredEye);
	camera->SetFocalPoint(target);
	camera->SetViewAngle(42.0);
	renderer->ResetCameraClippingRange();
	camera->SetClippingRange(
		std::max(0.02, largest * 0.001),
		std::max(standOff + largest * 4.0, 12.0));
	window->Render();
}

void VRRenderThread::configureEnvironment()
{
	if (!renderer) {
		return;
	}

	QString path;
	{
		QMutexLocker locker(&mutex);
		path = environmentPath;
	}

	if (environmentSkybox) {
		renderer->RemoveActor(environmentSkybox);
	}
	environmentSkybox = nullptr;
	environmentTexture = nullptr;
	environmentBaseImage = nullptr;
	environmentImage = nullptr;
	renderer->UseImageBasedLightingOff();
	renderer->SetEnvironmentTexture(nullptr);

	if (path.isEmpty()) {
		renderer->GradientBackgroundOn();
		renderer->SetBackground(0.03, 0.08, 0.18);
		renderer->SetBackground2(0.10, 0.16, 0.28);
		applyEnvironmentOrientation();
		return;
	}

	vtkSmartPointer<vtkHDRReader> reader = vtkSmartPointer<vtkHDRReader>::New();
	reader->SetFileName(path.toStdString().c_str());
	reader->Update();

	vtkImageData* hdrImage = reader->GetOutput();
	if (!hdrImage || hdrImage->GetNumberOfPoints() == 0) {
		renderer->GradientBackgroundOn();
		renderer->SetBackground(0.03, 0.08, 0.18);
		renderer->SetBackground2(0.10, 0.16, 0.28);
		applyEnvironmentOrientation();
		return;
	}

	environmentBaseImage = toneMapHdrForVrTexture(hdrImage);
	environmentImage = environmentBaseImage;
	environmentTexture = vtkSmartPointer<vtkTexture>::New();
	environmentTexture->SetInputData(environmentImage);
	environmentTexture->SetColorModeToDirectScalars();
	environmentTexture->MipmapOn();
	environmentTexture->InterpolateOn();

	environmentSkybox = vtkSmartPointer<vtkSkybox>::New();
	environmentSkybox->SetProjectionToSphere();
	environmentSkybox->SetTexture(environmentTexture);
	environmentSkybox->PickableOff();
	environmentSkybox->GammaCorrectOff();
	renderer->AddActor(environmentSkybox);

	renderer->UseImageBasedLightingOn();
	renderer->SetEnvironmentTexture(environmentTexture, true);
	renderer->GradientBackgroundOff();
	applyEnvironmentOrientation();
}

void VRRenderThread::applyEnvironmentOrientation()
{
	if (!renderer) {
		return;
	}

	double tiltDegrees = 0.0;
	double tiltYDegrees = 0.0;
	double headingDegrees = 0.0;
	{
		QMutexLocker locker(&mutex);
		tiltDegrees = environmentTiltDegrees;
		tiltYDegrees = environmentTiltYDegrees;
		headingDegrees = environmentHeadingDegrees;
	}
	const double correctedRollDegrees = kHdrUprightRollDegrees + tiltDegrees;

	renderer->SetEnvironmentUp(0.0, 0.0, 1.0);
	renderer->SetEnvironmentRight(1.0, 0.0, 0.0);

	if (environmentBaseImage && environmentTexture) {
		environmentImage = orientedEnvironmentImage(environmentBaseImage, correctedRollDegrees, tiltYDegrees, headingDegrees);
		environmentTexture->SetInputData(environmentImage);
		environmentTexture->Modified();
		renderer->SetEnvironmentTexture(environmentTexture, true);
	}

	if (environmentSkybox) {
		environmentSkybox->SetOrientation(0.0, 0.0, 0.0);
		environmentSkybox->Modified();
	}
}

void VRRenderThread::applyPendingAppearances(const QVector<PendingAppearance>& appearances)
{
	if (appearances.isEmpty()) {
		return;
	}

	for (const PendingAppearance& appearance : appearances) {
		for (TrackedActor& tracked : trackedActors) {
			if (tracked.partId != appearance.partId || !tracked.actor) {
				continue;
			}

			tracked.actor->SetVisibility(appearance.visible);
			vtkProperty* property = tracked.actor->GetProperty();
			property->SetColor(appearance.r / 255.0, appearance.g / 255.0, appearance.b / 255.0);
			property->SetDiffuse(0.72);
			property->SetSpecular(0.28);
			property->SetSpecularPower(32.0);
			property->SetAmbient(0.18);
			property->SetEmissiveFactor(0.0, 0.0, 0.0);
			property->EdgeVisibilityOff();
			property->SetLineWidth(1.0);
			property->SetEdgeWidth(1.0);

			if (appearance.glowEnabled) {
				const double glowR = appearance.glowR / 255.0;
				const double glowG = appearance.glowG / 255.0;
				const double glowB = appearance.glowB / 255.0;
				property->SetAmbient(0.85);
				property->SetDiffuse(0.45);
				property->SetSpecular(0.75);
				property->SetSpecularPower(90.0);
				property->SetEmissiveFactor(glowR, glowG, glowB);
				property->SetEdgeColor(glowR, glowG, glowB);
				property->SetEdgeVisibility(true);
				property->SetEdgeWidth(2.0);
			}

			tracked.actor->Modified();
		}
	}

	if (window) {
		window->Render();
	}
}

void VRRenderThread::applyPendingGeometries(const QVector<PendingGeometry>& geometries)
{
	if (geometries.isEmpty()) {
		return;
	}

	for (const PendingGeometry& update : geometries) {
		if (!update.geometry || update.geometry->GetNumberOfPoints() == 0 || update.geometry->GetNumberOfCells() == 0) {
			continue;
		}

		for (TrackedActor& tracked : trackedActors) {
			if (tracked.partId != update.partId || !tracked.actor) {
				continue;
			}

			vtkPolyDataMapper* mapper = vtkPolyDataMapper::SafeDownCast(tracked.actor->GetMapper());
			if (!mapper) {
				continue;
			}

			mapper->SetInputData(update.geometry);
			mapper->Update();
			tracked.actor->Modified();
		}
	}

	if (window) {
		window->Render();
	}
}

void VRRenderThread::updateVrInspectionTransform()
{
	const double width = std::max(1.0, sceneBounds[1] - sceneBounds[0]);
	const double depth = std::max(1.0, sceneBounds[3] - sceneBounds[2]);
	const double height = std::max(1.0, sceneBounds[5] - sceneBounds[4]);
	const double largest = std::max({ width, depth, height });
	const double targetLargestMeters = std::clamp(viewModelSizeMeters, 0.25, 8.0);

	desktopCenter[0] = (sceneBounds[0] + sceneBounds[1]) * 0.5;
	desktopCenter[1] = (sceneBounds[2] + sceneBounds[3]) * 0.5;
	desktopCenter[2] = sceneBounds[4];
	vrBaseCenter[0] = 0.0;
	vrBaseCenter[1] = 1.8;
	vrBaseCenter[2] = 0.0;
	vrWorldScale = targetLargestMeters / largest;

	vrSceneBounds[0] = (sceneBounds[0] - desktopCenter[0]) * vrWorldScale + vrBaseCenter[0];
	vrSceneBounds[1] = (sceneBounds[1] - desktopCenter[0]) * vrWorldScale + vrBaseCenter[0];
	vrSceneBounds[2] = (sceneBounds[2] - desktopCenter[1]) * vrWorldScale + vrBaseCenter[1];
	vrSceneBounds[3] = (sceneBounds[3] - desktopCenter[1]) * vrWorldScale + vrBaseCenter[1];
	vrSceneBounds[4] = (sceneBounds[4] - desktopCenter[2]) * vrWorldScale + vrBaseCenter[2];
	vrSceneBounds[5] = (sceneBounds[5] - desktopCenter[2]) * vrWorldScale + vrBaseCenter[2];
}

void VRRenderThread::captureDesktopTransform(TrackedActor& tracked)
{
	if (!tracked.actor) {
		return;
	}

	double* position = tracked.actor->GetPosition();
	double* orientation = tracked.actor->GetOrientation();
	double* scale = tracked.actor->GetScale();
	const double uniformScale = std::max(0.0001, (scale[0] + scale[1] + scale[2]) / 3.0);

	if (tracked.vrTransformApplied) {
		mapVrTransformToDesktop(position, orientation, uniformScale,
			tracked.desktopPosition, tracked.desktopOrientation, tracked.desktopScale);
	}
	else {
		tracked.desktopPosition[0] = position[0];
		tracked.desktopPosition[1] = position[1];
		tracked.desktopPosition[2] = position[2];
		tracked.desktopOrientation[0] = orientation[0];
		tracked.desktopOrientation[1] = orientation[1];
		tracked.desktopOrientation[2] = orientation[2];
		tracked.desktopScale = uniformScale;
	}

	tracked.desktopTransformValid = true;
}

void VRRenderThread::rememberCurrentVrTransform(TrackedActor& tracked)
{
	if (!tracked.actor) {
		return;
	}

	double* position = tracked.actor->GetPosition();
	double* orientation = tracked.actor->GetOrientation();
	double* scale = tracked.actor->GetScale();
	const double uniformScale = std::max(0.0001, (scale[0] + scale[1] + scale[2]) / 3.0);

	tracked.lastPosition[0] = position[0];
	tracked.lastPosition[1] = position[1];
	tracked.lastPosition[2] = position[2];
	tracked.lastOrientation[0] = orientation[0];
	tracked.lastOrientation[1] = orientation[1];
	tracked.lastOrientation[2] = orientation[2];
	tracked.lastScale = uniformScale;
	tracked.initialized = true;
}

void VRRenderThread::applyVrInspectionTransform(TrackedActor& tracked)
{
	if (!tracked.actor) {
		return;
	}

	if (!tracked.desktopTransformValid) {
		captureDesktopTransform(tracked);
	}

	tracked.actor->SetPosition(
		(tracked.desktopPosition[0] - desktopCenter[0]) * vrWorldScale + vrBaseCenter[0],
		(tracked.desktopPosition[1] - desktopCenter[1]) * vrWorldScale + vrBaseCenter[1],
		(tracked.desktopPosition[2] - desktopCenter[2]) * vrWorldScale + vrBaseCenter[2]);
	tracked.actor->SetOrientation(tracked.desktopOrientation);
	tracked.actor->SetScale(
		tracked.desktopScale * vrWorldScale,
		tracked.desktopScale * vrWorldScale,
		tracked.desktopScale * vrWorldScale);
	tracked.actor->Modified();
	tracked.vrTransformApplied = true;
	rememberCurrentVrTransform(tracked);
}

void VRRenderThread::mapVrTransformToDesktop(const double vrPosition[3], const double vrOrientation[3],
	double scale, double desktopPosition[3], double desktopOrientation[3], double& desktopScale) const
{
	desktopPosition[0] = (vrPosition[0] - vrBaseCenter[0]) / vrWorldScale + desktopCenter[0];
	desktopPosition[1] = (vrPosition[1] - vrBaseCenter[1]) / vrWorldScale + desktopCenter[1];
	desktopPosition[2] = (vrPosition[2] - vrBaseCenter[2]) / vrWorldScale + desktopCenter[2];
	desktopOrientation[0] = vrOrientation[0];
	desktopOrientation[1] = vrOrientation[1];
	desktopOrientation[2] = vrOrientation[2];
	desktopScale = scale / vrWorldScale;
}

void VRRenderThread::detectActorTransformChanges()
{
	{
		QMutexLocker locker(&mutex);
		if (showcaseSpin) {
			return;
		}
	}

	for (TrackedActor& tracked : trackedActors) {
		if (tracked.partId.isEmpty() || !tracked.actor) {
			continue;
		}

		double* position = tracked.actor->GetPosition();
		double* orientation = tracked.actor->GetOrientation();
		double* scale = tracked.actor->GetScale();
		const double uniformScale = (scale[0] + scale[1] + scale[2]) / 3.0;

		if (!tracked.initialized) {
			tracked.initialized = true;
			tracked.lastPosition[0] = position[0];
			tracked.lastPosition[1] = position[1];
			tracked.lastPosition[2] = position[2];
			tracked.lastOrientation[0] = orientation[0];
			tracked.lastOrientation[1] = orientation[1];
			tracked.lastOrientation[2] = orientation[2];
			tracked.lastScale = uniformScale;
			continue;
		}

		const bool changed = std::abs(position[0] - tracked.lastPosition[0]) > 0.001
			|| std::abs(position[1] - tracked.lastPosition[1]) > 0.001
			|| std::abs(position[2] - tracked.lastPosition[2]) > 0.001
			|| std::abs(orientation[0] - tracked.lastOrientation[0]) > 0.01
			|| std::abs(orientation[1] - tracked.lastOrientation[1]) > 0.01
			|| std::abs(orientation[2] - tracked.lastOrientation[2]) > 0.01
			|| std::abs(uniformScale - tracked.lastScale) > 0.001;

		if (!changed) {
			continue;
		}

		tracked.lastPosition[0] = position[0];
		tracked.lastPosition[1] = position[1];
		tracked.lastPosition[2] = position[2];
		tracked.lastOrientation[0] = orientation[0];
		tracked.lastOrientation[1] = orientation[1];
		tracked.lastOrientation[2] = orientation[2];
		tracked.lastScale = uniformScale;

		double desktopPosition[3];
		double desktopOrientation[3];
		double desktopScale = 1.0;
		mapVrTransformToDesktop(position, orientation, uniformScale,
			desktopPosition, desktopOrientation, desktopScale);

		tracked.desktopPosition[0] = desktopPosition[0];
		tracked.desktopPosition[1] = desktopPosition[1];
		tracked.desktopPosition[2] = desktopPosition[2];
		tracked.desktopOrientation[0] = desktopOrientation[0];
		tracked.desktopOrientation[1] = desktopOrientation[1];
		tracked.desktopOrientation[2] = desktopOrientation[2];
		tracked.desktopScale = desktopScale;
		tracked.desktopTransformValid = true;
		tracked.vrTransformApplied = true;

		emit actorTransformChanged(tracked.partId,
			desktopPosition[0], desktopPosition[1], desktopPosition[2],
			desktopOrientation[0], desktopOrientation[1], desktopOrientation[2],
			desktopScale);
	}
}

QString VRRenderThread::partIdForActor(vtkActor* actor) const
{
	if (!actor) {
		return QString();
	}

	for (const TrackedActor& tracked : trackedActors) {
		if (tracked.actor == actor) {
			return tracked.partId;
		}
	}

	return QString();
}

void VRRenderThread::handleControllerColorAction(vtkEventData* eventData)
{
	if (!eventData || !renderer) {
		return;
	}

	vtkEventDataDevice3D* controllerEvent = eventData->GetAsEventDataDevice3D();
	if (!controllerEvent || controllerEvent->GetAction() != vtkEventDataAction::Press) {
		return;
	}

	double position[3];
	double orientation[4];
	controllerEvent->GetWorldPosition(position);
	controllerEvent->GetWorldOrientation(orientation);

	vtkNew<vtkPropPicker> picker;
	if (!picker->Pick3DRay(position, orientation, renderer)) {
		return;
	}

	vtkActor* pickedActor = picker->GetActor();
	const QString partId = partIdForActor(pickedActor);
	if (partId.isEmpty()) {
		return;
	}

	static const int palette[][3] = {
		{ 245, 245, 245 },
		{ 59, 130, 246 },
		{ 239, 68, 68 },
		{ 34, 197, 94 },
		{ 245, 158, 11 },
		{ 168, 85, 247 },
		{ 20, 184, 166 }
	};
	static int paletteIndex = 0;
	paletteIndex = (paletteIndex + 1) % (sizeof(palette) / sizeof(palette[0]));

	const int r = palette[paletteIndex][0];
	const int g = palette[paletteIndex][1];
	const int b = palette[paletteIndex][2];
	pickedActor->GetProperty()->SetColor(r / 255.0, g / 255.0, b / 255.0);
	pickedActor->Modified();
	emit actorColorChanged(partId, r, g, b);
}
