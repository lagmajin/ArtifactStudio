cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreUniformScaleStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(UniformScaleModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/UniformScale.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_module_implementation(UniformScaleUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Core/UniformScale.cppm"
    LINK_LIBS UniformScaleModuleInterface
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(UniformScaleUnderTest UniformScaleModuleInterface)
endif()

artifact_add_standalone_gtest(ArtifactCoreUniformScaleContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../UniformScaleContractTest.cpp"
    LINK_LIBS UniformScaleUnderTest UniformScaleModuleInterface
    LABELS "core;uniform-scale;cpu;contract;standalone"
)

enable_testing()
