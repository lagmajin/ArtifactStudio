cmake_minimum_required(VERSION 3.30)

project(ArtifactCompositionNodesStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../../ArtifactCore/StandaloneTestHelpers.cmake")

set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

artifact_add_standalone_module_interface(CompositionNodesModuleInterfaces
    BASE_DIRS "${ARTIFACT_ROOT}/Artifact/include"
    MODULES "${ARTIFACT_ROOT}/Artifact/include/Composition/ArtifactCompositionNodes.ixx"
    INCLUDE_DIRS "${ARTIFACT_ROOT}/Artifact/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(CompositionNodesUnderTest
    SOURCES "${ARTIFACT_ROOT}/Artifact/src/Composition/ArtifactCompositionNodes.cppm"
    LINK_LIBS CompositionNodesModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_ROOT}/Artifact/include"
)

if(MSVC)
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/standalone_implementations/CompositionNodesUnderTest_ArtifactCompositionNodes.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Artifact.Composition.Nodes=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CompositionNodesModuleInterfaces.dir/Artifact.Composition.Nodes.ifc")
    add_dependencies(CompositionNodesUnderTest CompositionNodesModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCompositionNodeStoreContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../CompositionNodeStoreContractTest.cpp"
    LINK_LIBS CompositionNodesUnderTest CompositionNodesModuleInterfaces Qt6::Core
    LABELS "composition;container;cpu;contract;standalone"
)
set_property(TEST ArtifactCompositionNodeStoreContractTest APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

enable_testing()
