cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreMathVecStandalone LANGUAGES CXX)

find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui)
find_package(glm CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(MathVecModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Math/Vec.ixx"
    LINK_LIBS Qt6::Core Qt6::Gui glm::glm
)

artifact_add_standalone_gtest(ArtifactCoreMathVecStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../MathVecStandaloneTest.cpp"
    LINK_LIBS MathVecModuleInterfaces Qt6::Core Qt6::Gui glm::glm
    LABELS "core;math;coordinates;glm;cpu;contract;standalone"
)

enable_testing()
