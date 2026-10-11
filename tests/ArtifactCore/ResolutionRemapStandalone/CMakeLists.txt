cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreResolutionRemapStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Gui)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ResolutionRemapModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Geometry/ResolutionRemap.ixx"
    LINK_LIBS Qt6::Gui
)

artifact_add_standalone_module_implementation(ResolutionRemapUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Geometry/ResolutionRemap.cppm"
    LINK_LIBS ResolutionRemapModuleInterfaces Qt6::Gui
)

if(MSVC)
    add_dependencies(ResolutionRemapUnderTest ResolutionRemapModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreResolutionRemapStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ResolutionRemapStandaloneTest.cpp"
    LINK_LIBS ResolutionRemapUnderTest ResolutionRemapModuleInterfaces Qt6::Gui
    LABELS "core;geometry;resolution-remap;cpu;contract;standalone"
)

enable_testing()
