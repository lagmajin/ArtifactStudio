#include <gtest/gtest.h>

#include <DeviceContext.h>
#include <Texture.h>
#include <TextureView.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <memory>

#include "DiligentVulkanTestDevice.hpp"

import Artifact.Render.DepthOfFieldPass;
import Artifact.Render.FinalPostProcess;
import Artifact.Render.MotionBlurPass;
import Core.Composition.FinalEffect;

namespace {

using namespace Diligent;
using ArtifactTest::VulkanGpuDevice;

constexpr unsigned kWidth = 9;
constexpr unsigned kHeight = 9;
constexpr std::size_t kPixelCount = kWidth * kHeight;

class PostProcessPassGpuContractTest : public testing::Test {
protected:
    void SetUp() override
    {
        if (!gpu_.initialize()) {
            GTEST_SKIP() << "Diligent Vulkan headless device is unavailable on this host";
        }
    }

    VulkanGpuDevice gpu_;
};

RefCntAutoPtr<ITexture> createTexture(
    IRenderDevice* device,
    const char* name,
    TEXTURE_FORMAT format,
    BIND_FLAGS bindFlags,
    const float* data,
    Uint64 stride);

RefCntAutoPtr<ITexture> createDepthTexture(
    VulkanGpuDevice& gpu,
    float clearDepth);

bool readbackRgba32f(
    VulkanGpuDevice& gpu,
    ITexture* source,
    std::array<float, kPixelCount * 4>& pixels);

std::size_t pixelOffset(unsigned x, unsigned y);

TEST(FinalPostProcessContractTest, StackRequiresAnEnabledLutAndClearResetsIt)
{
    ArtifactCore::GpuContext context;
    Artifact::ArtifactFinalPostProcess postProcess(context);
    ArtifactCore::CompositionFinalEffectStack stack;

    auto unrelatedEffect = std::make_unique<ArtifactCore::CompositionFinalEffect>(
        ArtifactCore::FinalEffectType::Exposure);
    stack.addEffect(std::move(unrelatedEffect));
    auto disabledLut = std::make_unique<ArtifactCore::CompositionFinalEffect>(
        ArtifactCore::FinalEffectType::LUT);
    disabledLut->setEnabled(false);
    stack.addEffect(std::move(disabledLut));
    postProcess.updateFromEffectStack(stack);
    EXPECT_FALSE(postProcess.hasActiveLUT());

    auto enabledLut = std::make_unique<ArtifactCore::CompositionFinalEffect>(
        ArtifactCore::FinalEffectType::LUT);
    enabledLut->setParameter(QStringLiteral("size"), 16.0f);
    stack.addEffect(std::move(enabledLut));
    postProcess.updateFromEffectStack(stack);
    EXPECT_FALSE(postProcess.hasActiveLUT())
        << "Stack selection validates configuration; LUT pixels are uploaded separately";

    stack.clear();
    postProcess.updateFromEffectStack(stack);
    EXPECT_FALSE(postProcess.hasActiveLUT());
}

TEST(FinalPostProcessContractTest,
     UploadWithoutDeviceDoesNotReportAnActiveLut)
{
    ArtifactCore::GpuContext context;
    Artifact::ArtifactFinalPostProcess postProcess(context);
    constexpr std::array<float, 24> lut{
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f};

    postProcess.updateFromColorLUT(lut.data(), 2);

    EXPECT_FALSE(postProcess.hasActiveLUT());
}

TEST_F(PostProcessPassGpuContractTest, UploadedLutTransformsPixelsAndRejectsMissingTransform)
{
    ArtifactCore::GpuContext context(gpu_.device(), gpu_.context());
    Artifact::ArtifactFinalPostProcess postProcess(context);

    std::array<float, kPixelCount * 4> input{};
    for (std::size_t pixel = 0; pixel < kPixelCount; ++pixel) {
        input[pixel * 4 + 0] = 1.0f;
        input[pixel * 4 + 1] = 0.0f;
        input[pixel * 4 + 2] = 0.0f;
        input[pixel * 4 + 3] = 1.0f;
    }
    auto inputTexture = createTexture(
        gpu_.device(), "Final post-process LUT input", TEX_FORMAT_RGBA32_FLOAT,
        BIND_SHADER_RESOURCE, input.data(), kWidth * 4 * sizeof(float));
    auto outputTexture = createTexture(
        gpu_.device(), "Final post-process LUT output", TEX_FORMAT_RGBA32_FLOAT,
        BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE, nullptr, 0);
    ASSERT_NE(inputTexture, nullptr);
    ASSERT_NE(outputTexture, nullptr);
    auto* inputSrv = inputTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* outputUav = outputTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    ASSERT_NE(inputSrv, nullptr);
    ASSERT_NE(outputUav, nullptr);

    postProcess.setViewTransformEnabled(true);
    EXPECT_FALSE(postProcess.apply(
        gpu_.context(), inputSrv, outputUav, kWidth, kHeight));

    constexpr std::array<float, 24> invertRgbLut{
        1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    postProcess.updateFromColorLUT(invertRgbLut.data(), 2);
    ASSERT_TRUE(postProcess.hasActiveLUT());
    ASSERT_TRUE(postProcess.apply(
        gpu_.context(), inputSrv, outputUav, kWidth, kHeight));

    std::array<float, kPixelCount * 4> output{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, output));
    EXPECT_NEAR(output[0], 0.0f, 1.0e-4f);
    EXPECT_NEAR(output[1], 1.0f, 1.0e-4f);
    EXPECT_NEAR(output[2], 1.0f, 1.0e-4f);
    EXPECT_NEAR(output[3], 1.0f, 1.0e-4f);

    postProcess.setLUTInputDomain(0.0f, 2.0f);
    ASSERT_TRUE(postProcess.apply(
        gpu_.context(), inputSrv, outputUav, kWidth, kHeight));
    std::array<float, kPixelCount * 4> remappedDomainOutput{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, remappedDomainOutput));
    EXPECT_NEAR(remappedDomainOutput[0], 0.5f, 1.0e-4f);
    EXPECT_NEAR(remappedDomainOutput[1], 1.0f, 1.0e-4f);
    EXPECT_NEAR(remappedDomainOutput[2], 1.0f, 1.0e-4f);
    EXPECT_NEAR(remappedDomainOutput[3], 1.0f, 1.0e-4f);

