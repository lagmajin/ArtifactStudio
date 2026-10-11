cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreInterpolatorFactoryStandalone LANGUAGES CXX)

find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(InterpolatorFactoryModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Math/InterpolatorFactory.ixx"
    LINK_LIBS Qt6::Core Qt6::Gui
)

artifact_add_standalone_gtest(ArtifactCoreInterpolatorFactoryStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../InterpolatorFactoryStandaloneTest.cpp"
    LINK_LIBS InterpolatorFactoryModuleInterfaces Qt6::Core Qt6::Gui
    LABELS "core;math;interpolation;cpu;contract;standalone"
)

enable_testing()
