// Burn Away effect image-evaluation lab.
//
// Purpose: render the shipped Burn Away effect over a synthetic paper surface
// and write PNG frames plus alpha statistics, so the burn look is judged from
// images instead of from code reading.  The lab never modifies product code.
//
// Usage:
//   ArtifactBurnAwayLab [--out <dir>] [--width N] [--height N]
//
// Output:
//   <dir>/source.png              the untouched input
//   <dir>/burn_p<NN>.png          the composited result per progress step
//   stdout                        per-step alpha and luminance statistics

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <QGuiApplication>
#include <QImage>
#include <QString>
#include <QVariant>
#include <QVector3D>

#include <OpenImageIO/imageio.h>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.BurnAway;
import Artifact.Generator.Particle;
import Artifact.Layer.Particle;
import Artifact.Render.DiligentDeviceManager;
import Graphics.Compute;
import Graphics.SurfaceColorContract;
import Image.GpuImageUpload;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

struct LabOptions {
    std::filesystem::path outDir = "captures/burn-away-lab";
    int width = 960;
    int height = 540;
};

struct StepStats {
    float progress = 0.0f;
    float meanAlpha = 0.0f;
    float meanLuminance = 0.0f;
    float maxLuminance = 0.0f;
    float opaqueFraction = 0.0f;
    // The most saturated pixel, used to spot colour outliers such as a
    // channel-order regression in the effect output.
    float outlierRed = 0.0f;
    float outlierGreen = 0.0f;
    float outlierBlue = 0.0f;
    float outlierAlpha = 0.0f;
    float outlierScore = -1.0f;
};

// A small deterministic value noise so the paper texture is reproducible.
float hash2D(int x, int y, int seed)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u
                    + static_cast<std::uint32_t>(y) * 668265263u
                    + static_cast<std::uint32_t>(seed) * 2246822519u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= (h >> 16u);
    return static_cast<float>(h) / static_cast<float>(0xFFFFFFFFu);
}

float valueNoise(float x, float y, int seed)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const float v00 = hash2D(x0, y0, seed);
    const float v10 = hash2D(x0 + 1, y0, seed);
    const float v01 = hash2D(x0, y0 + 1, seed);
    const float v11 = hash2D(x0 + 1, y0 + 1, seed);
    const float sx = fx * fx * (3.0f - 2.0f * fx);
    const float sy = fy * fy * (3.0f - 2.0f * fy);
    const float ix0 = v00 + (v10 - v00) * sx;
    const float ix1 = v01 + (v11 - v01) * sx;
    return ix0 + (ix1 - ix0) * sy;
}

// A warm off-white paper with fibre texture and a few printed bars, so the
// char band has both light and dark content to darken.
std::vector<float> makePaper(int width, int height)
{
    std::vector<float> pixels(static_cast<std::size_t>(width) * height * 4, 0.0f);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float fibre = valueNoise(static_cast<float>(x) * 0.35f,
                                           static_cast<float>(y) * 0.35f, 11) * 0.06f;
            const float grain = valueNoise(static_cast<float>(x) * 1.7f,
                                           static_cast<float>(y) * 1.7f, 29) * 0.03f;
            float red = 0.86f + fibre + grain;
            float green = 0.84f + fibre + grain;
            float blue = 0.78f + fibre + grain;

            // Printed bars: dark ink rows that make the char readable.
            const int band = (y / 37) % 5;
            if (band == 2 && x > width / 8 && x < width * 3 / 4) {
                red = 0.10f;
                green = 0.10f;
                blue = 0.12f;
            }
            const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 4;
            pixels[index + 0] = red;
            pixels[index + 1] = green;
            pixels[index + 2] = blue;
            pixels[index + 3] = 1.0f;
        }
    }
    return pixels;
}

float linearToSrgb(float value)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    if (clamped <= 0.0031308f) {
        return clamped * 12.92f;
    }
    return 1.055f * std::pow(clamped, 1.0f / 2.4f) - 0.055f;
}