    postProcess.setLUTInputDomain(2.0f, 2.0f);
    ASSERT_TRUE(postProcess.apply(
        gpu_.context(), inputSrv, outputUav, kWidth, kHeight));
    std::array<float, kPixelCount * 4> invalidDomainOutput{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, invalidDomainOutput));
    EXPECT_NEAR(invalidDomainOutput[0], 0.5f, 1.0e-4f)
        << "an invalid domain must leave the previous valid domain intact";

    postProcess.updateFromColorLUT(nullptr, 2);
    EXPECT_FALSE(postProcess.hasActiveLUT())
        << "a missing LUT payload must invalidate the previously uploaded LUT";
    postProcess.updateFromColorLUT(invertRgbLut.data(), 1);
    EXPECT_FALSE(postProcess.hasActiveLUT())
        << "LUT dimensions below the supported minimum must be rejected";
    postProcess.updateFromColorLUT(invertRgbLut.data(), 257);
    EXPECT_FALSE(postProcess.hasActiveLUT())
        << "LUT dimensions above the supported limit must be rejected";
    EXPECT_FALSE(postProcess.apply(
        gpu_.context(), inputSrv, outputUav, kWidth, kHeight));

    ArtifactCore::CompositionFinalEffectStack selectedStack;
    auto selectedLut = std::make_unique<ArtifactCore::CompositionFinalEffect>(
        ArtifactCore::FinalEffectType::LUT);
    selectedStack.addEffect(std::move(selectedLut));
    postProcess.updateFromColorLUT(invertRgbLut.data(), 2);
    ASSERT_TRUE(postProcess.hasActiveLUT())
        << "the configured LUT pixels must be uploaded before selecting the stack effect";
    postProcess.updateFromEffectStack(selectedStack);
    EXPECT_TRUE(postProcess.hasActiveLUT())
        << "Selecting the configured LUT must preserve its separately uploaded pixels";
    ASSERT_TRUE(postProcess.apply(
        gpu_.context(), inputSrv, outputUav, kWidth, kHeight));
    std::array<float, kPixelCount * 4> selectedStackOutput{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, selectedStackOutput));
    EXPECT_NEAR(selectedStackOutput[0], 0.5f, 1.0e-4f);
    EXPECT_NEAR(selectedStackOutput[1], 1.0f, 1.0e-4f);
    EXPECT_NEAR(selectedStackOutput[2], 1.0f, 1.0e-4f);
    EXPECT_NEAR(selectedStackOutput[3], 1.0f, 1.0e-4f);

    ASSERT_NE(selectedStack.getEffect(0), nullptr);
    selectedStack.getEffect(0)->setEnabled(false);
    postProcess.updateFromEffectStack(selectedStack);
    EXPECT_FALSE(postProcess.hasActiveLUT())
        << "Disabling the only LUT effect must clear stale uploaded LUT state";
    EXPECT_FALSE(postProcess.apply(
        gpu_.context(), inputSrv, outputUav, kWidth, kHeight));

    postProcess.clearLUT();
    EXPECT_FALSE(postProcess.hasActiveLUT());
    EXPECT_FALSE(postProcess.apply(
        gpu_.context(), inputSrv, outputUav, kWidth, kHeight));
}

RefCntAutoPtr<ITexture> createTexture(
    IRenderDevice* device,
    const char* name,
    TEXTURE_FORMAT format,
    BIND_FLAGS bindFlags,
    const float* data,
    Uint64 stride)
{
    TextureDesc desc;
    desc.Name = name;
    desc.Type = RESOURCE_DIM_TEX_2D;
    desc.Width = kWidth;
    desc.Height = kHeight;
    desc.Format = format;
    desc.MipLevels = 1;
    desc.Usage = USAGE_DEFAULT;
    desc.BindFlags = bindFlags;

    TextureSubResData subresource;
    subresource.pData = data;
    subresource.Stride = stride;
    TextureData initialData;
    initialData.pSubResources = &subresource;
    initialData.NumSubresources = 1;

    RefCntAutoPtr<ITexture> texture;
    device->CreateTexture(desc, data ? &initialData : nullptr, &texture);
    return texture;
}

