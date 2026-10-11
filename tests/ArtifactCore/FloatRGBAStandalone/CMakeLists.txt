cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreFloatRGBAStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(FloatRGBAModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorTransferFunction.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/FloatColor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/FloatRGBA.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_module_implementation(FloatRGBAUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Color/FloatColor.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Color/FloatRGBA.cppm"
    LINK_LIBS FloatRGBAModuleInterfaces
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(FloatRGBAUnderTest FloatRGBAModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreFloatRGBAContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../FloatRGBAContractTest.cpp"
    LINK_LIBS FloatRGBAUnderTest FloatRGBAModuleInterfaces
    LABELS "color;rgba;cpu;contract;standalone"
)

enable_testing()
