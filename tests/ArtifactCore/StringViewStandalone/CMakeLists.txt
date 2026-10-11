cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreStringViewStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(StringViewModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Utils/StringView.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreStringViewContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../StringViewContractTest.cpp"
    LINK_LIBS StringViewModuleInterface Qt6::Core
    LABELS "core;string;view;cpu;contract;standalone"
)

enable_testing()