RefCntAutoPtr<ITexture> createDepthTexture(
    VulkanGpuDevice& gpu,
    float clearDepth)
{
    auto texture = createTexture(
        gpu.device(), "Post process contract depth",
        TEX_FORMAT_D32_FLOAT,
        BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE,
        nullptr, 0);
    if (!texture) {
        return {};
    }

    auto* dsv = texture->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    if (!dsv) {
        return {};
    }
    gpu.context()->ClearDepthStencil(
        dsv, CLEAR_DEPTH_FLAG, clearDepth, 0,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    return texture;
}

bool readbackRgba32f(
    VulkanGpuDevice& gpu,
    ITexture* source,
    std::array<float, kPixelCount * 4>& pixels)
{
    auto desc = source->GetDesc();
    desc.Name = "Post process contract readback";
    desc.Usage = USAGE_STAGING;
    desc.BindFlags = BIND_NONE;
    desc.CPUAccessFlags = CPU_ACCESS_READ;

    RefCntAutoPtr<ITexture> staging;
    gpu.device()->CreateTexture(desc, nullptr, &staging);
    if (!staging) {
        return false;
    }

    CopyTextureAttribs copy;
    copy.pSrcTexture = source;
    copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    copy.pDstTexture = staging;
    copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    gpu.context()->CopyTexture(copy);
    gpu.flushAndWait();

    MappedTextureSubresource mapped;
    gpu.context()->MapTextureSubresource(
        staging, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
    if (!mapped.pData) {
        return false;
    }

    for (unsigned y = 0; y < kHeight; ++y) {
        const auto* row = static_cast<const std::byte*>(mapped.pData) +
                          static_cast<std::size_t>(y) * mapped.Stride;
        std::memcpy(pixels.data() + static_cast<std::size_t>(y) * kWidth * 4,
                    row, static_cast<std::size_t>(kWidth) * 4 * sizeof(float));
    }
    gpu.context()->UnmapTextureSubresource(staging, 0, 0);
    return true;
}

std::size_t pixelOffset(unsigned x, unsigned y)
{
    return (static_cast<std::size_t>(y) * kWidth + x) * 4;
}

} // namespace

TEST(PostProcessPassContractTest, PassesRejectUseBeforeInitialization)
{
    Artifact::DepthOfFieldPass depthOfField;
    Artifact::MotionBlurPass motionBlur;
    Artifact::DepthOfFieldSettings dofSettings;
    Artifact::MotionBlurSettings motionSettings;

    EXPECT_FALSE(depthOfField.ready());
    EXPECT_FALSE(depthOfField.initialize(nullptr, nullptr));
    EXPECT_FALSE(depthOfField.apply(
        nullptr, nullptr, nullptr, nullptr, kWidth, kHeight, dofSettings));

    EXPECT_FALSE(motionBlur.ready());
    EXPECT_FALSE(motionBlur.initialize(nullptr, nullptr));
    EXPECT_FALSE(motionBlur.apply(
        nullptr, nullptr, nullptr, nullptr, nullptr, kWidth, kHeight,
        motionSettings));
}

TEST_F(PostProcessPassGpuContractTest, DepthOfFieldBlursAnOffFocusImpulse)
{
    Artifact::DepthOfFieldPass pass;
    ASSERT_TRUE(pass.initialize(gpu_.device(), gpu_.context()));
    ASSERT_TRUE(pass.ready());

    std::array<float, kPixelCount * 4> color{};
    const auto centerInput = pixelOffset(kWidth / 2, kHeight / 2);
    color[centerInput] = 1.0f;
    color[centerInput + 3] = 1.0f;

    auto colorTexture = createTexture(
        gpu_.device(), "DOF contract impulse", TEX_FORMAT_RGBA32_FLOAT,
        BIND_SHADER_RESOURCE, color.data(), kWidth * 4 * sizeof(float));
    auto depthTexture = createDepthTexture(gpu_, 0.0f);
    auto outputTexture = createTexture(
        gpu_.device(), "DOF contract output", TEX_FORMAT_RGBA32_FLOAT,
        BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE, nullptr, 0);
    ASSERT_NE(colorTexture, nullptr);
    ASSERT_NE(depthTexture, nullptr);
    ASSERT_NE(outputTexture, nullptr);

    auto* colorSrv = colorTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* depthSrv = depthTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* outputUav = outputTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    ASSERT_NE(colorSrv, nullptr);
    ASSERT_NE(depthSrv, nullptr);
    ASSERT_NE(outputUav, nullptr);

    Artifact::DepthOfFieldSettings settings;
    settings.enabled = true;
    settings.focusDistance.value = 10.0f;
    settings.nearClip.value = 1.0f;
    settings.farClip.value = 100.0f;
    settings.maxCocRadius.value = 3.0f;
    settings.cocScale.value = 1.0f;
    settings.sampleCount = 64;

    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));

    std::array<float, kPixelCount * 4> output{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, output));
    const auto center = pixelOffset(kWidth / 2, kHeight / 2);
    const auto adjacent = pixelOffset(kWidth / 2 + 1, kHeight / 2);
    EXPECT_LT(output[center], 1.0f);
    EXPECT_GT(output[adjacent], 0.0f);
    EXPECT_LT(output[center + 3], 1.0f);
    EXPECT_GT(output[adjacent + 3], 0.0f);
}

