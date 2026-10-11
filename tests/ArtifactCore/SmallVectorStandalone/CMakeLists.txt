cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreSmallVectorStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(SmallVectorModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/SmallVector.ixx"
)

artifact_add_standalone_module_implementation(SmallVectorModuleImplementations
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
)
target_link_libraries(SmallVectorModuleInterfaces PUBLIC SmallVectorModuleImplementations)

artifact_add_standalone_gtest(ArtifactCoreSmallVectorStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../SmallVectorStandaloneTest.cpp"
    LINK_LIBS SmallVectorModuleInterfaces
    LABELS "container;small-vector;storage;ownership;standalone"
)

enable_testing()
