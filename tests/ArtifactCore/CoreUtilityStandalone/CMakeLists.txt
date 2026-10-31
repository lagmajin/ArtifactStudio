cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreCoreUtilityStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)
find_package(Threads REQUIRED)
include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_CORE_INCLUDE "${ARTIFACT_CORE_ROOT}/include")
artifact_ensure_shared_string_modules()

if(NOT TARGET ArtifactCoreStandaloneFoundationModules)
    artifact_add_standalone_module_interface(ArtifactCoreStandaloneFoundationModules
        BASE_DIRS "${ARTIFACT_CORE_ROOT}"
        MODULES
            "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
            "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
            "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
            "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
            "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
            "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
            "${ARTIFACT_CORE_ROOT}/src/Memory/SharedPtr.cppm"
        INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    )
endif()

artifact_add_standalone_module_interface(CoreUtilityFeatureModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_INCLUDE}/Utils/Result.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/Optional.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/CppLinq.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/Text/Encoding.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/Text/Number.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/Text/String.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/Text/Path.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Asset/AssetSequence.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Diagnostics/CoreDiagnosticSnapshot.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Diagnostics/CoreDiagnosticRecorder.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Diagnostics/CoreCrashReportParser.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/PerformanceProfiler.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactUtility.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactAlgorithms.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactPtr.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactRegex.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactSet.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactTuple.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactSpan.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactVariant.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactHashMap.cppm"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactFunction.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactFunctionRef.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactCallback.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactChrono.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactSaturation.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Math/ArtifactMinMax.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Container/SmallVector.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Container/ContainerAlgorithms.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Container/NameMap.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Container/IdMap.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Container/NamedList.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactMath.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Geometry/Float3.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Animation/AnimationDynamics.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/SimulationSystem.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactExpected.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Channel/DiskSize.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/Defaults.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/Numeric.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules
)

artifact_add_standalone_module_interface(CoreQtUtilityFeatureModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactTimer.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactRandom.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/Handle.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/HashValue.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/VectorLike.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/PointLike.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/NumericLike.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Time/RationalTime.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Frame/FrameSkipTracker.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Diagnostics/DebugIdentity.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Diagnostics/SessionLedger.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Math/NoiseGenerator.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Utils/MultipleTag.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules ArtifactCoreSharedStringModules Qt6::Core
)

artifact_add_standalone_module_interface(CoreQtNameGeneratorFeatureModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_INCLUDE}/Utils/NameGenerator.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS CoreUtilityFeatureModules ArtifactCoreStandaloneFoundationModules Qt6::Core
)

