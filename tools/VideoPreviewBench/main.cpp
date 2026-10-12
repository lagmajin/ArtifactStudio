#include <QApplication>
#include <QBasicTimer>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QMainWindow>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>
#include <QTimer>
#include <QTimerEvent>
#include <QThread>
#include <QTemporaryDir>
#include <QWidget>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/CommandQueueD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/RenderDeviceD3D12.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d12va.h>
}

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Application.Manager;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;
import Artifact.Layer.Video;
import MediaPlaybackController;
import Playback.State;
import Artifact.Project.Manager;
import Artifact.Service.Playback;
import Artifact.Widgets.CompositionEditor;
import Artifact.Widgets.CompositionRenderController;
import Artifact.Render.DiligentDeviceManager;
import Frame.Debug;
import Frame.Position;
import Frame.Range;
import Frame.Rate;
import Utils.String.UniString;

namespace {

AVPixelFormat chooseProbeHardwareFormat(AVCodecContext*,
                                        const AVPixelFormat* formats)
{
    if (!formats) return AV_PIX_FMT_NONE;
    for (const AVPixelFormat* format = formats;
         *format != AV_PIX_FMT_NONE; ++format) {
        if (*format == AV_PIX_FMT_D3D12) return *format;
    }
    return formats[0];
}

struct Nv12ProbeSlot {
    ID3D12DescriptorHeap* descriptors = nullptr;
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12Resource* output = nullptr;
    ID3D12Resource* readback = nullptr;
    Diligent::ITexture* wrappedTexture = nullptr;
    AVFrame* retainedSourceFrame = nullptr;
    UINT64 completionValue = 0;
    D3D12_RESOURCE_STATES outputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    bool inFlight = false;
    bool validateOutput = false;
    int sampleIndex = -1;

    ~Nv12ProbeSlot()
    {
        if (retainedSourceFrame) av_frame_free(&retainedSourceFrame);
        if (wrappedTexture) wrappedTexture->Release();
        if (readback) readback->Release();
        if (output) output->Release();
        if (commandList) commandList->Release();
        if (allocator) allocator->Release();
        if (descriptors) descriptors->Release();
    }
};

struct Nv12ProbeResources {
    static constexpr size_t SlotCount = 3;
    ID3D12Device* device = nullptr;
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* pipeline = nullptr;
    ID3D12Fence* completionFence = nullptr;
    ID3DBlob* shader = nullptr;
    ID3DBlob* rootSignatureBlob = nullptr;
    HANDLE completionEvent = nullptr;
    UINT64 completionValue = 0;
    std::array<Nv12ProbeSlot, SlotCount> probeSlots{};

    ~Nv12ProbeResources()
    {
        if (completionEvent) CloseHandle(completionEvent);
        if (rootSignatureBlob) rootSignatureBlob->Release();
        if (shader) shader->Release();
        if (completionFence) completionFence->Release();
        if (pipeline) pipeline->Release();
        if (rootSignature) rootSignature->Release();
        if (device) device->Release();
    }
};

Nv12ProbeResources& nv12ProbeResources()
{
    static Nv12ProbeResources resources;
    return resources;
}

bool enqueueNv12ComputeConversionProbe(
                                      Diligent::IRenderDeviceD3D12* diligentDevice,
                                      Diligent::IDeviceContext* immediateContext,
                                      const AVD3D12VAFrame* source,
                                      AVFrame* sourceFrame,
                                      Nv12ProbeSlot& slot,
                                      int width, int height,
                                      int sampleIndex)
{
    Nv12ProbeResources& resources = nv12ProbeResources();
    ID3D12Device* device = diligentDevice
        ? diligentDevice->GetD3D12Device() : nullptr;
    if (!device || !immediateContext || !source || !sourceFrame || slot.inFlight || !source->texture || !source->sync_ctx.fence ||
        width != 1920 || height != 1080) return false;
    if (!resources.device) {
        resources.device = device;
        resources.device->AddRef();
    }
    if (resources.device != device) return false;

    const D3D12_RESOURCE_DESC sourceDesc = source->texture->GetDesc();
    if (sourceDesc.Format != DXGI_FORMAT_NV12 ||
        sourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        source->subresource_index < 0 ||
        sourceDesc.DepthOrArraySize <= source->subresource_index) {
        std::cerr << "SHARED_D3D12_CONVERT unsupported_source_desc format="
                  << sourceDesc.Format << " arraySize=" << sourceDesc.DepthOrArraySize
                  << " slice=" << source->subresource_index << "\n";
        return false;
    }

    using D3DCompileFn = decltype(&D3DCompile);
    HMODULE compilerModule = LoadLibraryW(L"d3dcompiler_47.dll");
    auto compile = compilerModule ? reinterpret_cast<D3DCompileFn>(
        GetProcAddress(compilerModule, "D3DCompile")) : nullptr;
    if (!compile) {
        if (compilerModule) FreeLibrary(compilerModule);
        std::cerr << "SHARED_D3D12_CONVERT shader_compiler=unavailable\n";
        return false;
    }

static constexpr char shaderSource[] = R"(
Texture2DArray<float> Luma : register(t0);
Texture2DArray<float2> Chroma : register(t1);
RWTexture2D<float4> Output : register(u0);
[numthreads(16,16,1)]
void main(uint3 p : SV_DispatchThreadID) {
    if (p.x >= 1920 || p.y >= 1080) return;
    float y = 1.16438356 * (Luma.Load(int4(p.xy, 0, 0)) - 0.0625);
    float2 uv = Chroma.Load(int4(p.xy / 2, 0, 0)) - float2(0.5, 0.5);
    float3 rgb = float3(y + 1.59602678 * uv.y,
                        y - 0.39176229 * uv.x - 0.81296764 * uv.y,
                        y + 2.01723214 * uv.x);
    Output[p.xy] = float4(saturate(rgb), 1.0);
})";
    ID3DBlob*& shader = resources.shader;
    ID3DBlob* shaderErrors = nullptr;
    if (!shader) {
        const HRESULT compileResult = compile(
            shaderSource, sizeof(shaderSource) - 1, "ArtifactVideoPreviewProbe",
            nullptr, nullptr, "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3,
            0, &shader, &shaderErrors);
        if (FAILED(compileResult) || !shader) {
            if (shaderErrors) {
                std::cerr << "SHARED_D3D12_CONVERT shader_error="
                          << static_cast<const char*>(shaderErrors->GetBufferPointer())
                          << "\n";
                shaderErrors->Release();
            }
            if (shader) shader->Release();
            shader = nullptr;
            FreeLibrary(compilerModule);
            return false;
        }
    }
    if (shaderErrors) shaderErrors->Release();
    FreeLibrary(compilerModule);

    bool success = false;
    ID3D12CommandAllocator*& allocator = slot.allocator;
    ID3D12GraphicsCommandList*& commandList = slot.commandList;
    ID3D12DescriptorHeap*& descriptors = slot.descriptors;
    ID3D12RootSignature*& rootSignature = resources.rootSignature;
    ID3D12PipelineState*& pipeline = resources.pipeline;
    ID3D12Resource*& output = slot.output;
    ID3D12Resource*& readback = slot.readback;
    ID3D12Fence*& completionFence = resources.completionFence;
    ID3DBlob*& rootSignatureBlob = resources.rootSignatureBlob;
    ID3DBlob* rootSignatureErrors = nullptr;
    const bool decoderFenceReadyAtSubmit =
        source->sync_ctx.fence->GetCompletedValue() >= source->sync_ctx.fence_value;

    auto executeConversion = [&]() -> bool {
    if (!descriptors) {
        D3D12_DESCRIPTOR_HEAP_DESC descriptorDesc{};
        descriptorDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descriptorDesc.NumDescriptors = 3;
        descriptorDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(device->CreateDescriptorHeap(
                &descriptorDesc, IID_PPV_ARGS(&descriptors)))) return false;
    }