// The surface buffer is B,G,R,A and the layer surface contract is straight
// alpha, so the composite multiplies the colour by alpha.
std::vector<std::uint8_t> compositeOverGray(const float* bgra, int width, int height,
                                             float background)
{
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4, 0);
    const std::size_t count = static_cast<std::size_t>(width) * height;
    for (std::size_t i = 0; i < count; ++i) {
        const float alpha = std::clamp(bgra[i * 4 + 3], 0.0f, 1.0f);
        const float blue = bgra[i * 4 + 0];
        const float green = bgra[i * 4 + 1];
        const float red = bgra[i * 4 + 2];
        const float outRed = red * alpha + background * (1.0f - alpha);
        const float outGreen = green * alpha + background * (1.0f - alpha);
        const float outBlue = blue * alpha + background * (1.0f - alpha);
        rgba[i * 4 + 0] = static_cast<std::uint8_t>(linearToSrgb(outRed) * 255.0f + 0.5f);
        rgba[i * 4 + 1] = static_cast<std::uint8_t>(linearToSrgb(outGreen) * 255.0f + 0.5f);
        rgba[i * 4 + 2] = static_cast<std::uint8_t>(linearToSrgb(outBlue) * 255.0f + 0.5f);
        rgba[i * 4 + 3] = 255;
    }
    return rgba;
}

bool writePng(const std::filesystem::path& path, const std::vector<std::uint8_t>& rgba,
               int width, int height)
{
    auto output = OIIO::ImageOutput::create(path.string());
    if (!output) {
        std::fprintf(stderr, "cannot create image output for %s\n", path.string().c_str());
        return false;
    }
    const OIIO::ImageSpec spec(width, height, 4, OIIO::TypeDesc::UINT8);
    if (!output->open(path.string(), spec)) {
        std::fprintf(stderr, "cannot open %s\n", path.string().c_str());
        return false;
    }
    const bool ok = output->write_image(OIIO::TypeDesc::UINT8, rgba.data());
    output->close();
    return ok;
}

// Renders the ash and ember particles that sit on the burn front.  The front
// position uses the same normalised geometry as the effect itself, so the
// particle band tracks the burning edge.
QImage renderBurnParticles(int width, int height, float frontY, float seconds)
{
    Artifact::ArtifactParticleLayer layer;
    layer.setLayerPropertyValue(QStringLiteral("particle.previewWidth"),
                                 QVariant(width));
    layer.setLayerPropertyValue(QStringLiteral("particle.previewHeight"),
                                 QVariant(height));
    layer.clearEmitters();

    auto ashParams = Artifact::ParticlePresets::ash();
    ashParams.position = QVector3D(static_cast<float>(width) * 0.5f, frontY, 0.0f);
    ashParams.randomSeed = 20261119u;
    layer.addEmitter(ashParams);

    auto emberParams = Artifact::ParticlePresets::ember();
    emberParams.position = QVector3D(static_cast<float>(width) * 0.5f, frontY, 0.0f);
    emberParams.randomSeed = 20261120u;
    layer.addEmitter(emberParams);

    layer.resetParticleSystem();
    layer.play();
    // The particle system clamps every update to a bounded number of fixed
    // sub-steps, so a single large delta only advances the simulation by a
    // fraction of a second.  Advance frame by frame like the player does.
    constexpr float kStep = 1.0f / 60.0f;
    const int steps = std::max(1, static_cast<int>(seconds / kStep));
    QImage frame;
    for (int step = 1; step <= steps; ++step) {
        frame = layer.renderFrame(width, height, static_cast<float>(step) * kStep);
    }
    // Diagnostic: report the particle state so a renderer regression is
    // visible instead of silent.
    if (const auto* system = layer.particleSystem()) {
        for (std::size_t index = 0; index < system->emitters().size(); ++index) {
            const auto* emitter = system->emitters()[index].get();
            const int count = emitter ? emitter->particleCount() : -1;
            std::printf("emitter %zu: particles=%d", index, count);
            if (emitter && count > 0) {
                const auto& particle = emitter->particles().front();
                std::printf(" pos=(%.1f,%.1f) scale=%.2f opacity=%.2f color=(%d,%d,%d,%d)",
                            particle.position.x(), particle.position.y(), particle.scale,
                            particle.opacity, particle.color.red(), particle.color.green(),
                            particle.color.blue(), particle.color.alpha());
            }
            std::printf("\n");
        }
    }
    int visible = 0;
    int bestScore = -1;
    QRgb bestPixel = 0;
    for (int y = 0; y < frame.height(); ++y) {
        for (int x = 0; x < frame.width(); ++x) {
            const QRgb pixel = frame.pixel(x, y);
            const int alpha = qAlpha(pixel);
            if (alpha == 0) {
                continue;
            }
            ++visible;
            const int score = qGreen(pixel) + qBlue(pixel) - qRed(pixel);
            if (score > bestScore) {
                bestScore = score;
                bestPixel = pixel;
            }
        }
    }
    std::printf("particles: visible=%d brightest_rgba=%d,%d,%d,%d\n", visible,
                qRed(bestPixel), qGreen(bestPixel), qBlue(bestPixel), qAlpha(bestPixel));
    return frame;
}