artifact_add_standalone_module_implementation(CoreQtUtilityImplementations
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Utils/HashValue.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Math/NoiseGenerator.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Utils/MulitpleTag.cpp"
    LINK_LIBS CoreQtUtilityFeatureModules Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
)
artifact_add_standalone_module_implementation(CoreQtNameGeneratorImplementations
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Utils/NameGenerator.cpp"
    LINK_LIBS CoreQtNameGeneratorFeatureModules CoreUtilityFeatureModules ArtifactCoreStandaloneFoundationModules Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
)
target_sources(CoreQtUtilityImplementations PRIVATE
    "${ARTIFACT_CORE_ROOT}/src/Time/RationalTime.cpp"
    "${ARTIFACT_CORE_ROOT}/src/Frame/FrameSkipTracker.cpp"
)
set_source_files_properties(
    "${ARTIFACT_CORE_ROOT}/src/Time/RationalTime.cpp"
    "${ARTIFACT_CORE_ROOT}/src/Frame/FrameSkipTracker.cpp"
    PROPERTIES CXX_SCAN_FOR_MODULES OFF
)
if(MSVC)
    add_dependencies(CoreQtUtilityImplementations CoreQtUtilityFeatureModules)
    add_dependencies(CoreQtNameGeneratorImplementations CoreQtNameGeneratorFeatureModules)
    add_dependencies(CoreQtNameGeneratorImplementations CoreUtilityFeatureModules)
    set(_core_qt_utility_ifc_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CoreQtUtilityFeatureModules.dir")
    get_target_property(_core_foundation_ifc_dir ArtifactCoreStandaloneFoundationModules ARTIFACT_STANDALONE_IFC_DIR)
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Time/RationalTime.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Time.Rational=${_core_qt_utility_ifc_dir}/Time.Rational.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Frame/FrameSkipTracker.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Frame.SkipTracker=${_core_qt_utility_ifc_dir}/Frame.SkipTracker.ifc")
    set(_core_name_generator_ifc_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CoreQtNameGeneratorFeatureModules.dir")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Utils/NameGenerator.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Utils.NameGenerator=${_core_name_generator_ifc_dir}/Utils.NameGenerator.ifc"
            "/reference;Container.NameMap=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CoreUtilityFeatureModules.dir/Container.NameMap.ifc"
            "/reference;Container.Debug=${_core_foundation_ifc_dir}/Container.Debug.ifc"
            "/reference;Core.ArtifactString=${_core_foundation_ifc_dir}/Core.ArtifactString.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Math/NoiseGenerator.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Math.Noise=${_core_qt_utility_ifc_dir}/Math.Noise.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Utils/MulitpleTag.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Utils.MultipleTag=${_core_qt_utility_ifc_dir}/Utils.MultipleTag.ifc")
    unset(_core_qt_utility_ifc_dir)
    unset(_core_name_generator_ifc_dir)
    unset(_core_foundation_ifc_dir)
endif()

artifact_add_standalone_module_interface(CoreThreadFeatureModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactThread.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules Threads::Threads
)

set(_core_utility_suites
    AssetSequence
    TextString
    CppLinq
    DiagnosticRecorder
    DiagnosticSnapshot
    Path
    TextEncoding
    TextNumber
    ArtifactAlgorithms
    Result
    UtilsOptional
    ArtifactPtr
    ArtifactRegex
    ArtifactSet
    ArtifactTuple
    ArtifactSpan
    ArtifactVariant
    ArtifactHashMap
    ArtifactFunction
    ArtifactFunctionRef
    ArtifactCallback
    ArtifactChrono
    ArtifactSaturation
    ArtifactMinMax
    SmallVector
    ContainerAlgorithms
    ArtifactMath
    Float3
    AnimationDynamics
    SimulationSystem
)
set(_core_utility_test_sources
    AssetSequenceStandaloneTest.cpp
    TextStringContractTest.cpp
    CppLinqContractTest.cpp
    DiagnosticRecorderStandaloneTest.cpp
    DiagnosticSnapshotStandaloneTest.cpp
    PathContractTest.cpp
    TextEncodingContractTest.cpp
    TextNumberContractTest.cpp
    ArtifactAlgorithmsContractTest.cpp
    ResultContractTest.cpp
    OptionalContractTest.cpp
    ArtifactPtrContractTest.cpp
    ArtifactRegexContractTest.cpp
    ArtifactSetContractTest.cpp
    ArtifactTupleContractTest.cpp
    ArtifactSpanContractTest.cpp
    ArtifactVariantContractTest.cpp
    ArtifactHashMapContractTest.cpp
    ArtifactFunctionContractTest.cpp
    ArtifactFunctionRefContractTest.cpp
    ArtifactCallbackContractTest.cpp
    ArtifactChronoContractTest.cpp
    ArtifactSaturationContractTest.cpp
    ArtifactMinMaxStandaloneTest.cpp
    SmallVectorStandaloneTest.cpp
    ContainerAlgorithmsStandaloneTest.cpp
    ArtifactMathContractTest.cpp
    Float3StandaloneTest.cpp
    AnimationDynamicsStandaloneTest.cpp
    SimulationSystemContractTest.cpp
)
foreach(_suite _test_source IN ZIP_LISTS _core_utility_suites _core_utility_test_sources)
    artifact_add_standalone_gtest(ArtifactCore${_suite}StandaloneTest
        SOURCES "${CMAKE_CURRENT_LIST_DIR}/../${_test_source}"
        LINK_LIBS CoreUtilityFeatureModules ArtifactCoreStandaloneFoundationModules
        LABELS "core;utility;contract;standalone"
    )
endforeach()

artifact_add_standalone_gtest(ArtifactCoreNamedVectorStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../NamedVectorStandaloneTest.cpp"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules
    LABELS "container;named-vector;operations;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreContainerDebugStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ContainerDebugStandaloneTest.cpp"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules
    LABELS "container;debug;metadata;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreContainerDebugRegistryStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ContainerDebugRegistryStandaloneTest.cpp"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules
    LABELS "container;debug-registry;lifecycle;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreCrashReportParserStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../CrashReportParserStandaloneTest.cpp"
    LINK_LIBS CoreUtilityFeatureModules ArtifactCoreStandaloneFoundationModules
    LABELS "diagnostics;crash-report;parser;serialization;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreNameMapStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../NameMapStandaloneTest.cpp"
    LINK_LIBS CoreUtilityFeatureModules ArtifactCoreStandaloneFoundationModules
    LABELS "container;name-map;lookup;conversion;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreIdMapStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../IdMapStandaloneTest.cpp"
    LINK_LIBS CoreUtilityFeatureModules ArtifactCoreStandaloneFoundationModules
    LABELS "container;id-map;operations;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreNamedListStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../NamedListStandaloneTest.cpp"
    LINK_LIBS CoreUtilityFeatureModules ArtifactCoreStandaloneFoundationModules
    LABELS "container;named-list;operations;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreNameGeneratorContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../NameGeneratorContractTest.cpp"
    LINK_LIBS CoreQtNameGeneratorImplementations CoreQtNameGeneratorFeatureModules CoreUtilityFeatureModules ArtifactCoreStandaloneFoundationModules
    LABELS "core;name-generator;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreNoiseGeneratorStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../NoiseGeneratorStandaloneTest.cpp"
    LINK_LIBS CoreQtUtilityImplementations CoreQtUtilityFeatureModules Qt6::Core
    LABELS "core;math;noise;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreMultipleTagContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../MultipleTagContractTest.cpp"
    LINK_LIBS CoreQtUtilityImplementations CoreQtUtilityFeatureModules Qt6::Core
    LABELS "core;tags;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreDefaultsContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DefaultsContractTest.cpp"
    LINK_LIBS CoreUtilityFeatureModules ArtifactCoreStandaloneFoundationModules
    LABELS "core;defaults;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreNumericContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../NumericContractTest.cpp"
    LINK_LIBS CoreUtilityFeatureModules ArtifactCoreStandaloneFoundationModules
    LABELS "core;numeric;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreDebugIdentityStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DebugIdentityStandaloneTest.cpp"
    LINK_LIBS CoreQtUtilityFeatureModules ArtifactCoreStandaloneFoundationModules Qt6::Core
    LABELS "diagnostics;debug-identity;registry;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreSessionLedgerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../SessionLedgerStandaloneTest.cpp"
    LINK_LIBS CoreQtUtilityFeatureModules ArtifactCoreStandaloneFoundationModules Qt6::Core
    LABELS "diagnostics;session-ledger;serialization;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCorePerformanceProfilerContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../PerformanceProfilerContractTest.cpp"
    LINK_LIBS CoreUtilityFeatureModules ArtifactCoreStandaloneFoundationModules
    LABELS "core;profiling;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreSharedPtrStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../SharedPtrContractTest.cpp"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules
    LABELS "core;memory;shared-ptr;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactTimerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactTimerContractTest.cpp"
    LINK_LIBS CoreQtUtilityFeatureModules
    LABELS "core;timer;time;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactRandomStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactRandomContractTest.cpp"
    LINK_LIBS CoreQtUtilityFeatureModules
    LABELS "core;random;numeric;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreHandleStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../HandleContractTest.cpp"
    LINK_LIBS CoreQtUtilityFeatureModules Qt6::Core
    LABELS "core;handle;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreHashValueStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../HashValueContractTest.cpp"
    LINK_LIBS CoreQtUtilityImplementations CoreQtUtilityFeatureModules Qt6::Core
    LABELS "core;hash;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreUtilityConceptsStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../UtilityConceptsContractTest.cpp"
    LINK_LIBS CoreQtUtilityFeatureModules Qt6::Core
    LABELS "core;concepts;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreRationalTimeStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../RationalTimeContractTest.cpp"
    LINK_LIBS CoreQtUtilityImplementations CoreQtUtilityFeatureModules Qt6::Core
    LABELS "time;rational;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreFrameSkipTrackerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../FrameSkipTrackerStandaloneTest.cpp"
    LINK_LIBS CoreQtUtilityImplementations CoreQtUtilityFeatureModules Qt6::Core
    LABELS "core;frame-skip-tracker;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactThreadStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactThreadContractTest.cpp"
    LINK_LIBS CoreThreadFeatureModules Threads::Threads
    LABELS "core;thread;concurrency;cpu;contract;standalone"
)

if(ARTIFACT_STANDALONE_GTEST_GROUP)
    artifact_finalize_standalone_gtest_group("${ARTIFACT_STANDALONE_GTEST_GROUP}")
endif()

enable_testing()