    const UINT descriptorIncrement = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle =
        descriptors->GetCPUDescriptorHandleForHeapStart();
    for (UINT plane = 0; plane < 2; ++plane) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format = plane == 0 ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8_UNORM;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srv.Texture2DArray.MostDetailedMip = 0;
        srv.Texture2DArray.MipLevels = 1;
        srv.Texture2DArray.FirstArraySlice =
            static_cast<UINT>(source->subresource_index);
        srv.Texture2DArray.ArraySize = 1;
        srv.Texture2DArray.PlaneSlice = plane;
        device->CreateShaderResourceView(source->texture, &srv, cpuHandle);
        cpuHandle.ptr += descriptorIncrement;
    }
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    if (!output) {
        D3D12_RESOURCE_DESC outputDesc{};
        outputDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        outputDesc.Width = static_cast<UINT64>(width);
        outputDesc.Height = static_cast<UINT>(height);
        outputDesc.DepthOrArraySize = 1;
        outputDesc.MipLevels = 1;
        outputDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        outputDesc.SampleDesc.Count = 1;
        outputDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        outputDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        D3D12_HEAP_PROPERTIES defaultHeap{};
        defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
        if (FAILED(device->CreateCommittedResource(
                &defaultHeap, D3D12_HEAP_FLAG_NONE, &outputDesc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                IID_PPV_ARGS(&output)))) return false;
    }
    device->CreateUnorderedAccessView(output, nullptr, &uav, cpuHandle);

    D3D12_DESCRIPTOR_RANGE ranges[2]{};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 2;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = 0;
    if (!rootSignature || !pipeline) {
        D3D12_ROOT_PARAMETER rootParams[2]{};
        for (UINT i = 0; i < 2; ++i) {
            rootParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParams[i].DescriptorTable.NumDescriptorRanges = 1;
            rootParams[i].DescriptorTable.pDescriptorRanges = &ranges[i];
            rootParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }
        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = 2;
        rootDesc.pParameters = rootParams;
        if (!rootSignatureBlob && FAILED(D3D12SerializeRootSignature(
                &rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                &rootSignatureBlob, &rootSignatureErrors))) return false;
        if (!rootSignature && FAILED(device->CreateRootSignature(
                0, rootSignatureBlob->GetBufferPointer(),
                rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))) return false;
        if (!pipeline) {
            D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
            pipelineDesc.pRootSignature = rootSignature;
            pipelineDesc.CS.pShaderBytecode = shader->GetBufferPointer();
            pipelineDesc.CS.BytecodeLength = shader->GetBufferSize();
            if (FAILED(device->CreateComputePipelineState(
                    &pipelineDesc, IID_PPV_ARGS(&pipeline)))) return false;
        }
    }

    constexpr UINT rowPitch = ((1920u * 4u + 255u) / 256u) * 256u;
    if (!readback) {
        D3D12_RESOURCE_DESC readbackDesc{};
        readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        readbackDesc.Width = static_cast<UINT64>(rowPitch) * 1080u;
        readbackDesc.Height = 1;
        readbackDesc.DepthOrArraySize = 1;
        readbackDesc.MipLevels = 1;
        readbackDesc.SampleDesc.Count = 1;
        readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_HEAP_PROPERTIES readbackHeap{};
        readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
        if (FAILED(device->CreateCommittedResource(
                &readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDesc,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                IID_PPV_ARGS(&readback)))) return false;
    }
    if (!allocator && FAILED(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)))) return false;
    if (!commandList) {
        if (FAILED(device->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, pipeline,
                IID_PPV_ARGS(&commandList)))) return false;
    } else {
        if (FAILED(allocator->Reset()) || FAILED(commandList->Reset(allocator, pipeline)))
            return false;
    }

    D3D12_RESOURCE_BARRIER inputBarrier{};
    inputBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    inputBarrier.Transition.pResource = source->texture;
    inputBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    inputBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    inputBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &inputBarrier);
    commandList->SetDescriptorHeaps(1, &descriptors);
    commandList->SetComputeRootSignature(rootSignature);
    commandList->SetPipelineState(pipeline);
    if (slot.outputState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER prepareOutput{};
        prepareOutput.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        prepareOutput.Transition.pResource = output;
        prepareOutput.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        prepareOutput.Transition.StateBefore = slot.outputState;
        prepareOutput.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        commandList->ResourceBarrier(1, &prepareOutput);
        slot.outputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
        descriptors->GetGPUDescriptorHandleForHeapStart();
    commandList->SetComputeRootDescriptorTable(0, gpuHandle);
    gpuHandle.ptr += 2u * descriptorIncrement;
    commandList->SetComputeRootDescriptorTable(1, gpuHandle);
    commandList->Dispatch((static_cast<UINT>(width) + 15u) / 16u,
                         (static_cast<UINT>(height) + 15u) / 16u, 1);

    D3D12_RESOURCE_BARRIER outputBarrier{};
    outputBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    outputBarrier.Transition.pResource = output;
    outputBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    const bool validateOutput = sampleIndex == 0;
    outputBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    outputBarrier.Transition.StateAfter = validateOutput
        ? D3D12_RESOURCE_STATE_COPY_SOURCE
        : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &outputBarrier);
    inputBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    inputBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    commandList->ResourceBarrier(1, &inputBarrier);
    if (validateOutput) {
        D3D12_TEXTURE_COPY_LOCATION srcLocation{};
        srcLocation.pResource = output;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLocation.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION dstLocation{};
        dstLocation.pResource = readback;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dstLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        dstLocation.PlacedFootprint.Footprint.Width = 1920;
        dstLocation.PlacedFootprint.Footprint.Height = 1080;
        dstLocation.PlacedFootprint.Footprint.Depth = 1;
        dstLocation.PlacedFootprint.Footprint.RowPitch = rowPitch;
        commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
        outputBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        outputBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        commandList->ResourceBarrier(1, &outputBarrier);
    }
    slot.outputState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    if (FAILED(commandList->Close())) return false;
    AVFrame* retainedFrame = av_frame_clone(sourceFrame);
    if (!retainedFrame) return false;
    if (!completionFence && FAILED(device->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&completionFence)))) {
        av_frame_free(&retainedFrame);
        return false;
    }
    Diligent::RefCntAutoPtr<Diligent::ICommandQueue> queueInterface{
        immediateContext->LockCommandQueue()};
    if (!queueInterface) {
        av_frame_free(&retainedFrame);
        return false;
    }
    struct QueueUnlock final {
        Diligent::IDeviceContext* context;
        ~QueueUnlock() { context->UnlockCommandQueue(); }
    } queueUnlock{immediateContext};
    Diligent::RefCntAutoPtr<Diligent::ICommandQueueD3D12> diligentQueue{
        queueInterface, Diligent::IID_CommandQueueD3D12};
    ID3D12CommandList* commandLists[] = {commandList};
    if (!diligentQueue) {
        av_frame_free(&retainedFrame);
        return false;
    }
    diligentQueue->WaitFence(source->sync_ctx.fence,
                             source->sync_ctx.fence_value);
    const Diligent::Uint64 queueSubmitValue = diligentQueue->Submit(
        1, commandLists);
    const UINT64 submittedValue = ++resources.completionValue;
    diligentQueue->EnqueueSignal(completionFence, submittedValue);
    slot.completionValue = submittedValue;
    slot.validateOutput = validateOutput;
    slot.sampleIndex = sampleIndex;
    slot.retainedSourceFrame = retainedFrame;
    slot.inFlight = true;
    std::cerr << "SHARED_D3D12_CONVERT result=ok sample=" << sampleIndex
              << " outputResource=" << output
              << " state=PIXEL_SHADER_RESOURCE"
              << " diligentQueueSubmitValue=" << queueSubmitValue
              << " completionValue=" << slot.completionValue
              << " decoderFenceReadyAtSubmit="
              << (decoderFenceReadyAtSubmit ? 1 : 0) << "\n";
    return true;
    };
    success = executeConversion();

    if (success && !slot.wrappedTexture) {
        diligentDevice->CreateTextureFromD3DResource(
            output, Diligent::RESOURCE_STATE_SHADER_RESOURCE, &slot.wrappedTexture);
    }
    if (success) {
        const bool wrapped = slot.wrappedTexture && slot.wrappedTexture->GetDefaultView(
            Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
        std::cerr << "SHARED_D3D12_CONVERT diligentTexture="
                  << (wrapped ? 1 : 0) << "\n";
        success = wrapped;
    }

cleanup:
    if (rootSignatureErrors) rootSignatureErrors->Release();
    if (!success) std::cerr << "SHARED_D3D12_CONVERT result=failed\n";
    return success;
}

bool retireNv12ProbeSlot(Nv12ProbeResources& resources, Nv12ProbeSlot& slot,
                         bool wait)
{
    if (!slot.inFlight) return true;
    if (resources.completionFence->GetCompletedValue() < slot.completionValue) {
        if (!wait) return false;
        if (!resources.completionEvent) {
            resources.completionEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!resources.completionEvent) return false;
        }
        if (FAILED(resources.completionFence->SetEventOnCompletion(
                slot.completionValue, resources.completionEvent)) ||
            WaitForSingleObject(resources.completionEvent, 10000) != WAIT_OBJECT_0)
            return false;
    }
    if (slot.validateOutput) {
        constexpr UINT rowPitch = ((1920u * 4u + 255u) / 256u) * 256u;
        D3D12_RANGE readRange{0, static_cast<SIZE_T>(rowPitch) * 1080u};
        const std::uint8_t* pixels = nullptr;
        if (FAILED(slot.readback->Map(0, &readRange,
                reinterpret_cast<void**>(const_cast<std::uint8_t**>(&pixels)))))
            return false;
        const std::uint8_t* sample = pixels +
            static_cast<std::size_t>(540) * rowPitch + 960u * 4u;
        std::cerr << "SHARED_D3D12_VALIDATE sample=" << slot.sampleIndex
                  << " rgba=" << static_cast<int>(sample[0]) << ","
                  << static_cast<int>(sample[1]) << ","
                  << static_cast<int>(sample[2]) << ","
                  << static_cast<int>(sample[3]) << "\n";
        D3D12_RANGE writtenRange{0, 0};
        slot.readback->Unmap(0, &writtenRange);
    }
    if (slot.retainedSourceFrame) av_frame_free(&slot.retainedSourceFrame);
    slot.inFlight = false;
    return true;
}

