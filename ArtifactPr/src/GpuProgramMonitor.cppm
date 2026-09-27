module;

#include <windows.h>

#include <cstring>
#include <exception>

#include <Buffer.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>
#include <Sampler.h>
#include <Shader.h>
#include <SwapChain.h>
#include <Texture.h>
#include <PipelineState.h>

#include <QString>
#include <QWidget>

export module ArtifactPr.GpuProgramMonitor;

import ArtifactPr.GpuProgramMonitor;

import Artifact.Render.Config;
import Artifact.Render.DiligentDeviceManager;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;

namespace ArtifactPr {

namespace {

// Full-screen triangle generated from the vertex id; no vertex buffer is
// required.  The uv mapping matches Artifact/shaders/globals.hlsli
// (vertexID_create_fullscreen_triangle) so the sampling orientation is
// identical to the rest of the renderer.
const char* const kProgramMonitorVS = R"(
struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut main(uint vI : SV_VERTEXID)
{
    VSOut Out;
    Out.pos.x = (float)(vI / 2) * 4.0 - 1.0;
    Out.pos.y = (float)(vI % 2) * 4.0 - 1.0;
    Out.pos.z = 0.0;
    Out.pos.w = 1.0;
    Out.uv.x  = (float)(vI / 2) * 2.0;
    Out.uv.y  = 1.0 - (float)(vI % 2) * 2.0;
    return Out;
}
)";

// The CPU path (ImageF32x4_RGBA::toQImage) feeds a descriptor produced by
// SurfaceColorDescriptor::unknownRgba32Float(), which leaves transferKnown
// false.  That routes through decodeLegacySrgbBoundary() (srgbToLinear) and
// then re-encodes with linearToSRGB.  The shader below reproduces exactly that
// round trip so the GPU preview stays pixel-comparable with the CPU fallback.
//
// g_view reproduces the CPU widget's Fit / zoom / pan placement in normalized
// device coordinates so both paths frame the image identically.
const char* const kProgramMonitorPS = R"(
Texture2D<float4> g_src : register(t0);
SamplerState      g_smp : register(s0);

cbuffer ViewCB : register(b0)
{
    float4 g_view; // xy = scale, zw = offset (NDC)
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float srgbToLinear(float srgb)
{
    srgb = max(srgb, 0.0);
    if (srgb <= 0.04045)
        return srgb / 12.92;
    return pow((srgb + 0.055) / 1.055, 2.4);
}

float linearToSRGB(float linear)
{
    linear = max(linear, 0.0);
    if (linear <= 0.0031308)
        return 12.92 * linear;
    return 1.055 * pow(linear, 1.0 / 2.4) - 0.055;
}

float decodeLegacySrgbBoundary(float value)
{
    if (value < 0.0 || value > 1.0)
        return value;
    return srgbToLinear(value);
}

float4 main(PSIn input) : SV_TARGET
{
    // Map the full-screen uv through the inverse view transform so the drawn
    // rectangle matches the CPU widget's zoom and pan.
    float2 centered = (input.uv * 2.0 - 1.0 - g_view.zw) / g_view.xy;
    float2 texUv = centered * 0.5 + 0.5;

    if (texUv.x < 0.0 || texUv.x > 1.0 || texUv.y < 0.0 || texUv.y > 1.0)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float4 texel = g_src.Sample(g_smp, texUv);

    // Mirror convertSurfacePixels(): clamp alpha, zero out fully transparent
    // texels, then apply the legacy sRGB boundary decode.
    float alpha = clamp(texel.a, 0.0, 1.0);
    float3 rgb = texel.rgb;
    if (alpha <= 1.0e-6)
    {
        rgb = float3(0.0, 0.0, 0.0);
    }
    rgb = float3(decodeLegacySrgbBoundary(rgb.r),
                decodeLegacySrgbBoundary(rgb.g),
                decodeLegacySrgbBoundary(rgb.b));

    return float4(linearToSRGB(rgb.r), linearToSRGB(rgb.g),
                  linearToSRGB(rgb.b), alpha);
}
)";

} // namespace

