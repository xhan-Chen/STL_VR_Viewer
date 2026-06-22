    cmake_minimum_required(VERSION 3.19)

project(WS6 LANGUAGES CXX)
set( CMAKE_NINJA_FORCE_RESPONSE_FILE "ON" CACHE BOOL "Force Ninja to use response files." )

set(OpenVR_INCLUDE_DIR "C:/OpenVR/headers" CACHE PATH "")
set(OpenVR_LIBRARY "C:/OpenVR/lib/win64/openvr_api.lib" CACHE FILEPATH "")

# --- 1. Find dependency packages ---
# Explicitly list core modules to prevent build failures caused by missing optional hardware drivers like OpenVR
find_package(VTK REQUIRED
    COMPONENTS
        CommonCore
        IOGeometry
        FiltersSources
        InteractionStyle
        RenderingContextOpenGL2
        RenderingCore
        RenderingFreeType
        RenderingOpenGL2
        GUISupportQt
        RenderingOpenVR
        CommonColor
)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Qt standard project setup
qt_standard_project_setup()

# --- 2. Create executable target ---
qt_add_executable(WS6
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    ModelPart.h
    ModelPart.cpp
    ModelPartList.h
    ModelPartList.cpp
    icons.qrc
    dialog.h
    dialog.cpp
    dialog.ui
    VRRenderThread.cpp
    VRRenderThread.h
)
target_compile_definitions(WS6 PRIVATE QT_NO_OPENGL_HEADERS)

# --- 3. Configure header include paths ---
# Use the confirmed absolute paths to resolve the issue of vtkSmartPointer.h not being found
target_include_directories(WS6 PRIVATE
    ${VTK_INCLUDE_DIRS}
    ${OpenVR_INCLUDE_DIR}
)

# --- 4. Link libraries ---
target_link_libraries(WS6
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        ${VTK_LIBRARIES}
        ${OpenVR_LIBRARY}
)


# --- 6. Auxiliary target (Handle vrbindings folder) ---
add_custom_target(VRBindings)
add_custom_command(TARGET VRBindings PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/vrbindings"
    "${CMAKE_BINARY_DIR}/"
)