bool probeSharedD3D12PlaybackControllerFrame(
    const QString& inputPath, ID3D12Device* rendererDevice)
{
    const bool hadPreviousValue = qEnvironmentVariableIsSet(
        "ARTIFACT_VIDEO_PREVIEW_FFMPEG_HWACCEL");
    const QByteArray previousValue = qgetenv(
        "ARTIFACT_VIDEO_PREVIEW_FFMPEG_HWACCEL");
    qputenv("ARTIFACT_VIDEO_PREVIEW_FFMPEG_HWACCEL", "d3d12va");

    ArtifactCore::MediaPlaybackController controller;
    controller.setDecoderBackend(ArtifactCore::DecoderBackend::FFmpeg);
    if (!controller.openMediaFile(inputPath)) {
        std::cerr << "SHARED_D3D12_CONTROLLER open=failed error="
                  << controller.getLastError().toUtf8().constData() << "\n";
        if (hadPreviousValue) qputenv("ARTIFACT_VIDEO_PREVIEW_FFMPEG_HWACCEL", previousValue);
        else qunsetenv("ARTIFACT_VIDEO_PREVIEW_FFMPEG_HWACCEL");
        return false;
    }
    controller.setD3D12Device(rendererDevice);
    const ArtifactCore::DecodedVideoFrame decoded =
        controller.getVideoFrameAtFrameDirectRaw(0, true);

    bool validSharedGpuFrame = false;
    if (const auto* gpu = std::get_if<ArtifactCore::GpuVideoFrame>(&decoded)) {
        const auto* handle = std::get_if<ArtifactCore::D3D12VideoFrameHandle>(
            &gpu->handle);
        ID3D12Resource* texture = handle
            ? static_cast<ID3D12Resource*>(handle->texture) : nullptr;
        ID3D12Device* frameDevice = nullptr;
        const HRESULT deviceResult = texture
            ? texture->GetDevice(IID_PPV_ARGS(&frameDevice)) : E_POINTER;
        validSharedGpuFrame = SUCCEEDED(deviceResult) && frameDevice == rendererDevice &&
                              gpu->storage == ArtifactCore::VideoFrameStorageKind::D3D12Texture;
        std::cerr << "SHARED_D3D12_CONTROLLER frame=0 storage="
                  << static_cast<int>(gpu->storage)
                  << " size=" << gpu->meta.width << "x" << gpu->meta.height
                  << " format=" << static_cast<int>(gpu->meta.pixelFormat)
                  << " sameDevice=" << (validSharedGpuFrame ? 1 : 0)
                  << " texture=" << texture << "\n";
        if (frameDevice) frameDevice->Release();
    } else {
        std::cerr << "SHARED_D3D12_CONTROLLER frame=0 result=not-d3d12-gpu\n";
    }
    controller.closeMedia();
    if (hadPreviousValue) qputenv("ARTIFACT_VIDEO_PREVIEW_FFMPEG_HWACCEL", previousValue);
    else qunsetenv("ARTIFACT_VIDEO_PREVIEW_FFMPEG_HWACCEL");
    return validSharedGpuFrame;
}

int runSharedD3D12DecodeProbe(const QString& inputPath)
{
    Artifact::SharedRenderDeviceLease lease;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> immediateContext;
    if (!lease.acquire(renderDevice, immediateContext) || !renderDevice) {
        std::cerr << "SHARED_D3D12_PROBE renderer_device=unavailable\n";
        return 2;
    }

    Diligent::RefCntAutoPtr<Diligent::IRenderDeviceD3D12> d3d12Device{
        renderDevice, Diligent::IID_RenderDeviceD3D12};
    ID3D12Device* nativeDevice = d3d12Device
        ? d3d12Device->GetD3D12Device() : nullptr;
    if (!nativeDevice) {
        std::cerr << "SHARED_D3D12_PROBE renderer_backend=not_d3d12\n";
        return 3;
    }
    const LUID rendererLuid = nativeDevice->GetAdapterLuid();
    if (!probeSharedD3D12PlaybackControllerFrame(inputPath, nativeDevice)) {
        std::cerr << "SHARED_D3D12_CONTROLLER result=failed\n";
        return 19;
    }

    AVBufferRef* deviceRef = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D12VA);
    if (!deviceRef) return 4;
    auto* hwDevice = reinterpret_cast<AVHWDeviceContext*>(deviceRef->data);
    auto* d3d12Context = reinterpret_cast<AVD3D12VADeviceContext*>(hwDevice->hwctx);
    if (!d3d12Context) {
        av_buffer_unref(&deviceRef);
        return 5;
    }
    d3d12Context->device = nativeDevice;
    nativeDevice->AddRef();
    const int hwInit = av_hwdevice_ctx_init(deviceRef);
    if (hwInit < 0) {
        char error[AV_ERROR_MAX_STRING_SIZE] = {};
        av_make_error_string(error, sizeof(error), hwInit);
        std::cerr << "SHARED_D3D12_PROBE hwdevice_init=" << hwInit
                  << " error=" << error << "\n";
        av_buffer_unref(&deviceRef);
        return 6;
    }

    AVFormatContext* format = nullptr;
    AVCodecContext* codecContext = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    int result = 0;
    int streamIndex = -1;
    const QByteArray encodedPath = QFile::encodeName(inputPath);
    int status = avformat_open_input(&format, encodedPath.constData(), nullptr, nullptr);
    if (status < 0 || avformat_find_stream_info(format, nullptr) < 0) {
        result = 7;
        goto cleanup;
    }

    streamIndex = av_find_best_stream(
        format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        result = 8;
        goto cleanup;
    }
    {
        AVStream* stream = format->streams[streamIndex];
        const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
        codecContext = decoder ? avcodec_alloc_context3(decoder) : nullptr;
        if (!codecContext || avcodec_parameters_to_context(
                codecContext, stream->codecpar) < 0) {
            result = 9;
            goto cleanup;
        }
        codecContext->hw_device_ctx = av_buffer_ref(deviceRef);
        codecContext->get_format = chooseProbeHardwareFormat;
        if (avcodec_open2(codecContext, decoder, nullptr) < 0) {
            result = 10;
            goto cleanup;
        }
    }

    packet = av_packet_alloc();
    frame = av_frame_alloc();
    if (!packet || !frame) {
        result = 11;
        goto cleanup;
    }
    {
        constexpr int sequenceFrameCount = 8;
        bool demuxEof = false;
        bool flushSent = false;
        int convertedFrames = 0;
        int completedFrames = 0;
        int maxInFlightSlots = 0;
        int64_t previousPts = AV_NOPTS_VALUE;
        bool ptsMonotonic = true;
        const auto sequenceStarted = std::chrono::steady_clock::now();

        auto receiveNextFrame = [&]() -> bool {
            for (;;) {
                status = avcodec_receive_frame(codecContext, frame);
                if (status == 0) return true;
                if (status == AVERROR_EOF) return false;
                if (status != AVERROR(EAGAIN)) return false;

                if (demuxEof) {
                    if (flushSent) return false;
                    flushSent = true;
                    const int flushResult = avcodec_send_packet(codecContext, nullptr);
                    if (flushResult < 0 && flushResult != AVERROR_EOF) return false;
                    continue;
                }

                bool packetSubmitted = false;
                while (av_read_frame(format, packet) >= 0) {
                    if (packet->stream_index == streamIndex) {
                        const int sendResult = avcodec_send_packet(codecContext, packet);
                        av_packet_unref(packet);
                        if (sendResult == 0 || sendResult == AVERROR(EAGAIN)) {
                            packetSubmitted = true;
                            break;
                        }
                        return false;
                    }
                    av_packet_unref(packet);
                }
                if (!packetSubmitted) {
                    demuxEof = true;
                }
            }
        };

        while (convertedFrames < sequenceFrameCount) {
            const auto decodeStarted = std::chrono::steady_clock::now();
            const bool frameReceived = receiveNextFrame();
            const auto frameReceivedAt = std::chrono::steady_clock::now();
            if (!frameReceived) break;
            if (frame->format != AV_PIX_FMT_D3D12 || !frame->data[0]) {
                std::cerr << "SHARED_D3D12_SEQUENCE decoded_format=" << frame->format
                          << " expected=" << AV_PIX_FMT_D3D12 << "\n";
                result = 14;
                av_frame_unref(frame);
                break;
            }

            auto* d3dFrame = reinterpret_cast<AVD3D12VAFrame*>(frame->data[0]);
            const int64_t pts = frame->best_effort_timestamp != AV_NOPTS_VALUE
                ? frame->best_effort_timestamp : frame->pts;
            if (previousPts != AV_NOPTS_VALUE && pts != AV_NOPTS_VALUE &&
                pts <= previousPts) {
                ptsMonotonic = false;
            }
            if (pts != AV_NOPTS_VALUE) previousPts = pts;

            ID3D12Device* frameDevice = nullptr;
            const HRESULT queryResult = d3dFrame->texture
                ? d3dFrame->texture->GetDevice(IID_PPV_ARGS(&frameDevice))
                : E_POINTER;
            const bool sameDevice = SUCCEEDED(queryResult) &&
                                    frameDevice == nativeDevice;
            if (convertedFrames == 0) {
                std::cerr << "SHARED_D3D12_PROBE rendererDevice=" << nativeDevice
                          << " frameDevice=" << frameDevice
                          << " sameDevice=" << (sameDevice ? 1 : 0)
                          << " rendererLuid=" << rendererLuid.HighPart << ":"
                          << rendererLuid.LowPart
                          << " size=" << frame->width << "x" << frame->height
                          << " format=" << frame->format
                          << " fenceValue=" << d3dFrame->sync_ctx.fence_value
                          << " textureFormat=" << (d3dFrame->texture
                              ? d3dFrame->texture->GetDesc().Format : DXGI_FORMAT_UNKNOWN)
                          << "\n";
            }
            if (frameDevice) frameDevice->Release();
            if (!sameDevice || !d3dFrame->texture || !d3dFrame->sync_ctx.fence) {
                result = 15;
                av_frame_unref(frame);
                break;
            }

            std::cerr << "SHARED_D3D12_SEQUENCE frame=" << convertedFrames
                      << " pts=" << pts
                      << " texture=" << d3dFrame->texture
                      << " subresource=" << d3dFrame->subresource_index
                      << " fence=" << d3dFrame->sync_ctx.fence
                      << " fenceValue=" << d3dFrame->sync_ctx.fence_value
                      << " fenceReady=" << (d3dFrame->sync_ctx.fence->GetCompletedValue() >=
                                                   d3dFrame->sync_ctx.fence_value ? 1 : 0)
                      << "\n";

            Nv12ProbeResources& probeResources = nv12ProbeResources();
            Nv12ProbeSlot& slot = probeResources.probeSlots[
                static_cast<size_t>(convertedFrames) % Nv12ProbeResources::SlotCount];
            const bool slotWasInFlight = slot.inFlight;
            const auto slotWaitStarted = std::chrono::steady_clock::now();
            if (!retireNv12ProbeSlot(probeResources, slot, true)) {
                result = 16;
                av_frame_unref(frame);
                break;
            }
            if (slotWasInFlight) ++completedFrames;
            const double slotWaitMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - slotWaitStarted).count();
            const auto conversionStarted = std::chrono::steady_clock::now();
            if (!enqueueNv12ComputeConversionProbe(
                    d3d12Device, immediateContext, d3dFrame, frame, slot,
                    frame->width, frame->height, convertedFrames)) {
                result = 16;
                av_frame_unref(frame);
                break;
            }
            const auto conversionCompleted = std::chrono::steady_clock::now();
            const double decodeWaitMs = std::chrono::duration<double, std::milli>(
                frameReceivedAt - decodeStarted).count();
            const double conversionWallMs = std::chrono::duration<double, std::milli>(
                conversionCompleted - conversionStarted).count();
            std::cerr << "SHARED_D3D12_SEQUENCE_TIMING frame=" << convertedFrames
                      << " receiveMs=" << decodeWaitMs
                      << " enqueueWallMs=" << conversionWallMs
                      << " slotWaitMs=" << slotWaitMs
                      << " slot=" << (convertedFrames % Nv12ProbeResources::SlotCount)
                      << "\n";
            ++convertedFrames;
            const int inFlightSlots = static_cast<int>(std::count_if(
                nv12ProbeResources().probeSlots.begin(),
                nv12ProbeResources().probeSlots.end(),
                [](const Nv12ProbeSlot& candidate) { return candidate.inFlight; }));
            maxInFlightSlots = std::max(maxInFlightSlots, inFlightSlots);
            av_frame_unref(frame);
        }

        for (Nv12ProbeSlot& slot : nv12ProbeResources().probeSlots) {
            const bool wasInFlight = slot.inFlight;
            if (!retireNv12ProbeSlot(nv12ProbeResources(), slot, true) && result == 0)
                result = 16;
            if (wasInFlight && !slot.inFlight) ++completedFrames;
        }

        const double sequenceMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - sequenceStarted).count();
        std::cerr << "SHARED_D3D12_SEQUENCE result="
                  << (result == 0 && convertedFrames == sequenceFrameCount ? "ok" : "incomplete")
                  << " decodedAndConverted=" << convertedFrames
                  << " completed=" << completedFrames
                  << " maxInFlightSlots=" << maxInFlightSlots
                  << " ptsMonotonic=" << (ptsMonotonic ? 1 : 0)
                  << " elapsedMs=" << sequenceMs << "\n";
        if (result == 0 && convertedFrames != sequenceFrameCount) result = 13;
        if (!ptsMonotonic && result == 0) result = 18;
    }

