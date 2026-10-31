cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreGeometryStandaloneSuite LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_CORE_INCLUDE "${ARTIFACT_CORE_ROOT}/include")

artifact_add_standalone_module_interface(ArtifactCoreGeometryFeatureModules
    BASE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    MODULES
        "${ARTIFACT_CORE_INCLUDE}/Geometry/RotationTurns.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Geometry/ResolutionRemap.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Geometry/BezierCalculator.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Geometry/BezierPathSampler.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Geometry/LayerBoundsCalculator.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Geometry/LayerAlignment.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/UniformScale.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/Position2D.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Common/Size.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Math/Quaternion.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Math/Matrix4x4.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS Qt6::Core Qt6::Gui
)

artifact_add_standalone_module_implementation(ArtifactCoreGeometryImplementations
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Math/RotationTurns.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Geometry/ResolutionRemap.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Geometry/BezierCalculator.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Geometry/BezierPathSampler.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Geometry/LayerBoundsCalculator.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Geometry/LayerAlignment.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Core/UniformScale.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Core/Position2D.cpp"
    LINK_LIBS ArtifactCoreGeometryFeatureModules Qt6::Core Qt6::Gui
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
)

if(MSVC)
    add_dependencies(ArtifactCoreGeometryImplementations ArtifactCoreGeometryFeatureModules)
    set(_geometry_ifc_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreGeometryFeatureModules.dir")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Math/RotationTurns.cpp"
        APPEND PROPERTY COMPILE_OPTIONS "/reference;Math.RotationTurns=${_geometry_ifc_dir}/Math.RotationTurns.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Geometry/ResolutionRemap.cpp"
        APPEND PROPERTY COMPILE_OPTIONS "/reference;Geometry.ResolutionRemap=${_geometry_ifc_dir}/Geometry.ResolutionRemap.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Geometry/BezierCalculator.cpp"
        APPEND PROPERTY COMPILE_OPTIONS "/reference;Math.Bezier=${_geometry_ifc_dir}/Math.Bezier.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Geometry/BezierPathSampler.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Math.Bezier.Sampler=${_geometry_ifc_dir}/Math.Bezier.Sampler.ifc"
            "/reference;Math.Bezier=${_geometry_ifc_dir}/Math.Bezier.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Geometry/LayerBoundsCalculator.cpp"
        APPEND PROPERTY COMPILE_OPTIONS "/reference;Geometry.LayerBounds=${_geometry_ifc_dir}/Geometry.LayerBounds.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Geometry/LayerAlignment.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Geometry.LayerAlignment=${_geometry_ifc_dir}/Geometry.LayerAlignment.ifc"
            "/reference;Geometry.LayerBounds=${_geometry_ifc_dir}/Geometry.LayerBounds.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Core/UniformScale.cpp"
        APPEND PROPERTY COMPILE_OPTIONS "/reference;Core.UniformScale=${_geometry_ifc_dir}/Core.UniformScale.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Core/Position2D.cpp"
        APPEND PROPERTY COMPILE_OPTIONS "/reference;Core.Position2D=${_geometry_ifc_dir}/Core.Position2D.ifc")
    unset(_geometry_ifc_dir)
endif()

artifact_add_standalone_gtest(ArtifactCoreRotationTurnsStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../RotationTurnsStandaloneTest.cpp"
    LINK_LIBS ArtifactCoreGeometryImplementations ArtifactCoreGeometryFeatureModules
    LABELS "core;geometry;rotation;cpu;contract;standalone"
)
artifact_add_standalone_gtest(ArtifactCoreResolutionRemapStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ResolutionRemapStandaloneTest.cpp"
    LINK_LIBS ArtifactCoreGeometryImplementations ArtifactCoreGeometryFeatureModules Qt6::Gui
    LABELS "core;geometry;resolution-remap;cpu;contract;standalone"
)
artifact_add_standalone_gtest(ArtifactCoreBezierPathSamplerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../BezierPathSamplerStandaloneTest.cpp"
    LINK_LIBS ArtifactCoreGeometryImplementations ArtifactCoreGeometryFeatureModules Qt6::Core
    LABELS "core;geometry;bezier;sampling;cpu;contract;standalone"
)
artifact_add_standalone_gtest(ArtifactCoreLayerAlignmentStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../LayerAlignmentStandaloneTest.cpp"
    LINK_LIBS ArtifactCoreGeometryImplementations ArtifactCoreGeometryFeatureModules Qt6::Core
    LABELS "core;geometry;layer-alignment;cpu;contract;standalone"
)
artifact_add_standalone_gtest(ArtifactCoreLayerBoundsCalculatorStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../LayerBoundsCalculatorStandaloneTest.cpp"
    LINK_LIBS ArtifactCoreGeometryImplementations ArtifactCoreGeometryFeatureModules Qt6::Core
    LABELS "core;geometry;layer-bounds;cpu;contract;standalone"
)
artifact_add_standalone_gtest(ArtifactCoreUniformScaleStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../UniformScaleContractTest.cpp"
    LINK_LIBS ArtifactCoreGeometryImplementations ArtifactCoreGeometryFeatureModules
    LABELS "core;uniform-scale;cpu;contract;standalone"
)
artifact_add_standalone_gtest(ArtifactCorePosition2DContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../Position2DContractTest.cpp"
    LINK_LIBS ArtifactCoreGeometryImplementations ArtifactCoreGeometryFeatureModules Qt6::Core
    LABELS "core;geometry;position;cpu;contract;standalone"
)
artifact_add_standalone_gtest(ArtifactCoreSize2DStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../Size2DStandaloneTest.cpp"
    LINK_LIBS ArtifactCoreGeometryFeatureModules Qt6::Core
    LABELS "core;size;concepts;cpu;contract;standalone"
)
artifact_add_standalone_gtest(ArtifactCoreQuaternionStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../QuaternionStandaloneTest.cpp"
    LINK_LIBS ArtifactCoreGeometryFeatureModules
    LABELS "core;math;quaternion;cpu;contract;standalone"
)
artifact_add_standalone_gtest(ArtifactCoreMatrix4x4StandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../Matrix4x4StandaloneTest.cpp"
    LINK_LIBS ArtifactCoreGeometryFeatureModules
    LABELS "core;math;matrix;cpu;contract;standalone"
)

if(ARTIFACT_STANDALONE_GTEST_GROUP)
    artifact_finalize_standalone_gtest_group("${ARTIFACT_STANDALONE_GTEST_GROUP}")
endif()

enable_testing()