TEST_F(PostProcessPassGpuContractTest, DisabledPassesLeaveOutputPixelsUntouched)
{
    std::array<float, kPixelCount * 4> input{};
    std::array<float, kPixelCount * 2> velocity{};
    std::array<float, kPixelCount * 4> initialOutput{};
    for (std::size_t pixel = 0; pixel < kPixelCount; ++pixel) {
        input[pixel * 4 + 3] = 1.0f;
        initialOutput[pixel * 4 + 0] = 0.125f;
        initialOutput[pixel * 4 + 1] = 0.25f;
        initialOutput[pixel * 4 + 2] = 0.5f;
        initialOutput[pixel * 4 + 3] = 0.75f;
    }
    auto inputTexture = createTexture(
        gpu_.device(), "Disabled post-process input", TEX_FORMAT_RGBA32_FLOAT,
        BIND_SHADER_RESOURCE, input.data(), kWidth * 4 * sizeof(float));
    auto velocityTexture = createTexture(
        gpu_.device(), "Disabled motion blur velocity", TEX_FORMAT_RG32_FLOAT,
        BIND_SHADER_RESOURCE, velocity.data(), kWidth * 2 * sizeof(float));
    auto depthTexture = createDepthTexture(gpu_, 0.5f);
    auto outputTexture = createTexture(
        gpu_.device(), "Disabled post-process output", TEX_FORMAT_RGBA32_FLOAT,
        BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE, initialOutput.data(),
        kWidth * 4 * sizeof(float));
    ASSERT_NE(inputTexture, nullptr);
    ASSERT_NE(velocityTexture, nullptr);
    ASSERT_NE(depthTexture, nullptr);
    ASSERT_NE(outputTexture, nullptr);

    auto* inputSrv = inputTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* velocitySrv = velocityTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* depthSrv = depthTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* outputSrv = outputTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* outputUav = outputTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    ASSERT_NE(inputSrv, nullptr);
    ASSERT_NE(velocitySrv, nullptr);
    ASSERT_NE(depthSrv, nullptr);
    ASSERT_NE(outputSrv, nullptr);
    ASSERT_NE(outputUav, nullptr);

    Artifact::DepthOfFieldPass depthOfField;
    ASSERT_TRUE(depthOfField.initialize(gpu_.device(), gpu_.context()));
    Artifact::DepthOfFieldSettings depthSettings;
    EXPECT_FALSE(depthOfField.apply(gpu_.context(), inputSrv, depthSrv,
                                    outputUav, kWidth, kHeight, depthSettings));

    Artifact::MotionBlurPass motionBlur;
    ASSERT_TRUE(motionBlur.initialize(gpu_.device(), gpu_.context()));
    Artifact::MotionBlurSettings motionSettings;
    EXPECT_FALSE(motionBlur.apply(gpu_.context(), inputSrv, velocitySrv,
                                  depthSrv, outputUav, kWidth, kHeight,
                                  motionSettings));

    std::array<float, kPixelCount * 4> output{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, output));
    for (std::size_t index = 0; index < output.size(); ++index) {
        EXPECT_FLOAT_EQ(output[index], initialOutput[index])
            << "component=" << index;
    }
}

TEST_F(PostProcessPassGpuContractTest, PassesRejectZeroDimensionsWithoutTouchingOutput)
{
    std::array<float, kPixelCount * 4> input{};
    std::array<float, kPixelCount * 2> velocity{};
    std::array<float, kPixelCount * 4> initialOutput{};
    for (std::size_t pixel = 0; pixel < kPixelCount; ++pixel) {
        input[pixel * 4 + 3] = 1.0f;
        initialOutput[pixel * 4 + 0] = 0.125f;
        initialOutput[pixel * 4 + 1] = 0.25f;
        initialOutput[pixel * 4 + 2] = 0.5f;
        initialOutput[pixel * 4 + 3] = 0.75f;
    }
    auto inputTexture = createTexture(
        gpu_.device(), "Zero-size post-process input", TEX_FORMAT_RGBA32_FLOAT,
        BIND_SHADER_RESOURCE, input.data(), kWidth * 4 * sizeof(float));
    auto velocityTexture = createTexture(
        gpu_.device(), "Zero-size motion blur velocity", TEX_FORMAT_RG32_FLOAT,
        BIND_SHADER_RESOURCE, velocity.data(), kWidth * 2 * sizeof(float));
    auto depthTexture = createDepthTexture(gpu_, 0.5f);
    auto outputTexture = createTexture(
        gpu_.device(), "Zero-size post-process output", TEX_FORMAT_RGBA32_FLOAT,
        BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE, initialOutput.data(),
        kWidth * 4 * sizeof(float));
    ASSERT_NE(inputTexture, nullptr);
    ASSERT_NE(velocityTexture, nullptr);
    ASSERT_NE(depthTexture, nullptr);
    ASSERT_NE(outputTexture, nullptr);

    auto* inputSrv = inputTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* velocitySrv = velocityTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* depthSrv = depthTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* outputUav = outputTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    ASSERT_NE(inputSrv, nullptr);
    ASSERT_NE(velocitySrv, nullptr);
    ASSERT_NE(depthSrv, nullptr);
    ASSERT_NE(outputUav, nullptr);

    Artifact::DepthOfFieldPass depthOfField;
    ASSERT_TRUE(depthOfField.initialize(gpu_.device(), gpu_.context()));
    Artifact::DepthOfFieldSettings depthSettings;
    depthSettings.enabled = true;
    EXPECT_FALSE(depthOfField.apply(gpu_.context(), inputSrv, depthSrv, outputUav,
                                    0, kHeight, depthSettings));
    EXPECT_FALSE(depthOfField.apply(gpu_.context(), inputSrv, depthSrv, outputUav,
                                    kWidth, 0, depthSettings));

    Artifact::MotionBlurPass motionBlur;
    ASSERT_TRUE(motionBlur.initialize(gpu_.device(), gpu_.context()));
    Artifact::MotionBlurSettings motionSettings;
    motionSettings.enabled = true;
    EXPECT_FALSE(motionBlur.apply(gpu_.context(), inputSrv, velocitySrv, depthSrv,
                                  outputUav, 0, kHeight, motionSettings));
    EXPECT_FALSE(motionBlur.apply(gpu_.context(), inputSrv, velocitySrv, depthSrv,
                                  outputUav, kWidth, 0, motionSettings));

    std::array<float, kPixelCount * 4> output{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, output));
    for (std::size_t index = 0; index < output.size(); ++index) {
        EXPECT_FLOAT_EQ(output[index], initialOutput[index]) << "component=" << index;
    }
}