cleanup:
    if (frame) av_frame_free(&frame);
    if (packet) av_packet_free(&packet);
    if (codecContext) avcodec_free_context(&codecContext);
    if (format) avformat_close_input(&format);
    av_buffer_unref(&deviceRef);
    if (result != 0) {
        std::cerr << "SHARED_D3D12_PROBE result=" << result << "\n";
    }
    return result;
}

struct CaseResult {
    int layerCount = 0;
    bool passed = false;
    QString status;
    QString codec;
    QString error;
    int64_t droppedFrames = 0;
    int64_t requestedStart = 0;
    int64_t requestedEnd = 0;
    int64_t finalFrame = 0;
    int64_t finalPresentedFrame = -1;
    int64_t observedPresentedFrames = 0;
    quint64 renderCycles = 0;
    double maximumDiagnosticSampleMs = 0.0;
    qint64 maximumDiagnosticSampleAtMs = -1;
    double maximumVideoDiagnosticSampleMs = 0.0;
    QJsonArray videoTimingByLayer;
    int cachedImageWidth = 0;
    int cachedImageHeight = 0;
    double sourceFps = 0.0;
    int readyPreviewFrames = 0;
    int requestedPreviewFrames = 0;
    int inRamPreviewFrames = 0;
    int onDiskPreviewFrames = 0;
    int preparedRamPreviewFrames = 0;
    int preparedDiskPreviewFrames = 0;
    double previewCacheHitRate = 0.0;
    int lateFramesObserved = 0;
    int repeatedFramesObserved = 0;
    int frameTimingSampleCount = 0;
    double minimumOnTimePercent = 100.0;
    double measuredSeconds = 0.0;
    double requestedFps = 0.0;
    bool playbackStarted = false;
    bool playbackStillPlaying = false;
    QString videoDiagnostics;
    QJsonArray layerSources;
    QJsonArray renderTimingSamples;
};

void pumpFor(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::AllEvents);
}

// Stack-owned timers and callback: dispatch through Qt without a polling sleep,
// new signal connections, or per-tick callback allocation.
template<class Sample>
class PreviewMeasurementLoop final : public QEventLoop {
public:
    explicit PreviewMeasurementLoop(Sample& sample) : sample_(sample) {}

    void run(int milliseconds) {
        if (milliseconds <= 0) return;
        sampleTimer_.start(250, Qt::PreciseTimer, this);
        deadlineTimer_.start(milliseconds, Qt::PreciseTimer, this);
        exec(QEventLoop::AllEvents);
        sampleTimer_.stop();
        deadlineTimer_.stop();
    }

protected:
    void timerEvent(QTimerEvent* event) override {
        if (event->timerId() == deadlineTimer_.timerId()) {
            deadlineTimer_.stop();
            sampleTimer_.stop();
            quit();
        } else if (event->timerId() == sampleTimer_.timerId()) {
            sample_();
        } else {
            QEventLoop::timerEvent(event);
        }
    }

private:
    Sample& sample_;
    QBasicTimer sampleTimer_;
    QBasicTimer deadlineTimer_;
};

QJsonObject resultToJson(const CaseResult& result)
{
    QJsonObject object;
    object.insert(QStringLiteral("layerCount"), result.layerCount);
    object.insert(QStringLiteral("passed"), result.passed);
    object.insert(QStringLiteral("status"), result.status);
    object.insert(QStringLiteral("codec"), result.codec);
    object.insert(QStringLiteral("error"), result.error);
    object.insert(QStringLiteral("droppedFrames"),
                  static_cast<double>(result.droppedFrames));
    object.insert(QStringLiteral("requestedStart"),
                  static_cast<double>(result.requestedStart));
    object.insert(QStringLiteral("requestedEnd"),
                  static_cast<double>(result.requestedEnd));
    object.insert(QStringLiteral("finalFrame"),
                  static_cast<double>(result.finalFrame));
    object.insert(QStringLiteral("finalPresentedFrame"),
                  static_cast<double>(result.finalPresentedFrame));
    object.insert(QStringLiteral("observedPresentedFrames"), static_cast<double>(result.observedPresentedFrames));
    object.insert(QStringLiteral("renderCycles"), static_cast<double>(result.renderCycles));
    object.insert(QStringLiteral("maximumDiagnosticSampleMs"), result.maximumDiagnosticSampleMs);
    object.insert(QStringLiteral("maximumDiagnosticSampleAtMs"), result.maximumDiagnosticSampleAtMs);
    object.insert(QStringLiteral("maximumVideoDiagnosticSampleMs"), result.maximumVideoDiagnosticSampleMs);
    object.insert(QStringLiteral("videoTimingByLayer"), result.videoTimingByLayer);
    object.insert(QStringLiteral("cachedImageWidth"), result.cachedImageWidth);
    object.insert(QStringLiteral("cachedImageHeight"), result.cachedImageHeight);
    object.insert(QStringLiteral("requestedFps"), result.requestedFps);
    object.insert(QStringLiteral("playbackStarted"), result.playbackStarted);
    object.insert(QStringLiteral("playbackStillPlaying"), result.playbackStillPlaying);
    object.insert(QStringLiteral("videoDiagnostics"), result.videoDiagnostics);
    object.insert(QStringLiteral("renderTimingSamples"), result.renderTimingSamples);
    object.insert(QStringLiteral("sourceFps"), result.sourceFps);
    object.insert(QStringLiteral("measuredSeconds"), result.measuredSeconds);
    object.insert(QStringLiteral("previewReadyFrames"), result.readyPreviewFrames);
    object.insert(QStringLiteral("previewRequestedFrames"), result.requestedPreviewFrames);
    object.insert(QStringLiteral("previewFramesInRam"), result.inRamPreviewFrames);
    object.insert(QStringLiteral("previewFramesOnDisk"), result.onDiskPreviewFrames);
    object.insert(QStringLiteral("preparedRamPreviewFrames"), result.preparedRamPreviewFrames);
    object.insert(QStringLiteral("preparedDiskPreviewFrames"), result.preparedDiskPreviewFrames);
    object.insert(QStringLiteral("previewCacheHitRate"), result.previewCacheHitRate);
    object.insert(QStringLiteral("lateFramesObserved"), result.lateFramesObserved);
    object.insert(QStringLiteral("repeatedFramesObserved"), result.repeatedFramesObserved);
    object.insert(QStringLiteral("frameTimingSampleCount"), result.frameTimingSampleCount);
    object.insert(QStringLiteral("minimumOnTimePercent"), result.minimumOnTimePercent);
    object.insert(QStringLiteral("layerSources"), result.layerSources);
    return object;
}

