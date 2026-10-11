cmake_minimum_required(VERSION 3.30)
project(ArtifactCoreFallbackPolicyStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)
include(../StandaloneTestHelpers.cmake)

set(artifact_core_root "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(artifact_core_include "${artifact_core_root}/include")

artifact_add_standalone_module_interface(FallbackPolicyModuleInterfaces
    BASE_DIRS "${artifact_core_root}"
    INCLUDE_DIRS "${artifact_core_include}"
    MODULES
        "${artifact_core_include}/Container/ContainerDebug.ixx"
        "${artifact_core_include}/Container/ContainerDebugRegistry.ixx"
        "${artifact_core_root}/src/Core/ArtifactOptional.cppm"
        "${artifact_core_include}/Core/ArtifactArray.ixx"
        "${artifact_core_include}/Core/ArtifactString.ixx"
        "${artifact_core_include}/Container/NamedVector.ixx"
        "${artifact_core_include}/Utils/Result.ixx"
        "${artifact_core_include}/Utils/Optional.ixx"
        "${artifact_core_include}/Diagnostics/FallbackPolicy.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(FallbackPolicyUnderTest
    SOURCES "${artifact_core_root}/src/Diagnostics/FallbackPolicy.cppm"
    LINK_LIBS FallbackPolicyModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${artifact_core_include}"
)

if(MSVC)
    add_dependencies(FallbackPolicyUnderTest FallbackPolicyModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreFallbackPolicyStandaloneTest
    SOURCES ../FallbackPolicyStandaloneTest.cpp
    LINK_LIBS FallbackPolicyUnderTest FallbackPolicyModuleInterfaces Qt6::Core
    LABELS "core;diagnostics;fallback-policy;cpu;contract;standalone"
)

enable_testing()
