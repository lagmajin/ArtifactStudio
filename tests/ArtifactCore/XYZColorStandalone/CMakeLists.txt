cmake_minimum_required(VERSION 3.30)
project(ArtifactCoreXYZColorStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)
include(../StandaloneTestHelpers.cmake)

set(artifact_core_root "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(artifact_core_include "${artifact_core_root}/include")
set(repository_root "${CMAKE_CURRENT_LIST_DIR}/../../..")

artifact_add_standalone_module_interface(XYZColorModuleInterfaces
    BASE_DIRS "${artifact_core_root}/include"
    INCLUDE_DIRS
        "${artifact_core_include}"
        "${repository_root}/third_party/verdigris/src"
    MODULES
        "${artifact_core_include}/Color/ColorTransferFunction.ixx"
        "${artifact_core_include}/Color/FloatColor.ixx"
        "${artifact_core_include}/Utils/StringConvertor.ixx"
        "${artifact_core_include}/Utils/UniString.ixx"
        "${artifact_core_include}/Color/XYZColor.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(XYZColorUnderTest
    SOURCES
        "${artifact_core_root}/src/Color/FloatColor.cppm"
        "${artifact_core_root}/src/Utils/UniString.cppm"
        "${artifact_core_root}/src/Color/XYZColor.cppm"
    LINK_LIBS XYZColorModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${artifact_core_include}"
)

if(MSVC)
    add_dependencies(XYZColorUnderTest XYZColorModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreXYZColorStandaloneTest
    SOURCES ../XYZColorStandaloneTest.cpp
    LINK_LIBS XYZColorUnderTest XYZColorModuleInterfaces Qt6::Core
    LABELS "color;xyz-color;cpu;contract;standalone"
)

enable_testing()
