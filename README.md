2026ECE Group 6 Repository (Clone)



Members:

\- Melvin Praneethan
\- Xiaohan Chen
\- Zhen Tang
\- Ahmad Bokanan


# VR Model Viewer - STL Assembly Studio

##  Project Introduction
**VR Model Viewer - STL Assembly Studio** is an advanced software tailored for engineering design, model review, and 3D visualization (originating from the EEEE2046 Software Engineering & VR Project). This project is developed in **C++**, deeply integrating **Qt** (for building a modern, responsive user interface), **VTK (Visualization Toolkit)** (for high-performance 3D graphics rendering), and **OpenVR** (providing an immersive virtual reality experience).

The software allows engineers and designers to easily import complex STL assembly models, perform precise spatial transformations and material adjustments on the desktop, and seamlessly switch to a VR environment with a single click to directly interact with model parts in a 3D physical space.

---

## Core Features

### Model & Scene Management
*   **Multi-mode Loading:** Supports importing single `.stl` model files or directly importing complex STL folder assemblies containing hundreds of parts (with multi-threaded asynchronous loading and progress bar display).
*   **Hierarchical Tree View:** The left panel provides intuitive part tree management, supporting quick renaming, multi-selection, drag-and-drop grouping, and fuzzy search based on part names.
*   **Visibility Filtering:** One-click hide/show for specific parts, featuring a "Visible Only" filter to keep the rendering workspace clean.
*   **Part Grouping & Locking:** Allows combining multiple discrete parts into "Locked Groups". Once grouped, the color and scale of the parts within the group can be adjusted uniformly.

### Desktop 3D Rendering & Editing
*   **Environment & Lighting (IBL & HDR):** Built-in advanced Image-Based Lighting. Supports importing custom panoramic `.hdr` files as skybox environments, allowing precise adjustment of the environment's Tilt and Heading to match the ground reflections of vehicles or models.
*   **Spatial Transform Controls:** Provides a precise control panel to modify the X/Y/Z coordinates, rotation angles, and overall Scale of selected parts with numerical precision.
*   **Undo/Redo Stack:** All transformation and deletion operations are fully supported, allowing you to revert history at any time to prevent accidental operations.
*   **Smart Interaction:** Supports mouse hover picking and click-to-select. The bottom status bar displays real-time geometric data of the selected model (triangle count, included parts, etc.).

### OpenVR Experience
*   **One-Click VR Mode:** Once the desktop scene is configured, simply click "Start VR" to push the entire scene to a SteamVR/OpenVR-based virtual reality headset.
*   **Adaptive Spatial Tuning:** A dedicated VR control panel allows customization of VR view modes (Sitting/Standing), reset surround angles (Front/Side/Rear), and the default distance, height offset, and world scale (Model Size M) relative to the car model.
*   **Two-Way Data Synchronization:** Features an independent background rendering thread (`VRRenderThread`). Actions performed via controllers in the VR headset, such as grabbing/moving parts or changing colors via the palette, are synchronized back to the desktop property tree and 3D view in real-time.
*   **Showcase Spin Mode:** When enabled, the model slowly and automatically rotates in the VR space, making it perfect for client presentations and design reviews.

### Performance & Utilities
*   **Fast Mode Optimization:** Designed for massive assemblies containing millions of triangles. Enabling performance mode automatically and dynamically lowers the anti-aliasing level and interaction update rate to ensure smooth, stutter-free software operation.
*   **Helper Grid & Focus View:** One-click toggle to show/hide the floor reference grid; supports a "Focus View" mode that collapses the side panels to maximize the central 3D rendering area.