// Composites the straight-alpha particle frame over the sRGB base image.
std::vector<std::uint8_t> overlayParticles(const std::vector<std::uint8_t>& base,
                                            const QImage& particles)
{
    std::vector<std::uint8_t> result = base;
    if (particles.isNull()) {
        return result;
    }
    const QImage image = particles.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            const int alpha = qAlpha(pixel);
            if (alpha == 0) {
                continue;
            }
            const std::size_t index =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width()) +
                 static_cast<std::size_t>(x)) * 4;
            const float weight = static_cast<float>(alpha) / 255.0f;
            const float sourceChannels[3] = {
                static_cast<float>(qRed(pixel)),
                static_cast<float>(qGreen(pixel)),
                static_cast<float>(qBlue(pixel))};
            for (int channel = 0; channel < 3; ++channel) {
                const float source = sourceChannels[channel];
                const float destination = static_cast<float>(result[index + channel]);
                result[index + channel] = static_cast<std::uint8_t>(
                    source * weight + destination * (1.0f - weight) + 0.5f);
            }
        }
    }
    return result;
}

StepStats measure(const float* bgra, int width, int height)
{
    StepStats stats;
    const std::size_t count = static_cast<std::size_t>(width) * height;
    double alphaSum = 0.0;
    double luminanceSum = 0.0;
    std::size_t opaque = 0;
    float maxLuminance = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const float alpha = std::clamp(bgra[i * 4 + 3], 0.0f, 1.0f);
        const float blue = bgra[i * 4 + 0];
        const float green = bgra[i * 4 + 1];
        const float red = bgra[i * 4 + 2];
        const float luminance = 0.2126f * red + 0.7152f * green + 0.0722f * blue;
        alphaSum += alpha;
        luminanceSum += luminance * alpha;
        if (alpha > 0.999f) {
            ++opaque;
        }
        maxLuminance = std::max(maxLuminance, luminance);
        const float saturation = std::max({red, green, blue}) -
                                 std::min({red, green, blue});
        if (saturation > stats.outlierScore) {
            stats.outlierScore = saturation;
            stats.outlierRed = red;
            stats.outlierGreen = green;
            stats.outlierBlue = blue;
            stats.outlierAlpha = alpha;
        }
    }
    stats.meanAlpha = static_cast<float>(alphaSum / static_cast<double>(count));
    stats.meanLuminance = static_cast<float>(luminanceSum / static_cast<double>(count));
    stats.opaqueFraction = static_cast<float>(static_cast<double>(opaque) /
                                              static_cast<double>(count));
    stats.maxLuminance = maxLuminance;
    return stats;
}

} // namespace

// Mirrors the generic resident prelude that the composition layer pipeline
// prepends to every registered shader body.  The lab dispatches the very
// same shader source the effect registers, so the comparison measures the
// shipped shader against the shipped CPU implementation.
static constexpr const char* kResidentGenericPrelude = R"(
cbuffer ResidentGenericParams : register(b0)
{
    float g_P0;
    float g_P1;
    float g_P2;
    float g_P3;
    float g_P4;
    float g_P5;
    float g_P6;
    float g_P7;
    float g_P8;
    float g_P9;
    float g_P10;
    float g_P11;
    float g_P12;
    float g_P13;
    float g_P14;
    float g_P15;
    float g_Width;
    float g_Height;
    float g_Time;
    float g_Frame;
    float g_HistoryValid;
};
)";

