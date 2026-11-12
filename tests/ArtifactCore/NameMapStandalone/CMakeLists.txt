cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreNameMapStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(NameMapModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NameMap.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactHashMap.cppm"
)

artifact_add_standalone_module_implementation(NameMapModuleImplementations
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
)
target_link_libraries(NameMapModuleInterfaces PUBLIC NameMapModuleImplementations)

artifact_add_standalone_gtest(ArtifactCoreNameMapStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../NameMapStandaloneTest.cpp"
    LINK_LIBS NameMapModuleInterfaces
    LABELS "container;name-map;lookup;conversion;standalone"
)

enable_testing()
