cmake_minimum_required(VERSION 3.21)
project(STL_VR_Viewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================
# Qt
# =========================
find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets Gui OpenGL OpenGLWidgets Svg Network
)

qt_standard_project_setup()

# =========================
# VTK
# =========================
find_package(VTK REQUIRED COMPONENTS
    CommonCore CommonColor CommonDataModel
    FiltersSources FiltersGeneral
    InteractionStyle InteractionWidgets
    RenderingCore RenderingOpenGL2 RenderingFreeType
    RenderingAnnotation RenderingContextOpenGL2
    GUISupportQt
    RenderingVR RenderingOpenVR
    IOGeometry
)

# =========================
# OpenVR 
# =========================
set(OPENVR_ROOT "C:/OpenVR/openvr-master")

set(OPENVR_INCLUDE_DIR "${OPENVR_ROOT}/headers")
set(OPENVR_LIB "${OPENVR_ROOT}/lib/win64/openvr_api.lib")
set(OPENVR_DLL "${OPENVR_ROOT}/bin/win64/openvr_api.dll")

# =========================
# Executable
# =========================
qt_add_executable(STL_VR_Viewer
    WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    ModelPart.cpp
    ModelPart.h
    ModelPartList.cpp
    ModelPartList.h
    dialog.cpp
    dialog.h
    dialog.ui
    VRRenderThread.cpp
    VRRenderThread.h
    icons.qrc
)

target_include_directories(STL_VR_Viewer PRIVATE
    ${OPENVR_INCLUDE_DIR}
)

target_link_libraries(STL_VR_Viewer PRIVATE
    Qt6::Core Qt6::Widgets Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Svg Qt6::Network
    ${VTK_LIBRARIES}
    ${OPENVR_LIB}
)

vtk_module_autoinit(
    TARGETS STL_VR_Viewer
    MODULES ${VTK_LIBRARIES}
)

# =========================
#  Qt plugin deployment 
# =========================
add_custom_command(TARGET STL_VR_Viewer POST_BUILD
    COMMAND Qt6::windeployqt
            --release
            --no-translations
            --no-system-d3d-compiler
            "$<TARGET_FILE:STL_VR_Viewer>"
)

# =========================
# OpenVR DLL copy
# =========================
add_custom_command(TARGET STL_VR_Viewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENVR_DLL}"
        "$<TARGET_FILE_DIR:STL_VR_Viewer>"
)

# =========================
# VTK runtime
# =========================
if(WIN32)
    add_custom_command(TARGET STL_VR_Viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:STL_VR_Viewer>
            $<TARGET_FILE_DIR:STL_VR_Viewer>
        COMMAND_EXPAND_LISTS
    )
endif()

# =========================
# Install layout 
# =========================
include(GNUInstallDirs)

# Install the main executable
install(TARGETS STL_VR_Viewer
    RUNTIME DESTINATION .
)

# Install Qt plugins and support folders
foreach(_dir iconengines imageformats networkinformation platforms styles tls generic)
    install(DIRECTORY "$<TARGET_FILE_DIR:STL_VR_Viewer>/${_dir}"
        DESTINATION .
        OPTIONAL
    )
endforeach()

# Install OpenVR DLL
install(FILES "${OPENVR_DLL}"
    DESTINATION .
)

# Install VTK runtime DLLs
install(FILES $<TARGET_RUNTIME_DLLS:STL_VR_Viewer>
    DESTINATION .
)

# Install HDR environment maps 
file(GLOB HDR_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.hdr")
if(HDR_FILES)
    install(FILES ${HDR_FILES} DESTINATION .)
endif()

# Install VTK OpenVR JSON bindings 
# These are often expected in a 'vrbindings' subfolder or root
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vrbindings")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/vrbindings" DESTINATION .)
endif()

# Install STL test models from F:/Models/Level0
# This assumes the build machine has access to this path
install(DIRECTORY "F:/Models/Level0/"
    DESTINATION "models"
    OPTIONAL
    FILES_MATCHING PATTERN "*.stl"
)

# =========================
# CPack NSIS 
# =========================
set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_NAME "STL_VR_Viewer")
set(CPACK_PACKAGE_VENDOR "Xiaohan Chen")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "STL_VR_Viewer")

# NSIS Specifics
set(CPACK_NSIS_DISPLAY_NAME "STL VR Viewer")
set(CPACK_NSIS_PACKAGE_NAME "STL VR Viewer")
# set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/icons/app.ico") # Missing file
set(CPACK_NSIS_INSTALLED_ICON_NAME "STL_VR_Viewer.exe")
set(CPACK_NSIS_HELP_LINK "https://github.com/xhan-Chen/STL_VR_Viewer")
set(CPACK_NSIS_MODIFY_PATH ON)

include(CPack)
