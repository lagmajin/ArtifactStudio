cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreFastSettingsStoreStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(FastSettingsStoreModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/FastSettingsStore.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(FastSettingsStoreUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Core/FastSettingsStore.cppm"
    LINK_LIBS FastSettingsStoreModuleInterface Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(FastSettingsStoreUnderTest FastSettingsStoreModuleInterface)
endif()

artifact_add_standalone_gtest(ArtifactCoreFastSettingsStoreContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../FastSettingsStoreContractTest.cpp"
    LINK_LIBS FastSettingsStoreUnderTest FastSettingsStoreModuleInterface Qt6::Core
    LABELS "core;settings;persistence;cpu;contract;standalone"
)

enable_testing()
