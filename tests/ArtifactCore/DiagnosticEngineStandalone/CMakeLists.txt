cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreDiagnosticEngineStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

if(NOT TARGET ArtifactCoreStandaloneFoundationModules)
    artifact_add_standalone_module_interface(ArtifactCoreStandaloneFoundationModules
        BASE_DIRS "${ARTIFACT_CORE_ROOT}"
        MODULES
            "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
            "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
            "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
            "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
            "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    )
endif()

artifact_add_standalone_module_interface(DiagnosticEngineModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Diagnostics/ProjectDiagnostic.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Diagnostics/DiagnosticEngine.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules Qt6::Core
)

artifact_add_standalone_module_implementation(DiagnosticEngineUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Diagnostics/ProjectDiagnostic.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Diagnostics/DiagnosticEngine.cpp"
    LINK_LIBS DiagnosticEngineModuleInterfaces ArtifactCoreStandaloneFoundationModules Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(DiagnosticEngineUnderTest DiagnosticEngineModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreDiagnosticEngineStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DiagnosticEngineStandaloneTest.cpp"
    LINK_LIBS DiagnosticEngineUnderTest DiagnosticEngineModuleInterfaces ArtifactCoreStandaloneFoundationModules Qt6::Core
    LABELS "diagnostics;validation;cache;contract;standalone"
)

if(ARTIFACT_STANDALONE_GTEST_GROUP)
    artifact_finalize_standalone_gtest_group("${ARTIFACT_STANDALONE_GTEST_GROUP}")
endif()

enable_testing()