void sampleVideoTimingDiagnostics(Artifact::ArtifactAbstractComposition* composition,
                                 CaseResult& result)
{
    static const QRegularExpression onTimePattern(QStringLiteral("onTimePct=([0-9]+(?:\\.[0-9]+)?)"));
    static const QRegularExpression latePattern(QStringLiteral("cumulativeLateFrames=([0-9]+)"));
    static const QRegularExpression repeatedPattern(QStringLiteral("cumulativeRepeatedFrames=([0-9]+)"));
    static const QRegularExpression windowPattern(QStringLiteral("windowFrames=([0-9]+)"));
    if (!composition) return;
    for (const auto& layer : composition->allLayer()) {
        auto* video = dynamic_cast<Artifact::ArtifactVideoLayer*>(layer.get());
        if (!video) continue;
        const QString diagnostics = video->decodeState();
        const auto windowMatch = windowPattern.match(diagnostics);
        const auto lateMatch = latePattern.match(diagnostics);
        const auto repeatedMatch = repeatedPattern.match(diagnostics);
        if (windowMatch.hasMatch() && windowMatch.captured(1).toInt() > 0 &&
            lateMatch.hasMatch() && repeatedMatch.hasMatch()) {
            ++result.frameTimingSampleCount;
        }
        const auto onTimeMatch = onTimePattern.match(diagnostics);
        if (onTimeMatch.hasMatch()) {
            result.minimumOnTimePercent = std::min(
                result.minimumOnTimePercent, onTimeMatch.captured(1).toDouble());
        }
        if (lateMatch.hasMatch()) {
            result.lateFramesObserved = std::max(
                result.lateFramesObserved, static_cast<int>(std::min<qulonglong>(
                    lateMatch.captured(1).toULongLong(), 2147483647ULL)));
        }
        if (repeatedMatch.hasMatch()) {
            result.repeatedFramesObserved = std::max(
                result.repeatedFramesObserved, static_cast<int>(std::min<qulonglong>(
                    repeatedMatch.captured(1).toULongLong(), 2147483647ULL)));
        }
    }
}

void sampleRendererTiming(Artifact::ArtifactCompositionEditor* viewport,
                          qint64 elapsedMs, CaseResult& result)
{
    if (!viewport) return;
    auto* controller = viewport->renderController();
    if (!controller) return;
    static const bool detailedTiming =
        qEnvironmentVariableIntValue("ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP") != 0;
    // Ordinary acceptance reads published counters. The detailed snapshot
    // walks resources and builds diagnostic strings on the GUI thread.
    const auto snapshot = detailedTiming ? controller->frameDebugSnapshot(false)
                                        : controller->frameDebugCounters();
    QJsonObject sample;
    sample.insert(QStringLiteral("elapsedMs"), elapsedMs);
    sample.insert(QStringLiteral("renderLastFrameMs"), snapshot.renderLastFrameMs);
    sample.insert(QStringLiteral("renderAverageFrameMs"), snapshot.renderAverageFrameMs);
    sample.insert(QStringLiteral("renderGpuFrameMs"), snapshot.renderGpuFrameMs);
    sample.insert(QStringLiteral("renderGpuTimingAvailable"),
                   snapshot.renderGpuTimingAvailable);
    sample.insert(QStringLiteral("drawCalls"),
                   static_cast<double>(snapshot.renderCost.drawCalls));
    sample.insert(QStringLiteral("indexedDrawCalls"),
                   static_cast<double>(snapshot.renderCost.indexedDrawCalls));
    QJsonArray passes;
    for (const auto& pass : snapshot.passes) {
        QJsonObject passSample;
        passSample.insert(QStringLiteral("name"), pass.name);
        passSample.insert(QStringLiteral("durationUs"),
                          static_cast<double>(pass.durationUs));
        passSample.insert(QStringLiteral("status"),
                          static_cast<int>(pass.status));
        if (!pass.note.isEmpty()) {
            passSample.insert(QStringLiteral("note"), pass.note);
        }
        passes.append(passSample);
    }
    sample.insert(QStringLiteral("passes"), passes);
    result.renderTimingSamples.append(sample);
}

