cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreIdMapStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(IdMapModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/IdMap.ixx"
)

artifact_add_standalone_module_implementation(IdMapModuleImplementations
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
)
target_link_libraries(IdMapModuleInterfaces PUBLIC IdMapModuleImplementations)

artifact_add_standalone_gtest(ArtifactCoreIdMapStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../IdMapStandaloneTest.cpp"
    LINK_LIBS IdMapModuleInterfaces
    LABELS "container;id-map;operations;contract;standalone"
)

enable_testing()
