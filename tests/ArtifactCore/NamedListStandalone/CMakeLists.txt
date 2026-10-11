cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreNamedListStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(NamedListModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedList.ixx"
)

artifact_add_standalone_module_implementation(NamedListModuleImplementations
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
)
target_link_libraries(NamedListModuleInterfaces PUBLIC NamedListModuleImplementations)

artifact_add_standalone_gtest(ArtifactCoreNamedListStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../NamedListStandaloneTest.cpp"
    LINK_LIBS NamedListModuleInterfaces
    LABELS "container;named-list;operations;contract;standalone"
)

enable_testing()
