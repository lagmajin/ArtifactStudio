cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreFrameRateStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

artifact_add_standalone_module_interface(FrameRateModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Frame/FrameRate.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/StringConvertor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/UniString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Serialization/ISerializable.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Serialization/SerializationRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Serialization/JsonSerializableAdapter.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Serialization/SchemaMigration.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(FrameRateUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Utils/UniString.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Frame/FrameRate.cppm"
    LINK_LIBS FrameRateModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(FrameRateUnderTest FrameRateModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreFrameRateContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../FrameRateContractTest.cpp"
    LINK_LIBS FrameRateUnderTest FrameRateModuleInterfaces Qt6::Core
    LABELS "time;frame-rate;cpu;contract;standalone"
)

enable_testing()
