cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreCameraGuideStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Gui)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(CameraGuideModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Geometry/CameraGuide.ixx"
    LINK_LIBS Qt6::Gui
)

artifact_add_standalone_gtest(ArtifactCoreCameraGuideStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../CameraGuideStandaloneTest.cpp"
    LINK_LIBS CameraGuideModuleInterfaces Qt6::Gui
    LABELS "core;geometry;camera-guide;cpu;contract;standalone"
)

enable_testing()