/// PImpl 実体。AGENTS.md の PImpl 規則に従い Impl* を明示所有し、
/// ヘッダ側に unique_ptr/shared_ptr を持たせない。
class GpuProgramMonitorRenderer::Impl {
public:
    Artifact::DiligentDeviceManager deviceManager;
    Diligent::RefCntAutoPtr<Diligent::IShader> shaderVs;
    Diligent::RefCntAutoPtr<Diligent::IShader> shaderPs;
    Diligent::RefCntAutoPtr<Diligent::IGraphicsPipelineState> pso;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> shaderResourceBinding;
    Diligent::RefCntAutoPtr<Diligent::ISampler> linearSampler;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> viewConstantBuffer;
    ArtifactCore::ImageF32x4RGBAWithCache frameCache;
    QString backendName;
    QString lastFailureReason;
    bool initialized = false;
    bool hasFrame = false;
    quint64 frameGeneration = 0;

    bool createPipeline(Diligent::IRenderDevice* device);
    void present(Diligent::ISwapChain* swapChain, Diligent::IDeviceContext* context);
};

bool GpuProgramMonitorRenderer::Impl::createPipeline(Diligent::IRenderDevice* device)
{
    if (!device) {
        return false;
    }

    Diligent::ShaderCreateInfo vsInfo;
    vsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    vsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    vsInfo.Desc.Name = "ArtifactPr Program Monitor VS";
    vsInfo.Source = kProgramMonitorVS;
    vsInfo.SourceLength = static_cast<Diligent::Uint32>(std::strlen(kProgramMonitorVS));
    device->CreateShader(vsInfo, &shaderVs);
    if (!shaderVs) {
        lastFailureReason = QStringLiteral("vertex shader creation failed");
        return false;
    }

    Diligent::ShaderCreateInfo psInfo;
    psInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    psInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    psInfo.Desc.Name = "ArtifactPr Program Monitor PS";
    psInfo.Source = kProgramMonitorPS;
    psInfo.SourceLength = static_cast<Diligent::Uint32>(std::strlen(kProgramMonitorPS));
    device->CreateShader(psInfo, &shaderPs);
    if (!shaderPs) {
        lastFailureReason = QStringLiteral("pixel shader creation failed");
        return false;
    }

    const Diligent::ShaderResourceVariableDesc variables[] = {
        { Diligent::SHADER_TYPE_PIXEL, "g_src", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { Diligent::SHADER_TYPE_PIXEL, "g_smp", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { Diligent::SHADER_TYPE_PIXEL, "ViewCB", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };

    Diligent::GraphicsPipelineStateCreateInfo psoInfo;
    psoInfo.PSODesc.Name = "ArtifactPr Program Monitor PSO";
    psoInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    psoInfo.pVS = shaderVs;
    psoInfo.pPS = shaderPs;
    auto& graphics = psoInfo.GraphicsPipeline;
    graphics.NumRenderTargets = 1;
    graphics.RTVFormats[0] = deviceManager.swapChain()
        ? deviceManager.swapChain()->GetDesc().ColorBufferFormat
        : Artifact::RenderConfig::MainRTVFormat;
    graphics.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    graphics.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
    graphics.DepthStencilDesc.DepthEnable = False;
    graphics.BlendDesc.RenderTargets[0].RenderTargetWriteMask = Diligent::COLOR_MASK_ALL;
    graphics.InputLayout.NumElements = 0;
    psoInfo.PSODesc.ResourceLayout.Variables = variables;
    psoInfo.PSODesc.ResourceLayout.NumVariables = _countof(variables);
    device->CreateGraphicsPipelineState(psoInfo, &pso);
    if (!pso) {
        lastFailureReason = QStringLiteral("pipeline state creation failed");
        return false;
    }

    pso->CreateShaderResourceBinding(&shaderResourceBinding, true);

    Diligent::BufferDesc viewBufferDesc;
    viewBufferDesc.Name = "ArtifactPr Program Monitor ViewCB";
    viewBufferDesc.Size = sizeof(float) * 4;
    viewBufferDesc.Usage = Diligent::USAGE_DYNAMIC;
    viewBufferDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
    viewBufferDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    device->CreateBuffer(viewBufferDesc, nullptr, &viewConstantBuffer);
    if (!viewConstantBuffer) {
        lastFailureReason = QStringLiteral("view constant buffer creation failed");
        return false;
    }

    Diligent::SamplerDesc samplerDesc;
    samplerDesc.Name = "ArtifactPr Program Monitor Sampler";
    device->CreateSampler(samplerDesc, &linearSampler);
    if (!linearSampler) {
        lastFailureReason = QStringLiteral("sampler creation failed");
        return false;
    }

    return true;
}

void GpuProgramMonitorRenderer::Impl::present(Diligent::ISwapChain* swapChain,
                                               Diligent::IDeviceContext* context)
{
    try {
        swapChain->Present();
        // Vulkan retires its dynamic upload allocations only on FinishFrame().
        context->FinishFrame();
    } catch (const std::exception& error) {
        lastFailureReason = QString::fromUtf8(error.what());
    }
}

GpuProgramMonitorRenderer::GpuProgramMonitorRenderer()
    : impl_(new Impl())
{
}

GpuProgramMonitorRenderer::~GpuProgramMonitorRenderer()
{
    delete impl_;
    impl_ = nullptr;
}

bool GpuProgramMonitorRenderer::isSupported() noexcept
{
    return true;
}

QString GpuProgramMonitorRenderer::backendName() const
{
    return impl_ ? impl_->backendName : QString();
}

QString GpuProgramMonitorRenderer::lastFailureReason() const
{
    return impl_ ? impl_->lastFailureReason : QString();
}

void GpuProgramMonitorRenderer::reset()
{
    if (!impl_) {
        return;
    }
    impl_->deviceManager.destroy();
    impl_->pso.Release();
    impl_->shaderVs.Release();
    impl_->shaderPs.Release();
    impl_->shaderResourceBinding.Release();
    impl_->linearSampler.Release();
    impl_->viewConstantBuffer.Release();
    impl_->frameCache = ArtifactCore::ImageF32x4RGBAWithCache();
    impl_->backendName.clear();
    impl_->lastFailureReason.clear();
    impl_->initialized = false;
    impl_->hasFrame = false;
    impl_->frameGeneration = 0;
}

bool GpuProgramMonitorRenderer::ensureInitialized(QWidget* host)
{
    if (!impl_) {
        return false;
    }
    if (impl_->initialized) {
        return true;
    }
    if (!host) {
        impl_->lastFailureReason = QStringLiteral("no host widget");
        return false;
    }

    impl_->deviceManager.initialize(host);
    Diligent::IRenderDevice* device = impl_->deviceManager.device();
    Diligent::IDeviceContext* context = impl_->deviceManager.immediateContext();
    if (!device || !context) {
        impl_->lastFailureReason = QStringLiteral("render device unavailable");
        return false;
    }

    const Artifact::SelectedGpuAdapterInfo adapter = impl_->deviceManager.selectedAdapterInfo();
    impl_->backendName = adapter.backend.isEmpty()
        ? QStringLiteral("unknown")
        : adapter.backend;

    if (!impl_->createPipeline(device)) {
        return false;
    }

    impl_->frameCache.SetGpuResources(impl_->deviceManager.device(),
                                     impl_->deviceManager.immediateContext());

    impl_->initialized = true;
    return true;
}

void GpuProgramMonitorRenderer::setFrame(const ArtifactCore::ImageF32x4_RGBA& frame,
                                         quint64 generation)
{
    if (!impl_) {
        return;
    }
    if (frame.isEmpty()) {
        // An empty ImageF32x4_RGBA is the "no frame" state; assigning a default
        // instance drops the CPU image, so GetGpuTextureSRV returns null and
        // renderAndPresent() falls back to the black clear.
        impl_->frameCache = ArtifactCore::ImageF32x4RGBAWithCache();
        impl_->hasFrame = false;
        return;
    }
    // A newer request already replaced this frame: drop the stale upload
    // instead of letting it present after the newer content.
    if (generation != 0 && generation < impl_->frameGeneration) {
        return;
    }
    impl_->frameGeneration = generation;
    impl_->frameCache.SetCpuImage(frame);
    impl_->hasFrame = true;
}

void GpuProgramMonitorRenderer::setViewTransform(float scaleX, float scaleY,
                                                 float offsetX, float offsetY)
{
    if (!impl_ || !impl_->viewConstantBuffer) {
        return;
    }
    Diligent::IDeviceContext* context = impl_->deviceManager.immediateContext();
    if (!context) {
        return;
    }

    // Guard against a zero/negative scale producing NaN in the shader's
    // divide; fall back to a full-frame view instead.
    if (!(scaleX > 0.0f) || !(scaleY > 0.0f)) {
        scaleX = 1.0f;
        scaleY = 1.0f;
        offsetX = 0.0f;
        offsetY = 0.0f;
    }

    void* mapped = nullptr;
    if (!context->MapBuffer(impl_->viewConstantBuffer, Diligent::MAP_WRITE,
                            Diligent::MAP_FLAG_DISCARD, mapped) || !mapped) {
        return;
    }
    const float values[4] = { scaleX, scaleY, offsetX, offsetY };
    std::memcpy(mapped, values, sizeof(values));
    context->UnmapBuffer(impl_->viewConstantBuffer, Diligent::MAP_WRITE);
}

void GpuProgramMonitorRenderer::renderAndPresent()
{
    if (!impl_) {
        return;
    }
    Diligent::ISwapChain* swapChain = impl_->deviceManager.swapChain();
    Diligent::IDeviceContext* context = impl_->deviceManager.immediateContext();
    if (!swapChain || !context || !impl_->pso || !impl_->shaderResourceBinding
        || !impl_->linearSampler) {
        return;
    }

    Diligent::ITextureView* backBuffer = swapChain->GetCurrentBackBufferRTV();
    context->SetRenderTargets(1, &backBuffer, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    context->ClearRenderTarget(backBuffer, clearColor,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Diligent::RefCntAutoPtr<Diligent::ITextureView> sourceView =
        impl_->frameCache.GetGpuTextureSRV(context);
    if (sourceView) {
        if (auto* sourceVariable = impl_->shaderResourceBinding->GetVariableByName(
                Diligent::SHADER_TYPE_PIXEL, "g_src")) {
            sourceVariable->Set(sourceView);
        }
        if (auto* samplerVariable = impl_->shaderResourceBinding->GetVariableByName(
                Diligent::SHADER_TYPE_PIXEL, "g_smp")) {
            samplerVariable->Set(impl_->linearSampler);
        }
        if (auto* viewVariable = impl_->shaderResourceBinding->GetVariableByName(
                Diligent::SHADER_TYPE_PIXEL, "ViewCB")) {
            viewVariable->Set(impl_->viewConstantBuffer);
        }

        context->SetPipelineState(impl_->pso);
        context->CommitShaderResources(impl_->shaderResourceBinding,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::DrawAttribs drawAttribs;
        drawAttribs.NumVertices = 3;
        drawAttribs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
        context->Draw(drawAttribs);
    }

    impl_->present(swapChain, context);
}

bool GpuProgramMonitorRenderer::recreateSwapChain(QWidget* host)
{
    if (!impl_ || !host) {
        return false;
    }
    impl_->deviceManager.recreateSwapChain(host);
    return impl_->deviceManager.swapChain() != nullptr;
}

} // namespace ArtifactPr
