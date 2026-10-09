cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreColorSpaceStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

artifact_add_standalone_module_interface(ColorSpaceModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorSpace.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorGamutConversion.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorTransferFunction.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorConversion.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorACES.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(ColorSpaceUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorSpace.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorConversion.cppm"
    LINK_LIBS ColorSpaceModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(ColorSpaceUnderTest ColorSpaceModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreColorSpaceTest
    SOURCES ../ColorSpaceTest.cpp
    LINK_LIBS ColorSpaceUnderTest ColorSpaceModuleInterfaces
    LABELS "color;color-space;cpu;unit"
)

artifact_add_standalone_gtest(ArtifactCoreColorPipelineTest
    SOURCES ../ColorPipelineStandaloneTest.cpp
    LINK_LIBS ColorSpaceModuleInterfaces
    LABELS "color;color-pipeline;cpu;unit"
)

artifact_add_standalone_gtest(ArtifactCoreColorTransferFunctionTest
    SOURCES ../ColorTransferFunctionStandaloneTest.cpp
    LINK_LIBS ColorSpaceModuleInterfaces
    LABELS "color;color-transfer;cpu;unit"
)

artifact_add_standalone_gtest(ArtifactCoreColorConversionTest
    SOURCES ../ColorConversionTest.cpp
    LINK_LIBS ColorSpaceUnderTest ColorSpaceModuleInterfaces
    LABELS "color;color-conversion;cpu;unit"
)

artifact_add_standalone_gtest(ArtifactCoreColorACESTest
    SOURCES ../ColorACESContractTest.cpp
    LINK_LIBS ColorSpaceUnderTest ColorSpaceModuleInterfaces
    LABELS "color;color-aces;cpu;unit"
)

artifact_add_standalone_module_interface(ColorGamutConversionModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Color/ColorGamutConversion.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreColorGamutConversionTest
    SOURCES ../ColorGamutConversionTest.cpp
    LINK_LIBS ColorGamutConversionModuleInterfaces
    LABELS "color;color-gamut;cpu;unit"
)

artifact_add_standalone_module_interface(ColorBridgeModuleInterfaces
    BASE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorTransferFunction.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorGamutConversion.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/FloatColor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/FloatRGBA.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorBridge.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/TaggedColor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Graphics/SurfaceColorContract.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS Qt6::Gui
)

artifact_add_standalone_module_implementation(ColorBridgeUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Color/FloatColor.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Color/FloatRGBA.cppm"
    LINK_LIBS ColorBridgeModuleInterfaces Qt6::Gui
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(ColorBridgeUnderTest ColorBridgeModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreColorBridgeTest
    SOURCES ../ColorBridgeTest.cpp
    LINK_LIBS ColorBridgeUnderTest ColorBridgeModuleInterfaces
    LABELS "color;color-bridge;cpu;unit"
)

artifact_add_standalone_module_interface(ColoramaModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Common/Parallel.ixx"
        "${ARTIFACT_CORE_ROOT}/include/ImageProcessing/ColorTransform/Colorama.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS Qt6::Core Qt6::Gui
)

artifact_add_standalone_module_implementation(ColoramaUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Core/Parallel.cppm"
        "${ARTIFACT_CORE_ROOT}/src/ImageProcessing/ColorTransform/Colorama.cppm"
    LINK_LIBS ColoramaModuleInterfaces Qt6::Core Qt6::Gui
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(ColoramaUnderTest ColoramaModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreColoramaProcessorTest
    SOURCES ../ColoramaProcessorContractTest.cpp
    LINK_LIBS ColoramaUnderTest ColoramaModuleInterfaces
    LABELS "color;colorama;cpu;unit"
)

artifact_add_standalone_module_interface(SurfaceConversionModuleInterfaces
    BASE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorTransferFunction.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Graphics/SurfaceColorContract.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Image/ImageSurfaceView.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Image/SurfacePixelConversion.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreSurfacePixelConversionTest
    SOURCES ../SurfacePixelConversionTest.cpp
    LINK_LIBS SurfaceConversionModuleInterfaces
    LABELS "color;color-conversion;surface;cpu;unit"
)

artifact_add_standalone_gtest(ArtifactCoreImageSurfaceViewTest
    SOURCES ../ImageSurfaceViewTest.cpp
    LINK_LIBS SurfaceConversionModuleInterfaces
    LABELS "image;surface;cpu;unit"
)

artifact_add_standalone_module_interface(ColorLUTModuleInterfaces
    BASE_DIRS
        "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorLUT.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Common/Parallel.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS Qt6::Gui
)

artifact_add_standalone_module_implementation(ColorLUTUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorLUT.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Core/Parallel.cppm"
    LINK_LIBS ColorLUTModuleInterfaces Qt6::Gui
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(ColorLUTUnderTest ColorLUTModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreColorLUTTest
    SOURCES ../ColorLUTContractTest.cpp
    LINK_LIBS ColorLUTUnderTest ColorLUTModuleInterfaces
    LABELS "color;color-lut;cpu;unit"
)

artifact_add_standalone_module_interface(ColorBlendModeModuleInterfaces
    BASE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorTransferFunction.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/FloatColor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorConversion.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorLuminance.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorBlendMode.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_module_implementation(ColorBlendModeUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Color/FloatColor.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorConversion.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorLuminance.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorBlendMode.cppm"
    LINK_LIBS ColorBlendModeModuleInterfaces
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(ColorBlendModeUnderTest ColorBlendModeModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreColorBlendModeTest
    SOURCES ../ColorBlendModeStandaloneTest.cpp
    LINK_LIBS ColorBlendModeUnderTest ColorBlendModeModuleInterfaces
    LABELS "color;color-blend;cpu;unit"
)

artifact_add_standalone_module_interface(ColorLuminanceModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorLuminance.ixx"
)

artifact_add_standalone_module_implementation(ColorLuminanceUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Color/ColorLuminance.cppm"
    LINK_LIBS ColorLuminanceModuleInterfaces
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(ColorLuminanceUnderTest ColorLuminanceModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreColorLuminanceTest
    SOURCES ../ColorLuminanceContractTest.cpp
    LINK_LIBS ColorLuminanceUnderTest ColorLuminanceModuleInterfaces
    LABELS "color;color-luminance;cpu;unit"
)

artifact_add_standalone_module_interface(ColorHarmonizerModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorTransferFunction.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/FloatColor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorConversion.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorHarmonizer.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(ColorHarmonizerUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Color/FloatColor.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorConversion.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorHarmonizer.cppm"
    LINK_LIBS ColorHarmonizerModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(ColorHarmonizerUnderTest ColorHarmonizerModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreColorHarmonizerTest
    SOURCES ../ColorHarmonizerContractTest.cpp
    LINK_LIBS ColorHarmonizerUnderTest ColorHarmonizerModuleInterfaces
    LABELS "color;color-harmonizer;cpu;unit"
)

enable_testing()
