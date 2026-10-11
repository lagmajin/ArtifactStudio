cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreLayerBoundsCalculatorStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(LayerBoundsCalculatorModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Geometry/LayerBoundsCalculator.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(LayerBoundsCalculatorUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Geometry/LayerBoundsCalculator.cppm"
    LINK_LIBS LayerBoundsCalculatorModuleInterfaces Qt6::Core
)

if(MSVC)
    add_dependencies(LayerBoundsCalculatorUnderTest LayerBoundsCalculatorModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreLayerBoundsCalculatorStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../LayerBoundsCalculatorStandaloneTest.cpp"
    LINK_LIBS LayerBoundsCalculatorUnderTest LayerBoundsCalculatorModuleInterfaces Qt6::Core
    LABELS "core;geometry;layer-bounds;cpu;contract;standalone"
)

enable_testing()
