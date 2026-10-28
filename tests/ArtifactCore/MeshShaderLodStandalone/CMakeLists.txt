cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreMeshShaderLodStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui)
find_package(meshoptimizer CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(MeshShaderLodModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Mesh/Mesh.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Memory/SharedPtr.cppm"
)
target_link_libraries(MeshShaderLodModuleInterfaces PUBLIC Qt6::Core Qt6::Gui)

artifact_add_standalone_module_implementation(MeshShaderLodImplementation
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Mesh/Mesh.cpp"
    LINK_LIBS MeshShaderLodModuleInterfaces Qt6::Core Qt6::Gui meshoptimizer::meshoptimizer
)

artifact_add_standalone_gtest(ArtifactCoreMeshShaderLodStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../MeshShaderLodStandaloneTest.cpp"
    LINK_LIBS MeshShaderLodImplementation Qt6::Core Qt6::Gui
    LABELS "core;mesh-shader;meshlet;lod;cpu;contract;standalone"
)

enable_testing()