struct ResidentGenericParams {
    float p[16] = {};
    float width = 0.0f;
    float height = 0.0f;
    float time = 0.0f;
    float frame = 0.0f;
    float historyValid = 0.0f;
};

int compareFloatBuffers(const std::vector<float>& left, const std::vector<float>& right,
                        int width, int height, const std::filesystem::path& diffPath)
{
    const std::size_t count = static_cast<std::size_t>(width) * height;
    double absoluteError = 0.0;
    double squaredError = 0.0;
    double maxDifference = 0.0;
    std::size_t differing = 0;
    int worstX = 0;
    int worstY = 0;
    std::vector<std::uint8_t> diff(count * 4, 0);
    for (std::size_t i = 0; i < count; ++i) {
        const float difference = std::fabs(left[i * 4 + 0] - right[i * 4 + 0]);
        absoluteError += difference;
        squaredError += difference * difference;
        if (difference > maxDifference) {
            maxDifference = difference;
            worstX = static_cast<int>(i % static_cast<std::size_t>(width));
            worstY = static_cast<int>(i / static_cast<std::size_t>(width));
        }
        if (difference > 0.5 / 255.0) {
            ++differing;
        }
        const std::uint8_t shade = static_cast<std::uint8_t>(
            std::min(255.0, difference * 255.0 * 8.0));
        diff[i * 4 + 0] = shade;
        diff[i * 4 + 1] = shade;
        diff[i * 4 + 2] = shade;
        diff[i * 4 + 3] = 255;
    }
    std::printf("gpu parity: mean_abs_error=%.6f rmse=%.6f max_difference=%.6f differing_pixels=%zu/%zu (%.4f%%) worst=(%d,%d)\n",
                absoluteError / static_cast<double>(count),
                std::sqrt(squaredError / static_cast<double>(count)), maxDifference,
                differing, count,
                100.0 * static_cast<double>(differing) / static_cast<double>(count),
                worstX, worstY);
    if (!diffPath.empty()) {
        writePng(diffPath, diff, width, height);
        std::printf("  diff image: %s\n", diffPath.string().c_str());
    }
    return 0;
}

