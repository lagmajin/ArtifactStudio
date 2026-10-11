cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreNoiseGeneratorStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(NoiseGeneratorModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Math/NoiseGenerator.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(NoiseGeneratorUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Math/NoiseGenerator.cppm"
    LINK_LIBS NoiseGeneratorModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(NoiseGeneratorUnderTest NoiseGeneratorModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreNoiseGeneratorStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../NoiseGeneratorStandaloneTest.cpp"
    LINK_LIBS NoiseGeneratorUnderTest NoiseGeneratorModuleInterfaces Qt6::Core
    LABELS "core;math;noise;cpu;contract;standalone"
)

enable_testing()
