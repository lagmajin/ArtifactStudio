cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreNameGeneratorStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(NameGeneratorModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactHashMap.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NameMap.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/NameGenerator.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_module_implementation(NameGeneratorUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Utils/NameGenerator.cppm"
    LINK_LIBS NameGeneratorModuleInterfaces
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(NameGeneratorUnderTest NameGeneratorModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreNameGeneratorContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../NameGeneratorContractTest.cpp"
    LINK_LIBS NameGeneratorUnderTest NameGeneratorModuleInterfaces
    LABELS "core;name-generator;cpu;contract;standalone"
)

enable_testing()