TEST_F(PostProcessPassGpuContractTest, DepthOfFieldThinLensModelRespondsToAperture)
{
    Artifact::DepthOfFieldPass pass;
    ASSERT_TRUE(pass.initialize(gpu_.device(), gpu_.context()));

    std::array<float, kPixelCount * 4> color{};
    const auto centerInput = pixelOffset(kWidth / 2, kHeight / 2);
    color[centerInput] = 1.0f;
    color[centerInput + 3] = 1.0f;
    auto colorTexture = createTexture(
        gpu_.device(), "DOF thin-lens impulse", TEX_FORMAT_RGBA32_FLOAT,
        BIND_SHADER_RESOURCE, color.data(), kWidth * 4 * sizeof(float));
    auto depthTexture = createDepthTexture(gpu_, 0.0f);
    auto outputTexture = createTexture(
        gpu_.device(), "DOF thin-lens output", TEX_FORMAT_RGBA32_FLOAT,
        BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE, nullptr, 0);
    ASSERT_NE(colorTexture, nullptr);
    ASSERT_NE(depthTexture, nullptr);
    ASSERT_NE(outputTexture, nullptr);
    auto* colorSrv = colorTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* depthSrv = depthTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* outputUav = outputTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    ASSERT_NE(colorSrv, nullptr);
    ASSERT_NE(depthSrv, nullptr);
    ASSERT_NE(outputUav, nullptr);

    Artifact::DepthOfFieldSettings settings;
    settings.enabled = true;
    settings.focusDistance.value = 10.0f;
    settings.nearClip.value = 1.0f;
    settings.farClip.value = 100.0f;
    settings.maxCocRadius.value = 3.0f;
    settings.sampleCount = 64;
    settings.fStop.value = 0.1f;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> stoppedDown{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, stoppedDown));

    settings.fStop.value = 2.0f;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> wideAperture{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, wideAperture));

    const auto center = pixelOffset(kWidth / 2, kHeight / 2);
    const auto adjacent = pixelOffset(kWidth / 2 + 1, kHeight / 2);
    EXPECT_FLOAT_EQ(stoppedDown[center], 1.0f);
    EXPECT_FLOAT_EQ(stoppedDown[adjacent], 0.0f);
    EXPECT_LT(wideAperture[center], stoppedDown[center]);
    EXPECT_GT(wideAperture[adjacent], stoppedDown[adjacent]);
    EXPECT_GT(wideAperture[adjacent + 3], 0.0f);
}

