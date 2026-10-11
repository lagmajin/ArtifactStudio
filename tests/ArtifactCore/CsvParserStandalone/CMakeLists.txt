cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreCsvParserStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(CsvParserModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Data/CsvParser.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreCsvParserStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../CsvParserStandaloneTest.cpp"
    LINK_LIBS CsvParserModuleInterfaces
    LABELS "core;data;csv-parser;cpu;contract;standalone"
)

enable_testing()
