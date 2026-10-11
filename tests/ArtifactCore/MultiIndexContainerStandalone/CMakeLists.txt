cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreMultiIndexContainerStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(MultiIndexContainerModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Container/MultiIndexContainer.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreMultiIndexContainerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../MultiIndexContainerStandaloneTest.cpp"
    LINK_LIBS MultiIndexContainerModuleInterface Qt6::Core
    LABELS "container;multi-index;lookup;lifecycle;standalone"
)

enable_testing()
