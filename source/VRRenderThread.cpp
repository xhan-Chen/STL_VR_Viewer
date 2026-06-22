/**		@file VRRenderThread.cpp
 *
 *		EEEE2046 - Software Engineering & VR Project
 *
 *		Template to add VR rendering to your application.
 *
 *		P Evans 2022
 */

#include "VRRenderThread.h"


 /* Vtk headers */
#include <vtkActor.h>
#include <vtkOpenVRRenderWindow.h>				
#include <vtkOpenVRRenderWindowInteractor.h>	
#include <vtkOpenVRRenderer.h>					
#include <vtkOpenVRCamera.h>	
#include <vtkLight.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkNamedColors.h>
#include <vtkCylinderSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkSTLReader.h>
#include <vtkDataSetmapper.h>
#include <vtkCallbackCommand.h>


/* The class constructor is called by MainWindow and runs in the primary program thread, this thread
 * will go on to handle the GUI (mouse clicks, etc). The OpenVRRenderWindowInteractor cannot be start()ed
 * in the constructor, as it will take control of the main thread to handle the VR interaction (headset
 * rotation etc. This means that a second thread is needed to handle the VR.
 */
VRRenderThread::VRRenderThread(QObject* parent) {
	/* Initialise actor list */
	actors = vtkActorCollection::New();

	/* Initialise command variables */
	rotateX = 0.;
	rotateY = 0.;
	rotateZ = 0.;
}


/* Standard destructor - this is important here as the class will be destroyed when the user
 * stops the VR thread, and recreated when the user starts it again. If class variables are
 * not deallocated properly then there will be a memory leak, where the program's total memory
 * usage will increase for each start/stop thread cycle.
 */
VRRenderThread::~VRRenderThread() {

}


void VRRenderThread::addActorOffline(vtkActor* actor) {

	/* Check to see if render thread is running */
	if (!this->isRunning()) {
		double* ac = actor->GetOrigin();

		/* I have found that these initial transforms will position the FS
		 * car model in a sensible position but you can experiment
		 */
		actor->RotateX(-90);
		actor->AddPosition(-ac[0] + 0, -ac[1] - 100, -ac[2] - 200);

		actors->AddItem(actor);
	}
}



void VRRenderThread::issueCommand(int cmd, double value) {

	/* Update class variables according to command */
	switch (cmd) {
		/* These are just a few basic examples */
	case END_RENDER:
		this->endRender = true;
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
	}
}

/* This function runs in a separate thread. This means that the program
 * can fork into two separate execution paths. This thread is triggered by
 * calling VRRenderThread::start()
 */
void VRRenderThread::run() {
	vtkNew<vtkNamedColors> colors;

	// Set the background color.
	std::array<unsigned char, 4> bkg{ {26, 51, 102, 255} };
	colors->SetColor("BkgColor", bkg.data());

	renderer = vtkOpenVRRenderer::New();
	renderer->SetBackground(colors->GetColor3d("BkgColor").GetData());

	/* Loop through list of actors provided and add to scene */
	vtkActor* a;
	actors->InitTraversal();
	while ((a = (vtkActor*)actors->GetNextActor())) {
		renderer->AddActor(a);
	}

	vtkSmartPointer<vtkLight> mainLight = vtkSmartPointer<vtkLight>::New();
	mainLight->SetPosition(100, 100, 100);
	mainLight->SetFocalPoint(0, 0, 0);
	mainLight->SetIntensity(1.0);
	renderer->AddLight(mainLight);

	// Add Fill Light
	vtkSmartPointer<vtkLight> fillLight = vtkSmartPointer<vtkLight>::New();
	// Positioned opposite to the main light
	fillLight->SetPosition(-100, -100, -100);
	fillLight->SetFocalPoint(0, 0, 0);
	// Lower intensity to illuminate dark areas
	fillLight->SetIntensity(0.4);
	renderer->AddLight(fillLight);

	window = vtkOpenVRRenderWindow::New();
	window->Initialize();
	window->AddRenderer(renderer);

	camera = vtkOpenVRCamera::New();
	renderer->SetActiveCamera(camera);

	interactor = vtkOpenVRRenderWindowInteractor::New();
	interactor->SetRenderWindow(window);
	interactor->Initialize();
	window->Render();

	endRender = false;
	t_last = std::chrono::steady_clock::now();

	/* Main render loop */
	while (!interactor->GetDone() && !this->endRender) {
		interactor->DoOneEvent(window, renderer);

		if (std::chrono::duration_cast <std::chrono::milliseconds> (std::chrono::steady_clock::now() - t_last).count() > 20) {

			vtkActorCollection* actorList = renderer->GetActors();
			vtkActor* act;

			/* X Rotation */
			actorList->InitTraversal();
			while ((act = (vtkActor*)actorList->GetNextActor())) {
				act->RotateX(rotateX);
			}

			/* Y Rotation */
			actorList->InitTraversal();
			while ((act = (vtkActor*)actorList->GetNextActor())) {
				act->RotateY(rotateY);
			}

			/* Z Rotation */
			actorList->InitTraversal();
			while ((act = (vtkActor*)actorList->GetNextActor())) {
				act->RotateZ(rotateZ);
			}

			t_last = std::chrono::steady_clock::now();
		}
	}
}