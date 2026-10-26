cmake_minimum_required(VERSION 3.30)

project(ArtifactCorePyroVolumeStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(TBB REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(PyroVolumeModuleInterfaces
    BASE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_CORE_ROOT}/src"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Geometry/Float3.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Common/Parallel.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Simulation/PyroSimulation.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/Vector3D.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/Ray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/Camera.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/ImageBuffer.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/VolumeRenderer.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_module_implementation(PyroVolumeUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Core/Parallel.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Simulation/PyroSimulation.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Render/VolumeRenderer.cpp"
    LINK_LIBS PyroVolumeModuleInterfaces TBB::tbb
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCorePyroVolumeSmokeTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/PyroVolumeSmokeTest.cpp"
    LINK_LIBS PyroVolumeUnderTest PyroVolumeModuleInterfaces TBB::tbb
    LABELS "core;pyro;volume;smoke;standalone"
)

enable_testing()
