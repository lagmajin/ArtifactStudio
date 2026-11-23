cmake_minimum_required(VERSION 3.28)
project(BoneVisualizerLab LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)

# ufbx FBX parser
set(UFBX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/ufbx")

add_executable(BoneVisualizerLab
    main.cpp
    BoneVisualizerWindow.h
    BoneVisualizerWindow.cpp
    BoneViewportWidget.h
    BoneViewportWidget.cpp
    BoneGeometryBuilder.h
    BoneGeometryBuilder.cpp
    FbxSkeletonLoader.h
    FbxSkeletonLoader.cpp
    "${UFBX_DIR}/ufbx.c"
)

target_include_directories(BoneVisualizerLab PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${UFBX_DIR}"
)

target_link_libraries(BoneVisualizerLab PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

if(MSVC)
    target_compile_options(BoneVisualizerLab PRIVATE /W4 /permissive-)
endif()
