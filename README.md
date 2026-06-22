# Clone

Clone is a Qt, VTK, and OpenVR desktop application for loading STL assemblies, editing model appearance and transforms, and viewing the scene in VR.

## Project Layout

```text
.
├── .github/workflows/      # Doxygen deployment workflow
├── src/                    # C++/Qt source, UI files, OpenVR bindings, and Doxygen config
└── README.md
```

## Documentation

The Doxygen configuration lives in `src/Doxyfile`.

Run locally:

```sh
cd src
doxygen Doxyfile
```

The generated HTML output is written to `src/html/` and is ignored by git. GitHub Actions publishes the generated documentation to the `gh-pages` branch.

## Build Notes

The application is built with CMake and requires Qt 6, VTK, and OpenVR. The current CMake configuration expects OpenVR to be available at `C:/OpenVR/openvr-master` on Windows.
