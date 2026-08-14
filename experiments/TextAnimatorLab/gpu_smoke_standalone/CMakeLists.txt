cmake_minimum_required(VERSION 3.28)
project(TextAnimatorGpuSmokeStandalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(PREBUILT_ROOT "${ARTIFACT_ROOT}/out/build/x64-Debug")
set(PREBUILT_ARTIFACT_IFC "${PREBUILT_ROOT}/Artifact/CMakeFiles/ArtifactRender.dir")
set(PREBUILT_CORE_IFC "${PREBUILT_ROOT}/ArtifactCore/CMakeFiles/ArtifactCore.dir")
set(PREBUILT_LIB_ROOT "${PREBUILT_ROOT}")
set(DILIGENT_TEXT_SMOKE_ROOT "${ARTIFACT_ROOT}/build_gpu_text_clean")
set(VCPKG_ROOT "${ARTIFACT_ROOT}/out/vcpkg_installed/x64-windows")

find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui)

add_executable(ArtifactRenderTextSmokeStandalone
    "${CMAKE_CURRENT_LIST_DIR}/../artifact_gpu_text_smoke.cpp"
)

target_include_directories(ArtifactRenderTextSmokeStandalone PRIVATE
    "${ARTIFACT_ROOT}/Artifact/include"
    "${ARTIFACT_ROOT}/ArtifactCore/include"
    "${ARTIFACT_ROOT}/libs/DiligentEngine"
    "${ARTIFACT_ROOT}/third_party"
    "${VCPKG_ROOT}/include"
)

target_compile_definitions(ArtifactRenderTextSmokeStandalone PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    PLATFORM_WIN32
    _WIN32_WINNT=0x0A00
    _GUARDOVERFLOW_CRT_ALLOCATORS=1
    _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
)

if(MSVC)
    target_compile_options(ArtifactRenderTextSmokeStandalone PRIVATE
        /Zf /Zc:__cplusplus /utf-8 /openmp /wd5050 /permissive-
        "/ifcSearchDir${PREBUILT_ARTIFACT_IFC}"
        "/ifcSearchDir${PREBUILT_CORE_IFC}"
    )
endif()

target_link_directories(ArtifactRenderTextSmokeStandalone PRIVATE
    "${VCPKG_ROOT}/debug/lib"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/ShaderTools/Debug"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Common/Debug"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsAccessories/Debug"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/Debug"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/Debug"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3DBase/Debug"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineNextGenBase/Debug"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsTools/Debug"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Platforms/Basic/Debug"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Platforms/Win32/Debug"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Primitives/Debug"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/ShaderTools"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Common"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsAccessories"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3DBase"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineNextGenBase"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsTools"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineVulkan"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/HLSL2GLSLConverterLib"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Platforms/Basic"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Platforms/Win32"
    "${DILIGENT_TEXT_SMOKE_ROOT}/libs/DiligentEngine/DiligentCore/Primitives"
    "${PREBUILT_ROOT}/Artifact"
    "${PREBUILT_ROOT}/ArtifactCore"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Common"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsAccessories"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3DBase"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineNextGenBase"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsTools"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineVulkan"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/HLSL2GLSLConverterLib"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Graphics/ShaderTools"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Platforms/Basic"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Platforms/Win32"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/Primitives"
    "${PREBUILT_ROOT}/_deps/nvapi-src/amd64"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/ThirdParty/glslang/glslang"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/ThirdParty/glslang/SPIRV"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/ThirdParty/SPIRV-Cross"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/ThirdParty/SPIRV-Tools/source"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/ThirdParty/SPIRV-Tools/source/opt"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/ThirdParty/volk"
    "${PREBUILT_ROOT}/libs/DiligentEngine/DiligentCore/ThirdParty/xxHash/build/cmake"
)

target_link_libraries(ArtifactRenderTextSmokeStandalone PRIVATE
    ArtifactRender
    ArtifactCore
    Diligent-GraphicsTools
    Diligent-ShaderTools
    Diligent-GraphicsEngineVk-static
    Diligent-GraphicsEngineD3D12-static
    Diligent-GraphicsEngineD3DBase
    Diligent-GraphicsEngineNextGenBase
    Diligent-GraphicsEngine
    Diligent-HLSL2GLSLConverterLib
    Diligent-GraphicsAccessories
    Diligent-Common
    Diligent-BasicPlatform
    Diligent-Win32Platform
    Diligent-Primitives
    Qt6::Core
    Qt6::Gui
    dxgi
    d3dcompiler
    nvapi64
    opencv_core4d
    opencv_imgproc4d
    opencv_imgcodecs4d
    OpenImageIO_d
    OpenImageIO_Util_d
    glslangd
    SPIRVd
    spirv-cross-cored
    SPIRV-Tools
    SPIRV-Tools-opt
    volk
    xxhash
)
