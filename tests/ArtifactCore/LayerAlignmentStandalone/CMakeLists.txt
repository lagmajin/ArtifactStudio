cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreLayerAlignmentStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(LayerAlignmentModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Geometry/LayerBoundsCalculator.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Geometry/LayerAlignment.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(LayerAlignmentUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Geometry/LayerBoundsCalculator.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Geometry/LayerAlignment.cppm"
    LINK_LIBS LayerAlignmentModuleInterfaces Qt6::Core
)

if(MSVC)
    add_dependencies(LayerAlignmentUnderTest LayerAlignmentModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreLayerAlignmentStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../LayerAlignmentStandaloneTest.cpp"
    LINK_LIBS LayerAlignmentUnderTest LayerAlignmentModuleInterfaces Qt6::Core
    LABELS "core;geometry;layer-alignment;cpu;contract;standalone"
)

enable_testing()
