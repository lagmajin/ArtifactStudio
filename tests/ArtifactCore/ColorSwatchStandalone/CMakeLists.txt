cmake_minimum_required(VERSION 3.30)
project(ArtifactCoreColorSwatchStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
include(../StandaloneTestHelpers.cmake)

set(artifact_core_root "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(artifact_core_include "${artifact_core_root}/include")

artifact_add_standalone_module_interface(ColorSwatchModuleInterfaces
    BASE_DIRS "${artifact_core_root}"
    INCLUDE_DIRS "${artifact_core_include}"
    MODULES
        "${artifact_core_include}/Container/ContainerDebug.ixx"
        "${artifact_core_include}/Container/ContainerDebugRegistry.ixx"
        "${artifact_core_root}/src/Core/ArtifactOptional.cppm"
        "${artifact_core_include}/Core/ArtifactArray.ixx"
        "${artifact_core_include}/Core/ArtifactString.ixx"
        "${artifact_core_include}/Container/NamedVector.ixx"
        "${artifact_core_include}/Color/ColorTransferFunction.ixx"
        "${artifact_core_include}/Color/FloatColor.ixx"
        "${artifact_core_root}/src/Memory/TrackedPtr.cppm"
        "${artifact_core_include}/Color/ColorSwatch.ixx"
)

artifact_add_standalone_module_implementation(ColorSwatchUnderTest
    SOURCES
        "${artifact_core_root}/src/Color/FloatColor.cppm"
        "${artifact_core_root}/src/Color/ColorSwatch.cppm"
    LINK_LIBS ColorSwatchModuleInterfaces
    INCLUDE_DIRS "${artifact_core_include}"
)

if(MSVC)
    add_dependencies(ColorSwatchUnderTest ColorSwatchModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreColorSwatchStandaloneTest
    SOURCES ../ColorSwatchStandaloneTest.cpp
    LINK_LIBS ColorSwatchUnderTest ColorSwatchModuleInterfaces
    LABELS "color;color-swatch;cpu;contract;standalone"
)

enable_testing()