TEST_F(PostProcessPassGpuContractTest, DepthOfFieldClampsCocScaleAndSampleCount)
{
    Artifact::DepthOfFieldPass pass;
    ASSERT_TRUE(pass.initialize(gpu_.device(), gpu_.context()));

    std::array<float, kPixelCount * 4> color{};
    const auto centerInput = pixelOffset(kWidth / 2, kHeight / 2);
    color[centerInput] = 1.0f;
    color[centerInput + 3] = 1.0f;
    auto colorTexture = createTexture(
        gpu_.device(), "DOF CoC scale impulse", TEX_FORMAT_RGBA32_FLOAT,
        BIND_SHADER_RESOURCE, color.data(), kWidth * 4 * sizeof(float));
    auto depthTexture = createDepthTexture(gpu_, 0.0f);
    auto outputTexture = createTexture(
        gpu_.device(), "DOF CoC scale output", TEX_FORMAT_RGBA32_FLOAT,
        BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE, nullptr, 0);
    ASSERT_NE(colorTexture, nullptr);
    ASSERT_NE(depthTexture, nullptr);
    ASSERT_NE(outputTexture, nullptr);
    auto* colorSrv = colorTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* depthSrv = depthTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* outputUav = outputTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    ASSERT_NE(colorSrv, nullptr);
    ASSERT_NE(depthSrv, nullptr);
    ASSERT_NE(outputUav, nullptr);

    Artifact::DepthOfFieldSettings settings;
    settings.enabled = true;
    settings.focusDistance.value = 10.0f;
    settings.nearClip.value = 1.0f;
    settings.farClip.value = 100.0f;
    settings.maxCocRadius.value = 3.0f;
    settings.sampleCount = 64;
    settings.cocScale.value = 0.0f;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> noBlur{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, noBlur));

    settings.cocScale.value = 1.0f;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> atMaximum{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, atMaximum));

    settings.cocScale.value = 2.0f;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> aboveMaximum{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, aboveMaximum));

    const auto center = pixelOffset(kWidth / 2, kHeight / 2);
    const auto adjacent = pixelOffset(kWidth / 2 + 1, kHeight / 2);
    EXPECT_FLOAT_EQ(noBlur[center], 1.0f);
    EXPECT_FLOAT_EQ(noBlur[adjacent], 0.0f);
    EXPECT_LT(atMaximum[center], noBlur[center]);
    EXPECT_GT(atMaximum[adjacent], noBlur[adjacent]);
    for (std::size_t component = 0; component < atMaximum.size(); ++component) {
        EXPECT_FLOAT_EQ(aboveMaximum[component], atMaximum[component])
            << "component=" << component;
    }

    settings.cocScale.value = 1.0f;
    settings.sampleCount = 0;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> belowMinimumSamples{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, belowMinimumSamples));
    settings.sampleCount = 4;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> minimumSamples{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, minimumSamples));
    settings.sampleCount = 64;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> maximumSamples{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, maximumSamples));
    settings.sampleCount = 100;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> aboveMaximumSamples{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, aboveMaximumSamples));
    for (std::size_t component = 0; component < minimumSamples.size(); ++component) {
        EXPECT_FLOAT_EQ(belowMinimumSamples[component], minimumSamples[component])
            << "minimum component=" << component;
        EXPECT_FLOAT_EQ(aboveMaximumSamples[component], maximumSamples[component])
            << "maximum component=" << component;
    }
}

TEST_F(PostProcessPassGpuContractTest, DepthOfFieldClampsNegativeCocRadiusToZero)
{
    Artifact::DepthOfFieldPass pass;
    ASSERT_TRUE(pass.initialize(gpu_.device(), gpu_.context()));

    std::array<float, kPixelCount * 4> color{};
    const auto centerInput = pixelOffset(kWidth / 2, kHeight / 2);
    color[centerInput] = 1.0f;
    color[centerInput + 3] = 1.0f;
    auto colorTexture = createTexture(
        gpu_.device(), "DOF negative CoC radius impulse", TEX_FORMAT_RGBA32_FLOAT,
        BIND_SHADER_RESOURCE, color.data(), kWidth * 4 * sizeof(float));
    auto depthTexture = createDepthTexture(gpu_, 0.0f);
    auto outputTexture = createTexture(
        gpu_.device(), "DOF negative CoC radius output", TEX_FORMAT_RGBA32_FLOAT,
        BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE, nullptr, 0);
    ASSERT_NE(colorTexture, nullptr);
    ASSERT_NE(depthTexture, nullptr);
    ASSERT_NE(outputTexture, nullptr);
    auto* colorSrv = colorTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* depthSrv = depthTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* outputUav = outputTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    ASSERT_NE(colorSrv, nullptr);
    ASSERT_NE(depthSrv, nullptr);
    ASSERT_NE(outputUav, nullptr);

    Artifact::DepthOfFieldSettings settings;
    settings.enabled = true;
    settings.focusDistance.value = 10.0f;
    settings.nearClip.value = 1.0f;
    settings.farClip.value = 100.0f;
    settings.maxCocRadius.value = -4.0f;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> negativeRadius{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, negativeRadius));

    settings.maxCocRadius.value = 0.0f;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> zeroRadius{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, zeroRadius));

    const auto center = pixelOffset(kWidth / 2, kHeight / 2);
    const auto adjacent = pixelOffset(kWidth / 2 + 1, kHeight / 2);
    EXPECT_FLOAT_EQ(negativeRadius[center], 1.0f);
    EXPECT_FLOAT_EQ(negativeRadius[adjacent], 0.0f);
    for (std::size_t component = 0; component < zeroRadius.size(); ++component) {
        EXPECT_FLOAT_EQ(negativeRadius[component], zeroRadius[component])
            << "component=" << component;
    }
}

