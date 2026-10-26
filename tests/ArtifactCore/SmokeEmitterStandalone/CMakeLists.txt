cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreSmokeEmitterStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Gui)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(SmokeEmitterModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Physics/FluidSolver2D.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_module_implementation(SmokeEmitterImplementation
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Physics/FluidSolver2D.cpp"
    LINK_LIBS SmokeEmitterModuleInterfaces
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreSmokeEmitterStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../SmokeEmitterStandaloneTest.cpp"
    LINK_LIBS SmokeEmitterImplementation SmokeEmitterModuleInterfaces Qt6::Gui
    LABELS "core;physics;fluid;smoke;standalone"
)

enable_testing()