// Runs the registered generic resident shader on the GPU and compares the
// result with the CPU reference implementation.
int runGpuParity(int width, int height, float progress, float emberIntensity,
                 float sparkDensity, float charAmount,
                 const std::filesystem::path& diffPath)
{
    // The effect registers its shader in the constructor, so the instance has
    // to exist before the registry can be queried.
    Artifact::BurnAwayEffect effect;
    effect.setProgress(progress);
    effect.setEmberIntensity(emberIntensity);
    effect.setSparkDensity(sparkDensity);
    effect.setCharAmount(charAmount);
    effect.setContext(Artifact::EffectContext{});

    Artifact::GpuGenericShaderRecord record{};
    const std::uint32_t key = Artifact::gpuGenericKeyFromString("burn_away");
    if (!Artifact::findGpuGenericShader(key, &record) || !record.shaderBody) {
        std::fprintf(stderr, "the burn away shader is not registered\n");
        return 1;
    }

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    if (!Artifact::acquireSharedRenderDeviceForCurrentBackend(device, context)) {
        std::fprintf(stderr, "no render device available\n");
        return 1;
    }

    // The same synthetic paper the CPU reference uses.
    const std::vector<float> paper = makePaper(width, height);
    ArtifactCore::ImageF32x4_RGBA paperImage;
    paperImage.setFromRGBA32F(paper.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(paperImage);

    ArtifactCore::ImageF32x4RGBAWithCache cpuOutput;
    effect.applyCPUOnly(source, cpuOutput);
    const float* cpuPixels = cpuOutput.image().rgba32fData();
    if (!cpuPixels) {
        std::fprintf(stderr, "the cpu reference produced no pixels\n");
        Artifact::releaseSharedRenderDevice();
        return 1;
    }

    Artifact::GpuSpatialEffectStack stack;
    if (!effect.appendGpuSpatialNodes(stack) || stack.count == 0) {
        std::fprintf(stderr, "the effect produced no gpu node\n");
        Artifact::releaseSharedRenderDevice();
        return 1;
    }
    const auto& node = stack.nodes[0];

    // Upload the input as canonical RGBA.
    const auto upload = ArtifactCore::makeGpuImageUploadBuffer(source.image().surfaceView());
    if (!upload.isValid()) {
        std::fprintf(stderr, "the input upload buffer is invalid\n");
        Artifact::releaseSharedRenderDevice();
        return 1;
    }
    Diligent::TextureDesc inputDesc;
    inputDesc.Name = "BurnAwayLab/Input";
    inputDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    inputDesc.Width = static_cast<Diligent::Uint32>(width);
    inputDesc.Height = static_cast<Diligent::Uint32>(height);
    inputDesc.MipLevels = 1;
    inputDesc.ArraySize = 1;
    inputDesc.SampleCount = 1;
    inputDesc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    inputDesc.Usage = Diligent::USAGE_IMMUTABLE;
    inputDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData inputData{};
    inputData.pData = upload.data();
    inputData.Stride = static_cast<Diligent::Uint64>(upload.rowStride);
    Diligent::TextureData inputInit{};
    inputInit.pSubResources = &inputData;
    inputInit.NumSubresources = 1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> inputTexture;
    device->CreateTexture(inputDesc, &inputInit, &inputTexture);
    if (!inputTexture) {
        std::fprintf(stderr, "the input texture was not created\n");
        Artifact::releaseSharedRenderDevice();
        return 1;
    }

    Diligent::TextureDesc outputDesc = inputDesc;
    outputDesc.Name = "BurnAwayLab/Output";
    outputDesc.Usage = Diligent::USAGE_DEFAULT;
    outputDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTexture;
    device->CreateTexture(outputDesc, nullptr, &outputTexture);
    if (!outputTexture) {
        std::fprintf(stderr, "the output texture was not created\n");
        Artifact::releaseSharedRenderDevice();
        return 1;
    }

    Diligent::BufferDesc paramsDesc;
    paramsDesc.Name = "BurnAwayLab/Params";
    paramsDesc.Usage = Diligent::USAGE_DYNAMIC;
    paramsDesc.Size = sizeof(ResidentGenericParams);
    paramsDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
    paramsDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsBuffer;
    device->CreateBuffer(paramsDesc, nullptr, &paramsBuffer);
    if (!paramsBuffer) {
        std::fprintf(stderr, "the parameter buffer was not created\n");
        Artifact::releaseSharedRenderDevice();
        return 1;
    }
    ResidentGenericParams params;
    for (std::size_t index = 0; index < node.parameters.size(); ++index) {
        params.p[index] = node.parameters[index];
    }
    params.width = static_cast<float>(width);
    params.height = static_cast<float>(height);
    params.time = 0.0f;
    params.frame = 0.0f;
    params.historyValid = 0.0f;
    void* mapped = nullptr;
    context->MapBuffer(paramsBuffer, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
    if (!mapped) {
        std::fprintf(stderr, "the parameter buffer was not mapped\n");
        Artifact::releaseSharedRenderDevice();
        return 1;
    }
    std::memcpy(mapped, &params, sizeof(params));
    context->UnmapBuffer(paramsBuffer, Diligent::MAP_WRITE);

    const std::string source_text = std::string(kResidentGenericPrelude) + record.shaderBody;
    static const Diligent::ShaderResourceVariableDesc variables[] = {
        {Diligent::SHADER_TYPE_COMPUTE, "ResidentGenericParams",
         Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture",
         Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture",
         Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
    ArtifactCore::GpuContext gpuContext{device, context};
    ArtifactCore::ComputeExecutor executor{gpuContext};
    ArtifactCore::ComputePipelineDesc pipeline;
    pipeline.name = "BurnAwayLab/PSO";
    pipeline.shaderSource = source_text.c_str();
    pipeline.entryPoint = record.entryPoint ? record.entryPoint : "main";
    pipeline.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    pipeline.variables = variables;
    pipeline.variableCount = 3;
    pipeline.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    if (!executor.build(pipeline) || !executor.createShaderResourceBinding(true) ||
        !executor.setBuffer("ResidentGenericParams", paramsBuffer) ||
        !executor.setTextureView("g_InputTexture",
                                 inputTexture->GetDefaultView(
                                     Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
        !executor.setTextureView("g_OutputTexture",
                                 outputTexture->GetDefaultView(
                                     Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
        std::fprintf(stderr, "the compute pipeline could not be prepared\n");
        Artifact::releaseSharedRenderDevice();
        return 1;
    }
    executor.dispatch(context,
                      ArtifactCore::ComputeExecutor::makeDispatchAttribs(
                          static_cast<Diligent::Uint32>(width),
                          static_cast<Diligent::Uint32>(height), 1, 8, 8, 1));

    Diligent::TextureDesc stagingDesc = outputDesc;
    stagingDesc.Name = "BurnAwayLab/Staging";
    stagingDesc.Usage = Diligent::USAGE_STAGING;
    stagingDesc.BindFlags = Diligent::BIND_NONE;
    stagingDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
    Diligent::RefCntAutoPtr<Diligent::ITexture> stagingTexture;
    device->CreateTexture(stagingDesc, nullptr, &stagingTexture);
    if (!stagingTexture) {
        std::fprintf(stderr, "the staging texture was not created\n");
        Artifact::releaseSharedRenderDevice();
        return 1;
    }
    Diligent::CopyTextureAttribs copy(outputTexture, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                      stagingTexture, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->CopyTexture(copy);
    context->Flush();
    context->WaitForIdle();
    Diligent::MappedTextureSubresource mappedRegion{};
    context->MapTextureSubresource(stagingTexture, 0, 0, Diligent::MAP_READ,
                                   Diligent::MAP_FLAG_NONE, nullptr, mappedRegion);
    if (!mappedRegion.pData) {
        std::fprintf(stderr, "the staging texture was not mapped\n");
        Artifact::releaseSharedRenderDevice();
        return 1;
    }
    const std::size_t count = static_cast<std::size_t>(width) * height;
    std::vector<float> gpuPixels(count * 4, 0.0f);
    const auto* sourceBytes = static_cast<const std::uint8_t*>(mappedRegion.pData);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t* pixel = sourceBytes + i * 16;
        // The readback is canonical RGBA float.
        gpuPixels[i * 4 + 0] = *reinterpret_cast<const float*>(pixel + 0);
        gpuPixels[i * 4 + 1] = *reinterpret_cast<const float*>(pixel + 4);
        gpuPixels[i * 4 + 2] = *reinterpret_cast<const float*>(pixel + 8);
        gpuPixels[i * 4 + 3] = *reinterpret_cast<const float*>(pixel + 12);
    }
    context->UnmapTextureSubresource(stagingTexture, 0, 0);

    // The cpu output buffer is B,G,R,A, so the comparison uses logical RGBA.
    std::vector<float> cpuLogical(count * 4, 0.0f);
    for (std::size_t i = 0; i < count; ++i) {
        cpuLogical[i * 4 + 0] = cpuPixels[i * 4 + 2];
        cpuLogical[i * 4 + 1] = cpuPixels[i * 4 + 1];
        cpuLogical[i * 4 + 2] = cpuPixels[i * 4 + 0];
        cpuLogical[i * 4 + 3] = cpuPixels[i * 4 + 3];
    }

    std::printf("gpu parity run: progress=%.2f ember=%.2f sparks=%.2f char=%.2f\n",
                progress, emberIntensity, sparkDensity, charAmount);
    const int result = compareFloatBuffers(cpuLogical, gpuPixels, width, height, diffPath);
    Artifact::releaseSharedRenderDevice();
    return result;
}

// Compares two PNG files and reports the difference statistics the R&D plan
// asks for: per-channel absolute error, RMSE, the maximum difference, the
// share of pixels that differ at all, and the worst pixel location.
int compareImages(const std::filesystem::path& leftPath,
                  const std::filesystem::path& rightPath,
                  const std::filesystem::path& diffPath)
{
    auto readImage = [](const std::filesystem::path& path, std::vector<float>& pixels,
                         int& width, int& height) {
        auto input = OIIO::ImageInput::open(path.string());
        if (!input) {
            return false;
        }
        const OIIO::ImageSpec spec = input->spec();
        width = spec.width;
        height = spec.height;
        pixels.assign(static_cast<std::size_t>(width) * height * spec.nchannels, 0.0f);
        input->read_image(0, 0, 0, spec.nchannels, OIIO::TypeDesc::FLOAT, pixels.data());
        input->close();
        return true;
    };

    std::vector<float> left;
    std::vector<float> right;
    int leftWidth = 0;
    int leftHeight = 0;
    int rightWidth = 0;
    int rightHeight = 0;
    if (!readImage(leftPath, left, leftWidth, leftHeight) ||
        !readImage(rightPath, right, rightWidth, rightHeight)) {
        std::fprintf(stderr, "cannot read the comparison images\n");
        return 1;
    }
    if (leftWidth != rightWidth || leftHeight != rightHeight) {
        std::fprintf(stderr, "image sizes differ: %dx%d vs %dx%d\n", leftWidth, leftHeight,
                     rightWidth, rightHeight);
        return 1;
    }

    const std::size_t count = static_cast<std::size_t>(leftWidth) * leftHeight;
    double absoluteError = 0.0;
    double squaredError = 0.0;
    double maxDifference = 0.0;
    std::size_t differing = 0;
    int worstX = 0;
    int worstY = 0;
    std::vector<std::uint8_t> diff(static_cast<std::size_t>(count) * 4, 0);
    for (std::size_t i = 0; i < count; ++i) {
        const float a = left[i * 4 + 0];
        const float b = right[i * 4 + 0];
        const float difference = std::fabs(a - b);
        absoluteError += difference;
        squaredError += difference * difference;
        maxDifference = std::max(maxDifference, static_cast<double>(difference));
        if (difference > 0.5 / 255.0) {
            ++differing;
        }
        if (difference > maxDifference * 0.999) {
            worstX = static_cast<int>(i % static_cast<std::size_t>(leftWidth));
            worstY = static_cast<int>(i / static_cast<std::size_t>(leftWidth));
        }
        const std::uint8_t shade = static_cast<std::uint8_t>(
            std::min(255.0, difference * 255.0 * 4.0));
        diff[i * 4 + 0] = shade;
        diff[i * 4 + 1] = shade;
        diff[i * 4 + 2] = shade;
        diff[i * 4 + 3] = 255;
    }

    std::printf("compare: %s vs %s\n", leftPath.string().c_str(), rightPath.string().c_str());
    std::printf("  mean_abs_error=%.6f rmse=%.6f max_difference=%.6f differing_pixels=%zu/%zu (%.4f%%) worst=(%d,%d)\n",
                absoluteError / static_cast<double>(count),
                std::sqrt(squaredError / static_cast<double>(count)), maxDifference,
                differing, count,
                100.0 * static_cast<double>(differing) / static_cast<double>(count),
                worstX, worstY);
    if (!diffPath.empty()) {
        writePng(diffPath, diff, leftWidth, leftHeight);
        std::printf("  diff image: %s\n", diffPath.string().c_str());
    }
    return 0;
}

int main(int argc, char** argv)
{
    // The particle layer renders through QPainter in the software path, which
    // needs a QGuiApplication even though no window is shown.
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication app(argc, argv);

    if (argc >= 4 && std::string(argv[1]) == "--compare") {
        return compareImages(argv[2], argv[3], argc >= 5 ? argv[4] : std::filesystem::path());
    }

    if (argc >= 2 && std::string(argv[1]) == "--gpu-parity") {
        const int width = argc >= 3 ? std::atoi(argv[2]) : 960;
        const int height = argc >= 4 ? std::atoi(argv[3]) : 540;
        const std::filesystem::path diffPath =
            argc >= 5 ? std::filesystem::path(argv[4]) : std::filesystem::path();
        return runGpuParity(width, height, 0.45f, 1.2f, 0.6f, 0.9f, diffPath);
    }

    LabOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--out" && i + 1 < argc) {
            options.outDir = argv[++i];
        } else if (argument == "--width" && i + 1 < argc) {
            options.width = std::atoi(argv[++i]);
        } else if (argument == "--height" && i + 1 < argc) {
            options.height = std::atoi(argv[++i]);
        }
    }

    std::error_code error;
    std::filesystem::create_directories(options.outDir, error);

    const std::vector<float> paper = makePaper(options.width, options.height);
    ArtifactCore::ImageF32x4_RGBA sourceImage;
    sourceImage.setFromRGBA32F(paper.data(), options.width, options.height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(sourceImage);

    if (!writePng(options.outDir / "source.png",
                  compositeOverGray(source.image().rgba32fData(), options.width,
                                    options.height, 0.5f),
                  options.width, options.height)) {
        return 1;
    }

    // The burn front geometry mirrors the effect: the front travels from the
    // bottom edge upward and the noise perturbs it.
    constexpr float kSpread = 0.35f;
    constexpr float kSoftness = 0.004f;

    const std::vector<float> progressSteps{0.0f, 0.10f, 0.25f, 0.40f, 0.60f, 0.80f, 1.0f};
    std::printf("progress,mean_alpha,opaque_fraction,mean_luminance,max_luminance,front_y\n");
    for (float progress : progressSteps) {
        Artifact::BurnAwayEffect effect;
        effect.setProgress(progress);
        effect.setContext(Artifact::EffectContext{});

        ArtifactCore::ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);

        const float* pixels = output.image().rgba32fData();
        if (!pixels) {
            std::fprintf(stderr, "no pixel data at progress %.2f\n", progress);
            return 1;
        }
        StepStats stats = measure(pixels, options.width, options.height);
        stats.progress = progress;

        char name[64];
        std::snprintf(name, sizeof(name), "burn_p%02d.png",
                      static_cast<int>(std::lround(progress * 100.0f)));
        const std::vector<std::uint8_t> base =
            compositeOverGray(pixels, options.width, options.height, 0.5f);
        if (!writePng(options.outDir / name, base, options.width, options.height)) {
            return 1;
        }

        const float front = progress * (1.0f + 0.5f * kSpread + 2.0f * kSoftness) - kSoftness;
        const float frontY = static_cast<float>(options.height) * (1.0f - front);
        const QImage particles = renderBurnParticles(options.width, options.height,
                                                      frontY, 1.5f);
        if (!particles.isNull()) {
            char particleName[64];
            std::snprintf(particleName, sizeof(particleName), "burn_p%02d_particles.png",
                          static_cast<int>(std::lround(progress * 100.0f)));
            if (!writePng(options.outDir / particleName,
                          overlayParticles(base, particles), options.width, options.height)) {
                return 1;
            }
            const QImage flat = particles.convertToFormat(QImage::Format_ARGB32);
            std::vector<std::uint8_t> raw(static_cast<std::size_t>(options.width) *
                                          options.height * 4, 0);
            for (int y = 0; y < flat.height(); ++y) {
                for (int x = 0; x < flat.width(); ++x) {
                    const QRgb pixel = flat.pixel(x, y);
                    const std::size_t index =
                        (static_cast<std::size_t>(y) * static_cast<std::size_t>(flat.width()) +
                         static_cast<std::size_t>(x)) * 4;
                    raw[index + 0] = static_cast<std::uint8_t>(qRed(pixel));
                    raw[index + 1] = static_cast<std::uint8_t>(qGreen(pixel));
                    raw[index + 2] = static_cast<std::uint8_t>(qBlue(pixel));
                    raw[index + 3] = 255;
                }
            }
            char rawName[64];
            std::snprintf(rawName, sizeof(rawName), "particles_only_p%02d.png",
                          static_cast<int>(std::lround(progress * 100.0f)));
            writePng(options.outDir / rawName, raw, options.width, options.height);
        }

        std::printf("%.2f,%.6f,%.6f,%.6f,%.6f,%.1f\n", stats.progress, stats.meanAlpha,
                    stats.opaqueFraction, stats.meanLuminance, stats.maxLuminance, frontY);
    }
    return 0;
}