TEST_F(PostProcessPassGpuContractTest, DepthOfFieldPreservesAnInFocusImpulse)
{
    Artifact::DepthOfFieldPass pass;
    ASSERT_TRUE(pass.initialize(gpu_.device(), gpu_.context()));

    std::array<float, kPixelCount * 4> color{};
    for (std::size_t pixel = 0; pixel < kPixelCount; ++pixel) {
        color[pixel * 4 + 3] = 1.0f;
    }
    color[pixelOffset(kWidth / 2, kHeight / 2)] = 1.0f;
    auto colorTexture = createTexture(
        gpu_.device(), "DOF in-focus contract impulse", TEX_FORMAT_RGBA32_FLOAT,
        BIND_SHADER_RESOURCE, color.data(), kWidth * 4 * sizeof(float));
    // (far - near * far / focus) / (far - near) maps to the focus plane.
    // With near=1, far=100 and focus=10, that depth is approximately 0.90909.
    auto depthTexture = createDepthTexture(gpu_, 0.90909094f);
    auto outputTexture = createTexture(
        gpu_.device(), "DOF in-focus contract output", TEX_FORMAT_RGBA32_FLOAT,
        BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE, nullptr, 0);
    ASSERT_NE(colorTexture, nullptr);
    ASSERT_NE(depthTexture, nullptr);
    ASSERT_NE(outputTexture, nullptr);
    auto* colorSrv = colorTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* depthSrv = depthTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* outputUav = outputTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    ASSERT_NE(colorSrv, nullptr);
    ASSERT_NE(depthSrv, nullptr);
    ASSERT_NE(outputUav, nullptr);

    Artifact::DepthOfFieldSettings settings;
    settings.enabled = true;
    settings.focusDistance.value = 10.0f;
    settings.nearClip.value = 1.0f;
    settings.farClip.value = 100.0f;
    settings.maxCocRadius.value = 3.0f;
    settings.sampleCount = 8;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, depthSrv, outputUav,
                           kWidth, kHeight, settings));

    std::array<float, kPixelCount * 4> output{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, output));
    const auto center = pixelOffset(kWidth / 2, kHeight / 2);
    EXPECT_FLOAT_EQ(output[center], 1.0f);
    EXPECT_FLOAT_EQ(output[pixelOffset(kWidth / 2 + 1, kHeight / 2)], 0.0f);
    EXPECT_FLOAT_EQ(output[center + 3], 1.0f);
}

TEST_F(PostProcessPassGpuContractTest, MotionBlurFollowsVelocityAndCopiesZeroVelocity)
{
    Artifact::MotionBlurPass pass;
    ASSERT_TRUE(pass.initialize(gpu_.device(), gpu_.context()));
    ASSERT_TRUE(pass.ready());

    std::array<float, kPixelCount * 4> color{};
    std::array<float, kPixelCount * 2> velocity{};
    std::array<float, kPixelCount * 2> zeroVelocity{};
    for (std::size_t pixel = 0; pixel < kPixelCount; ++pixel) {
        velocity[pixel * 2] = 2.0f;
    }
    const auto centerInput = pixelOffset(kWidth / 2, kHeight / 2);
    color[centerInput] = 1.0f;
    color[centerInput + 3] = 1.0f;

    auto colorTexture = createTexture(
        gpu_.device(), "Motion blur contract impulse", TEX_FORMAT_RGBA32_FLOAT,
        BIND_SHADER_RESOURCE, color.data(), kWidth * 4 * sizeof(float));
    auto velocityTexture = createTexture(
        gpu_.device(), "Motion blur contract velocity", TEX_FORMAT_RG32_FLOAT,
        BIND_SHADER_RESOURCE, velocity.data(), kWidth * 2 * sizeof(float));
    auto zeroVelocityTexture = createTexture(
        gpu_.device(), "Motion blur zero velocity", TEX_FORMAT_RG32_FLOAT,
        BIND_SHADER_RESOURCE, zeroVelocity.data(), kWidth * 2 * sizeof(float));
    auto depthTexture = createDepthTexture(gpu_, 0.5f);
    auto outputTexture = createTexture(
        gpu_.device(), "Motion blur contract output", TEX_FORMAT_RGBA32_FLOAT,
        BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE, nullptr, 0);
    ASSERT_NE(colorTexture, nullptr);
    ASSERT_NE(velocityTexture, nullptr);
    ASSERT_NE(zeroVelocityTexture, nullptr);
    ASSERT_NE(depthTexture, nullptr);
    ASSERT_NE(outputTexture, nullptr);

    auto* colorSrv = colorTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* velocitySrv = velocityTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* zeroVelocitySrv = zeroVelocityTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* depthSrv = depthTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* outputUav = outputTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    ASSERT_NE(colorSrv, nullptr);
    ASSERT_NE(velocitySrv, nullptr);
    ASSERT_NE(zeroVelocitySrv, nullptr);
    ASSERT_NE(depthSrv, nullptr);
    ASSERT_NE(outputUav, nullptr);

    Artifact::MotionBlurSettings settings;
    settings.enabled = true;
    settings.shutterAngle = 360.0f;
    settings.shutterPhase = 0.0f;
    settings.velocityScale = 1.0f;
    settings.sampleCount = 8;

    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, velocitySrv, depthSrv,
                           outputUav, kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> blurred{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, blurred));

    const auto center = pixelOffset(kWidth / 2, kHeight / 2);
    const auto adjacent = pixelOffset(kWidth / 2 + 1, kHeight / 2);
    EXPECT_LT(blurred[center], 1.0f);
    EXPECT_GT(blurred[adjacent], 0.0f);
    EXPECT_LT(blurred[center + 3], 1.0f);
    EXPECT_GT(blurred[adjacent + 3], 0.0f);

    settings.sampleCount = 32;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, velocitySrv, depthSrv,
                           outputUav, kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> maximumSamples{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, maximumSamples));
    settings.sampleCount = 100;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, velocitySrv, depthSrv,
                           outputUav, kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> overMaximumSamples{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, overMaximumSamples));
    for (std::size_t component = 0; component < maximumSamples.size(); ++component) {
        EXPECT_FLOAT_EQ(overMaximumSamples[component], maximumSamples[component])
            << "component=" << component;
    }

    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, zeroVelocitySrv, depthSrv,
                           outputUav, kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> unblurred{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, unblurred));
    EXPECT_FLOAT_EQ(unblurred[center], 1.0f);
    EXPECT_FLOAT_EQ(unblurred[adjacent], 0.0f);
    EXPECT_FLOAT_EQ(unblurred[center + 3], 1.0f);
    EXPECT_FLOAT_EQ(unblurred[adjacent + 3], 0.0f);

    settings.shutterAngle = 0.0f;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, velocitySrv, depthSrv,
                           outputUav, kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> zeroShutter{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, zeroShutter));
    EXPECT_FLOAT_EQ(zeroShutter[center], 1.0f);
    EXPECT_FLOAT_EQ(zeroShutter[adjacent], 0.0f);
    EXPECT_FLOAT_EQ(zeroShutter[center + 3], 1.0f);
    EXPECT_FLOAT_EQ(zeroShutter[adjacent + 3], 0.0f);

    settings.shutterAngle = 360.0f;
    settings.sampleCount = 0;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, velocitySrv, depthSrv,
                           outputUav, kWidth, kHeight, settings));
    std::array<float, kPixelCount * 4> zeroSamples{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, zeroSamples));
    EXPECT_FLOAT_EQ(zeroSamples[center], 1.0f);
    EXPECT_FLOAT_EQ(zeroSamples[adjacent], 0.0f);
    EXPECT_FLOAT_EQ(zeroSamples[center + 3], 1.0f);
    EXPECT_FLOAT_EQ(zeroSamples[adjacent + 3], 0.0f);
}

