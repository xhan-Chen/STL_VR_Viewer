# VR Model Viewer Documentation

@mainpage VR Model Viewer

## Overview

VR Model Viewer is a Qt, VTK, and OpenVR application for loading large STL
assemblies, inspecting individual parts, editing appearance and transforms, and
viewing the model in an HTC Vive-compatible VR environment.

The project is organised around four main areas:

- `MainWindow` coordinates the Qt interface, desktop VTK renderer, model loading,
  HDR environments, undo/redo, grouping, and VR start/stop controls.
- `ModelPart` stores one tree item, its STL geometry, VTK actor, filters,
  material state, transform state, and group membership.
- `ModelPartList` adapts the ModelPart hierarchy to Qt's model/view API for the
  tree view.
- `VRRenderThread` owns the OpenVR renderer and safely applies actor updates
  inside the VR thread.

## Key Features

- Bulk STL loading for large assemblies such as a multi-part car model.
- Per-part colour, visibility, glow, transform, shrink, and clip controls.
- Grouping so several parts can be moved together.
- Desktop hover/click picking with tree-view synchronisation.
- HDR environment selection and user HDR import.
- Floor grid, camera fitting, tree scaling, and performance mode.
- VR rendering with reset view, standing/sitting mode, controller movement, and
  VR-to-desktop transform synchronisation.

## Documentation Workflow

This documentation follows Worksheet 5's `gh-pages` workflow:

1. Doxygen reads this project from the repository root.
2. HTML is generated into the `html/` folder.
3. GitHub Actions publishes `html/` to the `gh-pages` branch.
4. GitHub Pages serves the documentation from that `gh-pages` branch.

Private members are intentionally included so the assessor can see how the UI,
rendering, and VR state are stored. The generated site uses `doxygen/custom.css`
for a dark theme that matches the application.
