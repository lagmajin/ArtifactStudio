cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreFileTypeDetectorStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(FileTypeDetectorModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/File/FileTypeDetector.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(FileTypeDetectorUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/File/FileTypeDetector.cppm"
    LINK_LIBS FileTypeDetectorModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(FileTypeDetectorUnderTest FileTypeDetectorModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreFileTypeDetectorContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../FileTypeDetectorContractTest.cpp"
    LINK_LIBS FileTypeDetectorUnderTest FileTypeDetectorModuleInterfaces Qt6::Core
    LABELS "core;file-type;cpu;contract;standalone"
)

enable_testing()
