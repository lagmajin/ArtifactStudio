cmake_minimum_required(VERSION 3.30)

project(ArtifactSmokeRuntimeStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Gui)
find_package(TBB REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../../ArtifactCore/StandaloneTestHelpers.cmake")

set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../Artifact")
set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(SmokeRuntimeModuleInterfaces
    BASE_DIRS
        "${ARTIFACT_CORE_ROOT}"
        "${ARTIFACT_ROOT}"
        "${CMAKE_CURRENT_LIST_DIR}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Common/Parallel.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Geometry/Float3.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Physics/FluidSolver2D.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Simulation/PyroSimulation.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/Vector3D.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/Ray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/Camera.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/ImageBuffer.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/VolumeRenderer.ixx"
        "${CMAKE_CURRENT_LIST_DIR}/TestParticleData.ixx"
        "${CMAKE_CURRENT_LIST_DIR}/TestArtifactIRenderer.ixx"
        "${ARTIFACT_ROOT}/include/Layer/ArtifactLayerFluidRuntimeState.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/include"
    LINK_LIBS Qt6::Gui
)

artifact_add_standalone_module_implementation(SmokeRuntimeCoreImplementation
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Physics/FluidSolver2D.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Core/Parallel.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Simulation/PyroSimulation.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Render/VolumeRenderer.cpp"
    LINK_LIBS SmokeRuntimeModuleInterfaces TBB::tbb
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_module_implementation(SmokeRuntimeImplementation
    SOURCES "${ARTIFACT_ROOT}/src/Layer/ArtifactLayerFluidRuntimeState.cpp"
    LINK_LIBS SmokeRuntimeModuleInterfaces SmokeRuntimeCoreImplementation Qt6::Gui
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include" "${ARTIFACT_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactSmokeRuntimeStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/SmokeRuntimeStandaloneTest.cpp"
    LINK_LIBS SmokeRuntimeImplementation SmokeRuntimeCoreImplementation
        SmokeRuntimeModuleInterfaces Qt6::Gui TBB::tbb
    LABELS "artifact;physics;fluid;smoke;runtime;standalone"
)

enable_testing()