TEST_F(PostProcessPassGpuContractTest, MotionBlurRejectsSamplesAcrossDepthEdges)
{
    Artifact::MotionBlurPass pass;
    ASSERT_TRUE(pass.initialize(gpu_.device(), gpu_.context()));
    ASSERT_TRUE(pass.ready());

    std::array<float, kPixelCount * 4> color{};
    std::array<float, kPixelCount * 2> velocity{};
    std::array<float, kPixelCount> depth{};
    for (std::size_t pixel = 0; pixel < kPixelCount; ++pixel) {
        color[pixel * 4 + 3] = 1.0f;
        velocity[pixel * 2] = 2.0f;
        depth[pixel] = 0.5f;
    }
    const auto foreground = pixelOffset(kWidth / 2, kHeight / 2);
    color[foreground] = 1.0f;
    depth[foreground / 4] = 0.0f;

    auto colorTexture = createTexture(
        gpu_.device(), "Motion blur depth-edge color", TEX_FORMAT_RGBA32_FLOAT,
        BIND_SHADER_RESOURCE, color.data(), kWidth * 4 * sizeof(float));
    auto velocityTexture = createTexture(
        gpu_.device(), "Motion blur depth-edge velocity", TEX_FORMAT_RG32_FLOAT,
        BIND_SHADER_RESOURCE, velocity.data(), kWidth * 2 * sizeof(float));
    auto depthTexture = createTexture(
        gpu_.device(), "Motion blur depth-edge depth", TEX_FORMAT_R32_FLOAT,
        BIND_SHADER_RESOURCE, depth.data(), kWidth * sizeof(float));
    auto outputTexture = createTexture(
        gpu_.device(), "Motion blur depth-edge output", TEX_FORMAT_RGBA32_FLOAT,
        BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE, nullptr, 0);
    ASSERT_NE(colorTexture, nullptr);
    ASSERT_NE(velocityTexture, nullptr);
    ASSERT_NE(depthTexture, nullptr);
    ASSERT_NE(outputTexture, nullptr);

    auto* colorSrv = colorTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* velocitySrv = velocityTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* depthSrv = depthTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* outputUav = outputTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    ASSERT_NE(colorSrv, nullptr);
    ASSERT_NE(velocitySrv, nullptr);
    ASSERT_NE(depthSrv, nullptr);
    ASSERT_NE(outputUav, nullptr);

    Artifact::MotionBlurSettings settings;
    settings.enabled = true;
    settings.shutterAngle = 360.0f;
    settings.velocityScale = 1.0f;
    settings.sampleCount = 16;
    ASSERT_TRUE(pass.apply(gpu_.context(), colorSrv, velocitySrv, depthSrv,
                           outputUav, kWidth, kHeight, settings));

    std::array<float, kPixelCount * 4> output{};
    ASSERT_TRUE(readbackRgba32f(gpu_, outputTexture, output));
    const auto background = pixelOffset(kWidth / 2 + 1, kHeight / 2);
    EXPECT_FLOAT_EQ(output[foreground], 1.0f)
        << "Foreground samples must not be diluted by a different depth surface";
    EXPECT_FLOAT_EQ(output[background], 0.0f)
        << "Foreground color must not bleed across the depth discontinuity";
}