bool waitForRamPreview(Artifact::ArtifactPlaybackService* playback,
                       Artifact::ArtifactCompositionEditor* viewport,
                       int timeoutMs,
                       bool waitForDiskCache,
                       CaseResult& result)
{
    QElapsedTimer timer;
    timer.start();
    if (viewport && viewport->renderController()) {
        // The preceding case stops rendering as well as transport. Cache
        // preparation needs the renderer running while transport stays stopped.
        viewport->renderController()->start();
    }
    const auto buildRange = playback->ramPreviewRange();
    int64_t drivenFrame = -1;
    qint64 nextRetryMs = 0;
    qint64 nextDiagnosticMs = 0;
    while (timer.elapsed() < timeoutMs) {
        const int64_t target = playback->nextRamPreviewBuildFrame();
        if (target >= 0 && (target != drivenFrame || timer.elapsed() >= nextRetryMs)) {
            if (target != drivenFrame) {
                // Prepare cached frames through the same transport owner as
                // playback. Restore the requested build range after seek's
                // normal near-playhead prewarm policy.
                playback->setCurrentFrame(ArtifactCore::FramePosition(target));
                playback->setRamPreviewRange(buildRange);
                playback->requestRamPreviewBuild(buildRange, QStringLiteral("video-preview-bench"));
                drivenFrame = target;
            }
            if (viewport && viewport->renderController()) {
                viewport->renderController()->markRenderDirty();
            }
            nextRetryMs = timer.elapsed() + 50;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        const auto summary = playback->ramPreviewSummary();
        if (qEnvironmentVariableIntValue("ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP") != 0 &&
            timer.elapsed() >= nextDiagnosticMs) {
            const auto frameState = playback->ramPreviewFrameState(summary.buildQueueNextFrame);
            std::cerr << "[VideoPreviewBuild] next=" << summary.buildQueueNextFrame
                      << " active=" << summary.buildQueueActive
                      << " generation=" << summary.buildQueueGeneration
                      << " queueReason=" << summary.buildQueueReason.toStdString()
                      << " current=" << playback->currentFrame().framePosition()
                      << " ready=" << summary.readyFrames
                      << " reason=" << frameState.reason.toStdString() << "\n";
            nextDiagnosticMs = timer.elapsed() + 1000;
        }
        result.readyPreviewFrames = summary.readyFrames;
        result.requestedPreviewFrames = summary.requestedFrames;
        result.inRamPreviewFrames = summary.inRamFrames;
        result.onDiskPreviewFrames = summary.onDiskFrames;
        result.previewCacheHitRate = summary.hitRate;
        const bool ramReady = summary.buildRangeReady ||
            (summary.rangeFrames > 0 && summary.readyFrames >= summary.rangeFrames);
        const bool diskReady = !waitForDiskCache ||
            (summary.rangeFrames > 0 && summary.onDiskFrames >= summary.rangeFrames);
        if (ramReady && diskReady) {
            return true;
        }
        if (!summary.buildQueueActive && summary.buildQueuePendingFrames == 0 &&
            summary.failedFrames > 0 && summary.readyFrames == 0) {
            result.error = QStringLiteral("RAM preview build failed before any frame became ready");
            return false;
        }
        QThread::msleep(5);
    }
    const auto summary = playback->ramPreviewSummary();
    result.readyPreviewFrames = summary.readyFrames;
    result.requestedPreviewFrames = summary.requestedFrames;
    result.inRamPreviewFrames = summary.inRamFrames;
    result.onDiskPreviewFrames = summary.onDiskFrames;
    result.previewCacheHitRate = summary.hitRate;
    result.error = QStringLiteral("Timed out waiting for the configured RAM preview range; ready %1 of %2")
                       .arg(summary.readyFrames)
                       .arg(summary.rangeFrames);
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument == QStringLiteral("--ffmpeg-hwaccel") && i + 1 < argc) {
            qputenv("ARTIFACT_VIDEO_PREVIEW_FFMPEG_HWACCEL",
                    QByteArray(argv[i + 1]));
            break;
        }
        constexpr auto prefix = "--ffmpeg-hwaccel=";
        if (argument.startsWith(QLatin1String(prefix))) {
            qputenv("ARTIFACT_VIDEO_PREVIEW_FFMPEG_HWACCEL",
                    argument.mid(static_cast<qsizetype>(sizeof(prefix) - 1)).toUtf8());
            break;
        }
    }
    static QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactVideoPreviewBench"));
    QStandardPaths::setTestModeEnabled(true);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Measure real-time Composition preview for video-only layer stacks."));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("final-only-diagnostics"), QStringLiteral("Collect cumulative video timing only after measurement; omit periodic diagnostic formatting.")});
    parser.addOption({QStringLiteral("input"), QStringLiteral("Video source; repeat to cycle independent sources."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("layer-counts"), QStringLiteral("Comma-separated layer counts (default: 1 through 8)."), QStringLiteral("counts"), QStringLiteral("1,2,3,4,5,6,7,8")});
    parser.addOption({QStringLiteral("seconds"), QStringLiteral("Measured playback duration per case."), QStringLiteral("seconds"), QStringLiteral("3")});
    parser.addOption({QStringLiteral("capture-final-frame"), QStringLiteral("Save the current GPU target after measurement; requires --report. Presented-frame identity and physical scanout are unverified.")});
    parser.addOption({QStringLiteral("loop-window-seconds"), QStringLiteral("Loop a bounded source/cache window during the measured duration."), QStringLiteral("seconds")});
    parser.addOption({QStringLiteral("composition-width"), QStringLiteral("Composition render width in pixels."), QStringLiteral("pixels"), QStringLiteral("1280")});
    parser.addOption({QStringLiteral("composition-height"), QStringLiteral("Composition render height in pixels."), QStringLiteral("pixels"), QStringLiteral("720")});
    parser.addOption({QStringLiteral("ram-preview-timeout"), QStringLiteral("Maximum seconds to wait for full RAM preview cache."), QStringLiteral("seconds"), QStringLiteral("180")});
    parser.addOption({QStringLiteral("report"), QStringLiteral("JSON report output path."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("ram-warmup-frames"), QStringLiteral("Prepare only an initial RAM prefix, then measure the full source range with cache enabled."), QStringLiteral("frames")});
    parser.addOption({QStringLiteral("no-ram-cache"), QStringLiteral("Disable RAM preview cache; default uses a fully built RAM preview when available." )});
    parser.addOption({QStringLiteral("no-disk-cache"), QStringLiteral("Disable disk persistence to isolate RAM-only preview; RAM cache policy is unchanged.")});
    parser.addOption({QStringLiteral("wait-disk-cache"), QStringLiteral("Wait for all requested disk cache frames before measurement; requires RAM and disk cache.")});
    parser.addOption({QStringLiteral("measurement-loop"), QStringLiteral("Measurement event dispatch: polling (baseline) or qt (native event loop)."), QStringLiteral("mode"), QStringLiteral("polling")});
    parser.addOption({QStringLiteral("ffmpeg-hwaccel"), QStringLiteral("Opt-in FFmpeg decoder hardware backend (d3d11va, d3d12va, or vulkan). Hardware frames download to CPU for presentation."), QStringLiteral("backend")});
    parser.addOption({QStringLiteral("probe-shared-d3d12-device"), QStringLiteral("Decode and convert eight H.264 frames on the active Composition renderer's D3D12 device and Diligent queue.")});
    parser.process(app);
    if (parser.isSet(QStringLiteral("capture-final-frame")) &&
        parser.value(QStringLiteral("report")).isEmpty()) {
        std::cerr << "--capture-final-frame requires --report.\n";
        return 2;
    }

    const QString measurementLoopMode = parser.value(QStringLiteral("measurement-loop"));
    if (measurementLoopMode != QStringLiteral("polling") &&
        measurementLoopMode != QStringLiteral("qt")) {
        std::cerr << "--measurement-loop must be polling or qt.\n";
        return 2;
    }
    if (parser.isSet(QStringLiteral("wait-disk-cache")) &&
        (parser.isSet(QStringLiteral("no-disk-cache")) ||
         parser.isSet(QStringLiteral("no-ram-cache")))) {
        std::cerr << "--wait-disk-cache requires both RAM and disk cache.\n";
        return 2;
    }

    const QString ffmpegHwAccel = parser.value(QStringLiteral("ffmpeg-hwaccel")).trimmed();
    if (!ffmpegHwAccel.isEmpty()) {
        std::cerr << "FFmpeg hardware decode requested: "
                  << ffmpegHwAccel.toStdString()
                  << " env="
                  << qgetenv("ARTIFACT_VIDEO_PREVIEW_FFMPEG_HWACCEL").constData()
                  << "\n";
    }

    const QStringList inputs = parser.values(QStringLiteral("input"));
    if (inputs.isEmpty()) {
        std::cerr << "At least one --input video file is required.\n";
        return 2;
    }
    for (const QString& path : inputs) {
        if (!QFileInfo::exists(path)) {
            std::cerr << "Input video does not exist: " << path.toStdString() << "\n";
            return 2;
        }
    }

    bool secondsOk = false;
    bool widthOk = false;
    bool heightOk = false;
    const int compositionWidth = parser.value(QStringLiteral("composition-width")).toInt(&widthOk);
    const int compositionHeight = parser.value(QStringLiteral("composition-height")).toInt(&heightOk);
    if (!widthOk || !heightOk || compositionWidth < 1 || compositionHeight < 1 ||
        compositionWidth > 8192 || compositionHeight > 8192) {
        std::cerr << "Composition width and height must be integers in 1..8192.\n";
        return 2;
    }
    const double measuredSeconds = parser.value(QStringLiteral("seconds")).toDouble(&secondsOk);
    if (measurementLoopMode == QStringLiteral("qt") && measuredSeconds > 2147483.0) {
        std::cerr << "Qt measurement duration exceeds the timer's integer millisecond range.\n";
        return 2;
    }
    if (!secondsOk || !std::isfinite(measuredSeconds) || measuredSeconds < 1.0) {
        std::cerr << "--seconds must be a finite value >= 1.\n";
        return 2;
    }
    bool cacheTimeoutOk = false;
    const bool loopMeasurement = parser.isSet(QStringLiteral("loop-window-seconds"));
    bool windowOk = false;
    const double windowSeconds = loopMeasurement
        ? parser.value(QStringLiteral("loop-window-seconds")).toDouble(&windowOk)
        : measuredSeconds;
    if (loopMeasurement && (!windowOk || !std::isfinite(windowSeconds) ||
        windowSeconds < 1.0 || windowSeconds > measuredSeconds ||
        std::ceil(windowSeconds * 30.0) > 128.0)) {
        std::cerr << "--loop-window-seconds must be >= 1, <= measured seconds, and at most 128 frames at 30fps.\n";
        return 2;
    }
    int ramWarmupFrames = 0;
    if (parser.isSet(QStringLiteral("ram-warmup-frames"))) {
        bool warmupOk = false;
        ramWarmupFrames = parser.value(QStringLiteral("ram-warmup-frames")).toInt(&warmupOk);
        if (!warmupOk || ramWarmupFrames < 8 || ramWarmupFrames > 128 ||
            ramWarmupFrames > std::ceil(windowSeconds * 30.0) ||
            parser.isSet(QStringLiteral("no-ram-cache"))) {
            std::cerr << "--ram-warmup-frames requires cache and 8..128 frames within the source range.\n";
            return 2;
        }
    }
    const int ramPreviewTimeoutMs = static_cast<int>(
        parser.value(QStringLiteral("ram-preview-timeout")).toDouble(&cacheTimeoutOk) * 1000.0);
    if (!cacheTimeoutOk || ramPreviewTimeoutMs < 1000) {
        std::cerr << "--ram-preview-timeout must be >= 1 second.\n";
        return 2;
    }

    std::vector<int> layerCounts;
    for (const QString& part : parser.value(QStringLiteral("layer-counts")).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int count = part.trimmed().toInt(&ok);
        if (!ok || count <= 0 || count > 64) {
            std::cerr << "--layer-counts values must be in 1..64.\n";
            return 2;
        }
        layerCounts.push_back(count);
    }
    if (layerCounts.empty()) {
        std::cerr << "--layer-counts must contain at least one value.\n";
        return 2;
    }
    std::sort(layerCounts.begin(), layerCounts.end());
    layerCounts.erase(std::unique(layerCounts.begin(), layerCounts.end()), layerCounts.end());

    QTemporaryDir isolatedData;
    if (!isolatedData.isValid()) {
        std::cerr << "Could not create isolated benchmark data directory.\n";
        return 3;
    }
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", isolatedData.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactVideoPreviewBench"));

    // Set the isolated project root before constructing application singletons;
    // their constructors may resolve or create project state immediately.
    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    Artifact::ArtifactApplicationManager::instance();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("VideoPreviewBench"), true);
    if (!projects.isProjectCreated()) {
        std::cerr << "Could not create isolated benchmark project.\n";
        return 3;
    }

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(ArtifactCore::UniString(QStringLiteral("Video Preview Bench")));
    compositionParams.setResolution(compositionWidth, compositionHeight);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationSeconds(windowSeconds);
    const auto compositionResult = projects.createComposition(compositionParams);
    if (!compositionResult.success) {
        std::cerr << "Could not create benchmark composition.\n";
        return 3;
    }
    auto composition = projects.findComposition(compositionResult.id).ptr.lock();
    if (!composition) {
        std::cerr << "Benchmark composition could not be resolved.\n";
        return 3;
    }

    auto* playback = Artifact::ArtifactPlaybackService::instance();
    playback->setDiskPreviewCacheEnabled(!parser.isSet(QStringLiteral("no-disk-cache")));
    playback->setRamPreviewPlaybackFallbackWhilePlaying(true);
    if (!parser.isSet(QStringLiteral("no-ram-cache"))) {
        playback->setRamPreviewEnabled(true);
    } else {
        playback->setRamPreviewEnabled(false);
    }

    QMainWindow window;
    auto* viewport = new Artifact::ArtifactCompositionEditor(&window);
    viewport->resize(compositionWidth, compositionHeight);
    window.setCentralWidget(viewport);
    window.resize(compositionWidth, compositionHeight);
    window.setWindowTitle(QStringLiteral("Artifact Video Preview Bench"));
    window.show();
    viewport->setComposition(composition);
    pumpFor(1000);

    if (parser.isSet(QStringLiteral("probe-shared-d3d12-device"))) {
        const int probeResult = runSharedD3D12DecodeProbe(inputs.front());
        window.close();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        projects.closeCurrentProject();
        return probeResult;
    }

    QJsonArray caseArray;
    int overallExit = 0;
    const auto saveCaseCheckpoint = [&]() {
        const QString reportPath = parser.value(QStringLiteral("report"));
        if (reportPath.isEmpty()) return;
        QJsonObject checkpoint;
        checkpoint.insert(QStringLiteral("schema"), QStringLiteral("artifact-video-preview-checkpoint-v1"));
        checkpoint.insert(QStringLiteral("complete"), false);
        checkpoint.insert(QStringLiteral("expectedCaseCount"), static_cast<int>(layerCounts.size()));
        checkpoint.insert(QStringLiteral("cases"), caseArray);
        checkpoint.insert(QStringLiteral("measurementLoop"), measurementLoopMode);
        checkpoint.insert(QStringLiteral("requestedSeconds"), measuredSeconds);
        checkpoint.insert(QStringLiteral("diagnosticSamplingPolicy"),
            parser.isSet(QStringLiteral("final-only-diagnostics"))
                ? QStringLiteral("final-only") : QStringLiteral("periodic-250ms-and-final"));
        checkpoint.insert(QStringLiteral("sourceWindowSeconds"), windowSeconds);
        checkpoint.insert(QStringLiteral("ramWarmupFrames"), ramWarmupFrames);
        checkpoint.insert(QStringLiteral("loopMeasurement"), loopMeasurement);
        checkpoint.insert(QStringLiteral("cacheEnabled"), !parser.isSet(QStringLiteral("no-ram-cache")));
        checkpoint.insert(QStringLiteral("diskCacheEnabled"), playback->isDiskPreviewCacheEnabled());
        checkpoint.insert(QStringLiteral("waitForDiskCache"), parser.isSet(QStringLiteral("wait-disk-cache")));
        checkpoint.insert(QStringLiteral("rendererBackendOverride"), qEnvironmentVariable("ARTIFACT_RENDER_BACKEND"));
        QJsonArray checkpointInputs;
        for (const auto& input : inputs) checkpointInputs.append(QFileInfo(input).absoluteFilePath());
        checkpoint.insert(QStringLiteral("inputs"), checkpointInputs);
        checkpoint.insert(QStringLiteral("compositionWidth"), compositionWidth);
        checkpoint.insert(QStringLiteral("compositionHeight"), compositionHeight);
        checkpoint.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        const QByteArray data = QJsonDocument(checkpoint).toJson(QJsonDocument::Indented);
        QDir().mkpath(QFileInfo(reportPath).absolutePath());
        QSaveFile file(reportPath + QStringLiteral(".partial.json"));
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
            std::cerr << "Could not save incomplete case checkpoint.\n";
            overallExit = 1;
        }
    };
    for (const int layerCount : layerCounts) {
        std::cerr << "[VideoPreviewCase] begin layers=" << layerCount
                  << " composition=" << compositionWidth << "x" << compositionHeight
                  << " seconds=" << measuredSeconds << "\n";
        CaseResult result;
        result.layerCount = layerCount;
        result.measuredSeconds = measuredSeconds;

        playback->stop();
        playback->waitForStop();
        playback->setCurrentFrame(ArtifactCore::FramePosition(0));
        pumpFor(50);
        composition->removeAllLayers();
        playback->clearRamPreviewCache();

        Artifact::ArtifactVideoLayer* firstVideo = nullptr;
        for (int index = 0; index < layerCount; ++index) {
            const QString source = inputs[index % inputs.size()];
            Artifact::ArtifactVideoInitParams params(
                QStringLiteral("Video %1").arg(index + 1));
            params.setVideoPath(source);
            auto layerResult = projects.addLayerToComposition(composition->id(), params);
            auto* video = layerResult.success
                ? dynamic_cast<Artifact::ArtifactVideoLayer*>(layerResult.layer.get())
                : nullptr;
            if (!video) {
                result.error = QStringLiteral("Could not create video layer %1 from %2")
                                   .arg(index + 1).arg(source);
                break;
            }
            QElapsedTimer openTimer;
            openTimer.start();
            while (!video->isLoaded() && openTimer.elapsed() < 30000) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                QThread::msleep(5);
            }
            if (!video->isLoaded()) {
                result.error = QStringLiteral("Video layer %1 failed to open %2: %3")
                                   .arg(index + 1).arg(source).arg(video->debugState());
                break;
            }
            QJsonObject sourceMetadata;
            sourceMetadata.insert(QStringLiteral("layer"), index + 1);
            sourceMetadata.insert(QStringLiteral("path"), QFileInfo(source).absoluteFilePath());
            sourceMetadata.insert(QStringLiteral("codec"), video->streamInfo().codecName);
            sourceMetadata.insert(QStringLiteral("sourceFps"), video->streamInfo().frameRate);
            sourceMetadata.insert(QStringLiteral("width"), video->streamInfo().width);
            sourceMetadata.insert(QStringLiteral("height"), video->streamInfo().height);
            sourceMetadata.insert(QStringLiteral("bitRate"), video->streamInfo().bitRate);
            sourceMetadata.insert(QStringLiteral("durationSeconds"), video->streamInfo().duration);
            result.layerSources.append(sourceMetadata);
            if (!std::isfinite(video->streamInfo().duration) ||
                video->streamInfo().duration < windowSeconds) {
                result.error = QStringLiteral("Input duration for layer %1 is shorter than the measured playback window")
                                   .arg(index + 1);
                break;
            }
            if (!firstVideo) {
                firstVideo = video;
                result.codec = video->streamInfo().codecName;
                result.sourceFps = video->streamInfo().frameRate;
                result.requestedFps = 30.0;
            }
        }

        if (!result.error.isEmpty() || !firstVideo) {
            result.status = QStringLiteral("setup_failed");
            overallExit = 1;
            caseArray.append(resultToJson(result));
            saveCaseCheckpoint();
            break;
        }

        const int fps = 30;
        const int frameCount = std::max(2, static_cast<int>(std::ceil(windowSeconds * fps)));
        result.requestedStart = 0;
        result.requestedEnd = frameCount - 1;
        const ArtifactCore::FrameRate rate(30.0f);
        playback->setFrameRate(rate);
        playback->setFrameRange(ArtifactCore::FrameRange(0, frameCount));
        playback->setLooping(loopMeasurement);
        playback->setPlaybackSkipMode(Artifact::PlaybackSkipMode::None);
        playback->setCurrentComposition(composition);
        playback->setCurrentFrame(ArtifactCore::FramePosition(0));

        if (!parser.isSet(QStringLiteral("no-ram-cache"))) {
            // The warmup seeks through this whole range. Its normal prewarm
            // radius must retain frames prepared earlier in the same build.
            playback->setRamPreviewRadius(frameCount);
            const ArtifactCore::FrameRange range(0, ramWarmupFrames > 0 ? ramWarmupFrames : frameCount);
            playback->setRamPreviewRange(range);
            playback->requestRamPreviewBuild(range, QStringLiteral("video-preview-bench"));
            if (!waitForRamPreview(playback, viewport, ramPreviewTimeoutMs,
                                   parser.isSet(QStringLiteral("wait-disk-cache")), result)) {
                for (const auto& layer : composition->allLayer()) {
                    if (auto* video = dynamic_cast<Artifact::ArtifactVideoLayer*>(layer.get())) {
                        result.videoDiagnostics += video->debugState() + QStringLiteral("\n") +
                                                   video->decodeState() + QStringLiteral("\n");
                    }
                }
                result.status = QStringLiteral("ram_preview_not_ready");
                overallExit = 1;
                caseArray.append(resultToJson(result));
                saveCaseCheckpoint();
                break;
            }
        }

        playback->setCurrentFrame(ArtifactCore::FramePosition(0));
        pumpFor(100);
        if (ramWarmupFrames > 0) {
            const ArtifactCore::FrameRange measuredRange(0, frameCount);
            playback->setRamPreviewRange(measuredRange);
            playback->requestRamPreviewBuild(measuredRange, QStringLiteral("video-preview-bench-streaming"));
        }
        if (const auto* cached = playback->borrowRamPreviewFrameImage(0)) {
            result.cachedImageWidth = cached->width();
            result.cachedImageHeight = cached->height();
        }
        for (const auto& layer : composition->allLayer()) {
            if (auto* video = dynamic_cast<Artifact::ArtifactVideoLayer*>(layer.get())) {
                video->resetFrameTimingDiagnostics();
            }
        }
        const int64_t droppedBefore = playback->droppedFrameCount();
        const auto preparedCacheSummary = playback->ramPreviewSummary();
        result.preparedRamPreviewFrames = preparedCacheSummary.inRamFrames;
        result.preparedDiskPreviewFrames = preparedCacheSummary.onDiskFrames;
        const auto* measuredController = viewport->renderController();
        const quint64 renderSerialBefore = measuredController
            ? measuredController->currentFrameSerial() : 0;
        QElapsedTimer playbackTimer;
        std::cerr << "[VideoPreviewCase] measure layers=" << layerCount << "\n";
        playbackTimer.start();
        viewport->play();
        result.playbackStarted = playback->isPlaying();
        auto sampleMeasurement = [&]() {
                if (parser.isSet(QStringLiteral("final-only-diagnostics"))) return;
                const qint64 sampleAtMs = playbackTimer.elapsed();
                QElapsedTimer sampleTimer;
                sampleTimer.start();
                sampleVideoTimingDiagnostics(composition.get(), result);
                const auto videoSampleNs = sampleTimer.nsecsElapsed();
                sampleRendererTiming(viewport, playbackTimer.elapsed(), result);
                const double diagnosticSampleMs = sampleTimer.nsecsElapsed() / 1.0e6;
                result.maximumVideoDiagnosticSampleMs = std::max(
                    result.maximumVideoDiagnosticSampleMs, videoSampleNs / 1.0e6);
                if (diagnosticSampleMs > result.maximumDiagnosticSampleMs) {
                    result.maximumDiagnosticSampleMs = diagnosticSampleMs;
                    result.maximumDiagnosticSampleAtMs = sampleAtMs;
                }
                if (qEnvironmentVariableIntValue("ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP") != 0) {
                    std::cerr << "[VideoPreviewSample] videoMs=" << videoSampleNs / 1.0e6
                              << " rendererMs=" << (sampleTimer.nsecsElapsed() - videoSampleNs) / 1.0e6 << "\n";
                }
        };
        if (measurementLoopMode == QStringLiteral("qt")) {
            sampleMeasurement();
            PreviewMeasurementLoop loop(sampleMeasurement);
            loop.run(static_cast<int>(std::max<qint64>(
                0, static_cast<qint64>(measuredSeconds * 1000.0) - playbackTimer.elapsed())));
        } else {
            qint64 nextTimingSample = 0;
            while (playbackTimer.elapsed() < static_cast<qint64>(measuredSeconds * 1000.0)) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                if (playbackTimer.elapsed() >= nextTimingSample) {
                    sampleMeasurement();
                    nextTimingSample = playbackTimer.elapsed() + 250;
                }
                QThread::msleep(2);
            }
        }
        result.measuredSeconds = playbackTimer.elapsed() / 1000.0;
        // Include outcomes after the last periodic sample before judging the
        // case; otherwise the final quarter-second can hide late/repeated frames.
        sampleVideoTimingDiagnostics(composition.get(), result);
        result.finalFrame = playback->currentFrame().framePosition();
        result.finalPresentedFrame = playback->lastObservedPresentedFrame();
        result.observedPresentedFrames = playback->observedPresentedFrameCount();
        result.renderCycles = measuredController
            ? measuredController->currentFrameSerial() - renderSerialBefore : 0;
        result.playbackStillPlaying = playback->isPlaying();
        result.droppedFrames = playback->droppedFrameCount() - droppedBefore;
        result.videoDiagnostics = firstVideo->debugState() + QStringLiteral("\n") +
                                 firstVideo->decodeState(true);
        // Serialize all layer outcomes only after measurement has ended.
        int timingLayerIndex = 0;
        for (const auto& layer : composition->allLayer()) {
            ++timingLayerIndex;
            if (auto* video = dynamic_cast<Artifact::ArtifactVideoLayer*>(layer.get())) {
                QJsonObject timing;
                timing.insert(QStringLiteral("layer"), timingLayerIndex);
                timing.insert(QStringLiteral("layerId"), video->id().toString());
                timing.insert(QStringLiteral("sourcePath"), video->sourcePath());
                timing.insert(QStringLiteral("diagnostics"), video->decodeState(true));
                result.videoTimingByLayer.append(timing);
            }
        }
        viewport->stop();
        playback->waitForStop();
        const auto cacheSummary = playback->ramPreviewSummary();
        result.readyPreviewFrames = cacheSummary.readyFrames;
        result.requestedPreviewFrames = cacheSummary.requestedFrames;
        result.inRamPreviewFrames = cacheSummary.inRamFrames;
        result.onDiskPreviewFrames = cacheSummary.onDiskFrames;
        result.previewCacheHitRate = cacheSummary.hitRate;
        result.passed = result.droppedFrames == 0 &&
                        result.lateFramesObserved == 0 &&
                        result.repeatedFramesObserved == 0 &&
                        result.frameTimingSampleCount > 0 &&
                        (loopMeasurement
                            ? result.playbackStillPlaying &&
                              result.observedPresentedFrames >=
                                  static_cast<int64_t>(std::floor(measuredSeconds * fps)) - 2
                            : result.finalFrame >= frameCount - 2 &&
                              result.finalPresentedFrame >= frameCount - 2);
        result.status = result.passed ? QStringLiteral("realtime_no_drops")
                                      : QStringLiteral("realtime_failed");
        if (result.droppedFrames > 0) {
            result.error = QStringLiteral("Presented frame sequence skipped %1 frame(s)")
                               .arg(result.droppedFrames);
        } else if (result.lateFramesObserved > 0 ||
                   result.repeatedFramesObserved > 0) {
            result.error = QStringLiteral("Video timing diagnostics observed late=%1, repeated=%2 frame(s)")
                               .arg(result.lateFramesObserved)
                               .arg(result.repeatedFramesObserved);
        } else if (result.frameTimingSampleCount == 0) {
            result.error = QStringLiteral("No video frame timing diagnostics were observed during playback");
        } else if (!result.passed) {
            result.error = QStringLiteral("Playback did not reach the expected realtime frame range");
        }
        QJsonObject completedCase = resultToJson(result);
        if (parser.isSet(QStringLiteral("capture-final-frame"))) {
            const QFileInfo reportInfo(parser.value(QStringLiteral("report")));
            const QString capturePath = reportInfo.absolutePath() + QLatin1Char('/') +
                reportInfo.completeBaseName() + QStringLiteral("-layers-%1-gpu.png").arg(layerCount);
            QJsonObject capture;
            capture.insert(QStringLiteral("path"), capturePath);
            capture.insert(QStringLiteral("source"), QStringLiteral("renderer-readback-after-stop"));
            capture.insert(QStringLiteral("physicalScanoutVerified"), false);
            capture.insert(QStringLiteral("presentedFrameIdentityVerified"), false);
            capture.insert(QStringLiteral("mayReadRotatedBackBuffer"), true);
            bool saved = false;
            if (!QFileInfo::exists(capturePath) && viewport->renderController() &&
                viewport->renderController()->renderer()) {
                // Explicit cold diagnostic boundary, outside all timing samples.
                const QImage image = viewport->renderController()->renderer()->readbackToImage();
                capture.insert(QStringLiteral("width"), image.width());
                capture.insert(QStringLiteral("height"), image.height());
                QDir().mkpath(reportInfo.absolutePath());
                saved = !image.isNull() && image.save(capturePath, "PNG");
            }
            capture.insert(QStringLiteral("saved"), saved);
            completedCase.insert(QStringLiteral("postMeasurementCapture"), capture);
            if (!saved) overallExit = 1;
        }
        caseArray.append(completedCase);
        saveCaseCheckpoint();
        std::cerr << "[VideoPreviewCase] end layers=" << layerCount
                  << " status=" << result.status.toStdString()
                  << " transportFrame=" << result.finalFrame
                  << " presentedFrame=" << result.finalPresentedFrame
                  << " dropped=" << result.droppedFrames
                  << " late=" << result.lateFramesObserved
                  << " repeated=" << result.repeatedFramesObserved << "\n";
        if (!result.passed) {
            overallExit = 1;
            continue;
        }
    }

    QJsonObject report;
    report.insert(QStringLiteral("schema"), QStringLiteral("artifact-video-preview-bench-v1"));
    report.insert(QStringLiteral("postMeasurementCaptureRequested"),
                  parser.isSet(QStringLiteral("capture-final-frame")));
    report.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    report.insert(QStringLiteral("host"), QSysInfo::prettyProductName());
    report.insert(QStringLiteral("cpuArchitecture"), QSysInfo::currentCpuArchitecture());
    report.insert(QStringLiteral("qtVersion"), QString::fromLatin1(qVersion()));
    report.insert(QStringLiteral("compositionWidth"), compositionWidth);
    report.insert(QStringLiteral("compositionHeight"), compositionHeight);
    report.insert(QStringLiteral("compositionFps"), 30.0);
    report.insert(QStringLiteral("viewportWidth"), viewport->width());
    report.insert(QStringLiteral("viewportHeight"), viewport->height());
    report.insert(QStringLiteral("viewportDevicePixelRatio"), viewport->devicePixelRatioF());
    report.insert(QStringLiteral("rendererBackendOverride"), qEnvironmentVariable("ARTIFACT_RENDER_BACKEND"));
    report.insert(QStringLiteral("cacheEnabled"), !parser.isSet(QStringLiteral("no-ram-cache")));
    report.insert(QStringLiteral("loopMeasurement"), loopMeasurement);
    report.insert(QStringLiteral("sourceWindowSeconds"), windowSeconds);
    report.insert(QStringLiteral("ramWarmupFrames"), ramWarmupFrames);
    report.insert(QStringLiteral("diskCacheEnabled"), playback->isDiskPreviewCacheEnabled());
    report.insert(QStringLiteral("waitForDiskCache"), parser.isSet(QStringLiteral("wait-disk-cache")));
    report.insert(QStringLiteral("measurementLoop"), measurementLoopMode);
    report.insert(QStringLiteral("diagnosticSamplingPolicy"),
                  parser.isSet(QStringLiteral("final-only-diagnostics"))
                      ? QStringLiteral("final-only") : QStringLiteral("periodic-250ms-and-final"));
    report.insert(QStringLiteral("cachePolicy"), parser.isSet(QStringLiteral("no-ram-cache"))
        ? QStringLiteral("disabled") : ramWarmupFrames > 0
            ? QStringLiteral("initial RAM prefix required; full source range measured with cache")
            : QStringLiteral("full RAM preview required before measurement"));
    report.insert(QStringLiteral("condition"), QStringLiteral("zero dropped, late, or repeated frames and realtime frame-range progress"));
    report.insert(QStringLiteral("videoTimingCountPolicy"),
                  QStringLiteral("maximum per-layer cumulative count since measurement reset; saturated at INT32_MAX"));
    QJsonArray inputArray;
    for (const auto& input : inputs) inputArray.append(QFileInfo(input).absoluteFilePath());
    report.insert(QStringLiteral("inputs"), inputArray);
    report.insert(QStringLiteral("cases"), caseArray);

    const QByteArray json = QJsonDocument(report).toJson(QJsonDocument::Indented);
    const QString reportPath = parser.value(QStringLiteral("report"));
    if (!reportPath.isEmpty()) {
        const QFileInfo reportInfo(reportPath);
        if (!QDir().mkpath(reportInfo.absolutePath())) {
            std::cerr << "Could not create report directory: "
                      << reportInfo.absolutePath().toStdString() << "\n";
            overallExit = 1;
        }
        QFile output(reportPath);
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
            output.write(json) != json.size()) {
            std::cerr << "Could not write report: " << reportPath.toStdString() << "\n";
            overallExit = 1;
        }
    }
    std::cout << json.constData() << std::endl;
    viewport->stop();
    playback->waitForStop();
    window.close();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    // Join layer decode/upload work while both background pools are alive.
    // Leaving layers in singleton-owned compositions until static destruction
    // lets a decode worker submit upload work to an already destroyed pool.
    composition->removeAllLayers();
    projects.closeCurrentProject();
    return overallExit;
}
