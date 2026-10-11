cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreBezierPathSamplerStandalone LANGUAGES CXX)

find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(BezierPathSamplerModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Geometry/BezierCalculator.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Geometry/BezierPathSampler.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(BezierPathSamplerUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Geometry/BezierCalculator.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Geometry/BezierPathSampler.cppm"
    LINK_LIBS BezierPathSamplerModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(BezierPathSamplerUnderTest BezierPathSamplerModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreBezierPathSamplerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../BezierPathSamplerStandaloneTest.cpp"
    LINK_LIBS BezierPathSamplerUnderTest BezierPathSamplerModuleInterfaces Qt6::Core
    LABELS "core;geometry;bezier;sampling;cpu;contract;standalone"
)

enable_testing()
