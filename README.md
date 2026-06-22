# Clone

Clone is a desktop STL assembly viewer for engineering design review and virtual reality inspection. It combines a Qt interface, VTK rendering, and OpenVR support so users can load model parts, adjust their appearance, and view the same scene in an immersive VR environment.

## Project Introduction

The application is designed for working with complex STL assemblies rather than single isolated meshes. Users can import models, inspect individual parts through a tree-based interface, modify visual properties, and switch between desktop rendering and VR presentation workflows.

The desktop view focuses on precise model management and editing. The VR mode extends the same scene into a headset-based environment for spatial review, presentation, and interaction.

## Core Features

### Model and Scene Management

- Load STL files and STL assembly folders.
- Browse model parts through a hierarchical tree view.
- Select, rename, hide, show, and delete individual parts.
- Group and manage parts for clearer scene organization.

### Desktop 3D Editing

- Render STL geometry with VTK inside a Qt application.
- Adjust part color, visibility, transform, and scale.
- Inspect model information from the main interface.
- Use a focused desktop view for model preparation before entering VR.

### VR Viewing

- Launch an OpenVR-based viewing mode from the configured desktop scene.
- Display loaded assemblies inside a VR environment.
- Support VR-oriented scene scaling and positioning.
- Use controller interaction data through the included OpenVR binding files.

### Documentation

The project includes Doxygen documentation for the main C++ classes and VR rendering components. The generated documentation is published through GitHub Pages by the repository workflow.

## Technology Stack

- C++17
- Qt 6
- VTK
- OpenVR
- CMake
- Doxygen

## Purpose

Clone was built as an engineering visualization tool for reviewing STL assemblies on both desktop and VR devices. Its main goal is to make model inspection, part-level editing, and immersive presentation available from a single application.
