cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreNamedVectorStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(NamedVectorModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
)

artifact_add_standalone_module_implementation(NamedVectorModuleImplementations
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
)
target_link_libraries(NamedVectorModuleInterfaces PUBLIC NamedVectorModuleImplementations)

artifact_add_standalone_gtest(ArtifactCoreNamedVectorStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../NamedVectorStandaloneTest.cpp"
    LINK_LIBS NamedVectorModuleInterfaces
    LABELS "container;named-vector;operations;contract;standalone"
)

enable_testing()
