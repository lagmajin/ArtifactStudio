cmake_minimum_required(VERSION 3.30)

project(ArtifactCorePropertyPathStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(PropertyPathModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Property/PropertyPath.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCorePropertyPathContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../PropertyPathTest.cpp"
    LINK_LIBS PropertyPathModuleInterfaces Qt6::Core
    LABELS "property-path;cpu;contract;standalone"
)
set_property(TEST ArtifactCorePropertyPathContractTest APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

enable_testing()
