cmake_minimum_required(VERSION 3.30)

project(ArtifactCorePluginRegistryStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_CORE_INCLUDE "${ARTIFACT_CORE_ROOT}/include")

artifact_add_standalone_module_interface(PluginRegistryModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactUtility.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactSet.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Container/NameMap.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactHashMap.cppm"
        "${ARTIFACT_CORE_INCLUDE}/Plugin/PluginCommon.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Plugin/PluginRegistry.ixx"
)

add_library(PluginRegistryModuleImplementations STATIC
    "${ARTIFACT_CORE_ROOT}/src/Plugin/PluginRegistry.cpp")
target_link_libraries(PluginRegistryModuleImplementations PUBLIC PluginRegistryModuleInterfaces)
set_target_properties(PluginRegistryModuleImplementations PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
if(MSVC)
    target_compile_options(PluginRegistryModuleImplementations PRIVATE
        /experimental:module /Zc:__cplusplus /permissive- /EHsc
    )
endif()

artifact_add_standalone_gtest(ArtifactCorePluginRegistryStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../PluginRegistryContractTest.cpp"
    LINK_LIBS PluginRegistryModuleInterfaces PluginRegistryModuleImplementations
    LABELS "plugin;registry;lifecycle;contract;standalone"
)

enable_testing()
