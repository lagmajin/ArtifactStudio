cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreFrameSkipTrackerStandalone LANGUAGES CXX)

find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(FrameSkipTrackerModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Frame/FrameSkipTracker.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(FrameSkipTrackerUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Frame/FrameSkipTracker.cppm"
    LINK_LIBS FrameSkipTrackerModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(FrameSkipTrackerUnderTest FrameSkipTrackerModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreFrameSkipTrackerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../FrameSkipTrackerStandaloneTest.cpp"
    LINK_LIBS FrameSkipTrackerUnderTest FrameSkipTrackerModuleInterfaces Qt6::Core
    LABELS "core;frame-skip-tracker;cpu;contract;standalone"
)

enable_testing()
