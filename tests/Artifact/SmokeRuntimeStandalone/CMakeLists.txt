cmake_minimum_required(VERSION 3.30)

project(ArtifactSmokeRuntimeStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Gui)
find_package(TBB REQUIRED)
find_package(Vulkan REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../../ArtifactCore/StandaloneTestHelpers.cmake")

set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../Artifact")
set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(DILIGENT_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../libs/DiligentEngine")
set(DILIGENT_BUILD_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../build/libs/DiligentEngine/DiligentCore")

artifact_add_standalone_module_interface(SmokeRuntimeModuleInterfaces
    BASE_DIRS
        "${ARTIFACT_CORE_ROOT}"
        "${ARTIFACT_ROOT}"
        "${CMAKE_CURRENT_LIST_DIR}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Common/Parallel.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/ColorTransferFunction.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Color/FloatColor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Geometry/Float3.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Graphics/GPUInfo.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Graphics/GPUCapabilities.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Graphics/GPUComputeContext.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Graphics/Shader/Compute/Compute.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Physics/FluidSolver2D.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Simulation/PyroSimulation.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/Vector3D.ixx"
        "${CMAKE_CURRENT_LIST_DIR}/TestRay.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/Camera.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/ImageBuffer.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Render/VolumeRenderer.ixx"
        "${CMAKE_CURRENT_LIST_DIR}/TestParticleData.ixx"
        "${CMAKE_CURRENT_LIST_DIR}/TestArtifactIRenderer.ixx"
        "${ARTIFACT_ROOT}/include/Layer/ArtifactLayerFluidRuntimeState.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/include"
        "${DILIGENT_ENGINE_ROOT}"
    LINK_LIBS Qt6::Gui Boost::boost
)

target_compile_definitions(SmokeRuntimeModuleInterfaces PRIVATE PLATFORM_WIN32)

artifact_add_standalone_module_implementation(SmokeRuntimeCoreImplementation
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Physics/FluidSolver2D.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Core/Parallel.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Simulation/PyroSimulation.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Render/VolumeRenderer.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Color/FloatColor.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Graphics/Compute.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Graphics/GPUComputeContext.cpp"
    LINK_LIBS SmokeRuntimeModuleInterfaces TBB::tbb
        Vulkan::Vulkan
        "${DILIGENT_BUILD_ROOT}/Graphics/GraphicsEngineVulkan/$<CONFIG>/Diligent-GraphicsEngineVk-static.lib"
        "${DILIGENT_BUILD_ROOT}/ThirdParty/volk/$<CONFIG>/volk.lib"
        "${DILIGENT_BUILD_ROOT}/Graphics/GraphicsEngineD3D12/$<CONFIG>/Diligent-GraphicsEngineD3D12-static.lib"
        "${DILIGENT_BUILD_ROOT}/Graphics/GraphicsEngineNextGenBase/$<CONFIG>/Diligent-GraphicsEngineNextGenBase.lib"
        "${DILIGENT_BUILD_ROOT}/Graphics/GraphicsEngineD3DBase/$<CONFIG>/Diligent-GraphicsEngineD3DBase.lib"
        "${DILIGENT_BUILD_ROOT}/Graphics/GraphicsEngine/$<CONFIG>/Diligent-GraphicsEngine.lib"
        "${DILIGENT_BUILD_ROOT}/Graphics/GraphicsAccessories/$<CONFIG>/Diligent-GraphicsAccessories.lib"
        "${DILIGENT_BUILD_ROOT}/Graphics/ShaderTools/$<CONFIG>/Diligent-ShaderTools.lib"
        "${DILIGENT_BUILD_ROOT}/Graphics/HLSL2GLSLConverterLib/$<CONFIG>/Diligent-HLSL2GLSLConverterLib.lib"
        "${DILIGENT_BUILD_ROOT}/ThirdParty/SPIRV-Cross/$<CONFIG>/spirv-cross-core.lib"
        "${DILIGENT_BUILD_ROOT}/ThirdParty/glslang/SPIRV/$<CONFIG>/SPIRV.lib"
        "${DILIGENT_BUILD_ROOT}/ThirdParty/glslang/glslang/$<CONFIG>/glslang.lib"
        "${DILIGENT_BUILD_ROOT}/ThirdParty/SPIRV-Tools/source/opt/$<CONFIG>/SPIRV-Tools-opt.lib"
        "${DILIGENT_BUILD_ROOT}/ThirdParty/SPIRV-Tools/source/$<CONFIG>/SPIRV-Tools.lib"
        "${DILIGENT_BUILD_ROOT}/Common/$<CONFIG>/Diligent-Common.lib"
        "${DILIGENT_BUILD_ROOT}/Platforms/Win32/$<CONFIG>/Diligent-Win32Platform.lib"
        "${DILIGENT_BUILD_ROOT}/Platforms/Basic/$<CONFIG>/Diligent-BasicPlatform.lib"
        "${DILIGENT_BUILD_ROOT}/Primitives/$<CONFIG>/Diligent-Primitives.lib"
        d3d12 dxgi d3dcompiler
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include" "${DILIGENT_ENGINE_ROOT}"
)

target_compile_definitions(SmokeRuntimeCoreImplementation PRIVATE
    ARTIFACT_SMOKE_RUNTIME_RND=1
    PLATFORM_WIN32
)

artifact_add_standalone_module_implementation(SmokeRuntimeImplementation
    SOURCES "${ARTIFACT_ROOT}/src/Layer/ArtifactLayerFluidRuntimeState.cpp"
    LINK_LIBS SmokeRuntimeModuleInterfaces SmokeRuntimeCoreImplementation Qt6::Gui
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include" "${ARTIFACT_ROOT}/include"
        "${DILIGENT_ENGINE_ROOT}"
)

artifact_add_standalone_gtest(ArtifactSmokeRuntimeStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/SmokeRuntimeStandaloneTest.cpp"
    LINK_LIBS SmokeRuntimeImplementation SmokeRuntimeCoreImplementation
        SmokeRuntimeModuleInterfaces Qt6::Gui TBB::tbb
    LABELS "artifact;physics;fluid;smoke;runtime;standalone"
)

enable_testing()
