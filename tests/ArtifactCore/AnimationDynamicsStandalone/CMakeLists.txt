cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAnimationDynamicsStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AnimationDynamicsModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_CORE_ROOT}/src/Core"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Animation/AnimationDynamics.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cpp"
)

artifact_add_standalone_gtest(ArtifactCoreAnimationDynamicsStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AnimationDynamicsStandaloneTest.cpp"
    LINK_LIBS AnimationDynamicsModuleInterfaces
    LABELS "core;animation;dynamics;spring;cpu;contract;standalone"
)

enable_testing()
