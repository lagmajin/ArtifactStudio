#include <gtest/gtest.h>
#include <QCryptographicHash>
#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <QVariant>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

import ImageProcessing;
import Graphics.SurfaceColorContract;
import Artifact.Effect.Abstract;
import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.Keying.DifferenceKey;
import Artifact.Effect.Keying.IBKKeyer;
import Artifact.Effect.Keying.LumaKey;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

using namespace ArtifactCore::Keying;
using ArtifactCore::SurfaceAlphaMode;
using ArtifactCore::SurfaceChannelOrder;
using Artifact::ChromaKeyEffect;
using Artifact::DifferenceKeyEffect;
using Artifact::IBKKeyerEffect;
using Artifact::LumaKeyEffect;
using ArtifactCore::ImageF32x4RGBAWithCache;
using ArtifactCore::ImageF32x4_RGBA;

namespace {

float alphaAt(const std::array<float, 12>& pixels, std::size_t pixelIndex)
{
    return pixels[pixelIndex * 4 + 3];
}

} // namespace

TEST(KeyingEffectContractTest, ChromaKeyRemovesScreenAndKeepsForeground)
{
    // Green screen, red subject, and a partially transparent green pixel.
    const std::array<float, 12> source{
        0.0f, 1.0f, 0.0f, 1.0f,
        0.9f, 0.08f, 0.04f, 1.0f,
        0.0f, 1.0f, 0.0f, 0.4f};
    std::array<float, 12> output{};
    ChromaKeyParams params;
    params.despillStrength = 0.0f;

    ASSERT_TRUE(processChromaKey(
        ChromaKeyBuffers{source.data(), output.data(), 3, 1}, params));

    EXPECT_LT(alphaAt(output, 0), 0.05f);
    EXPECT_GT(alphaAt(output, 1), 0.95f);
    EXPECT_LT(alphaAt(output, 2), 0.05f);
}

TEST(KeyingEffectContractTest, ChromaKeyTreatsRgbaAndBgraPixelsEquivalently)
{
    const std::array<float, 8> rgbaSource{
        0.0f, 1.0f, 0.0f, 1.0f,
        0.9f, 0.08f, 0.04f, 0.7f};
    const std::array<float, 8> bgraSource{
        0.0f, 1.0f, 0.0f, 1.0f,
        0.04f, 0.08f, 0.9f, 0.7f};
    std::array<float, 8> rgbaOutput{};
    std::array<float, 8> bgraOutput{};

    ASSERT_TRUE(processChromaKey(
        ChromaKeyBuffers{rgbaSource.data(), rgbaOutput.data(), 2, 1,
                         SurfaceChannelOrder::RGBA}, {}));
    ASSERT_TRUE(processChromaKey(
        ChromaKeyBuffers{bgraSource.data(), bgraOutput.data(), 2, 1,
                         SurfaceChannelOrder::BGRA}, {}));

    for (std::size_t i = 0; i < 2; ++i) {
        EXPECT_NEAR(rgbaOutput[i * 4 + 3], bgraOutput[i * 4 + 3], 1.0e-6f);
        EXPECT_NEAR(rgbaOutput[i * 4], bgraOutput[i * 4 + 2], 1.0e-6f);
        EXPECT_NEAR(rgbaOutput[i * 4 + 1], bgraOutput[i * 4 + 1], 1.0e-6f);
        EXPECT_NEAR(rgbaOutput[i * 4 + 2], bgraOutput[i * 4], 1.0e-6f);
    }
}

TEST(KeyingEffectContractTest, LumaAndDifferenceKeyTreatRgbaAndBgraEquivalently)
{
    const std::array<float, 8> rgbaSource{
        0.8f, 0.4f, 0.2f, 1.0f,
        0.1f, 0.6f, 0.9f, 0.7f};
    const std::array<float, 8> bgraSource{
        0.2f, 0.4f, 0.8f, 1.0f,
        0.9f, 0.6f, 0.1f, 0.7f};
    std::array<float, 8> lumaRgbaOutput{};
    std::array<float, 8> lumaBgraOutput{};
    std::array<float, 8> differenceRgbaOutput{};
    std::array<float, 8> differenceBgraOutput{};
    LumaKeyParams lumaParams;
    lumaParams.low = 0.25f;
    lumaParams.high = 0.75f;
    lumaParams.softness = 0.2f;
    DifferenceKeyParams differenceParams;
    differenceParams.refR = 0.2f;
    differenceParams.refG = 0.4f;
    differenceParams.refB = 0.8f;
    differenceParams.threshold = 0.15f;
    differenceParams.softness = 0.3f;

    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{rgbaSource.data(), lumaRgbaOutput.data(), 2, 1,
                       SurfaceChannelOrder::RGBA}, lumaParams));
    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{bgraSource.data(), lumaBgraOutput.data(), 2, 1,
                       SurfaceChannelOrder::BGRA}, lumaParams));
    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{rgbaSource.data(), differenceRgbaOutput.data(), 2, 1,
                             SurfaceChannelOrder::RGBA}, differenceParams));
    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{bgraSource.data(), differenceBgraOutput.data(), 2, 1,
                             SurfaceChannelOrder::BGRA}, differenceParams));

    for (std::size_t pixel = 0; pixel < 2; ++pixel) {
        EXPECT_NEAR(lumaRgbaOutput[pixel * 4 + 3],
                    lumaBgraOutput[pixel * 4 + 3], 1.0e-6f);
        EXPECT_NEAR(differenceRgbaOutput[pixel * 4 + 3],
                    differenceBgraOutput[pixel * 4 + 3], 1.0e-6f);
        EXPECT_NEAR(lumaRgbaOutput[pixel * 4],
                    lumaBgraOutput[pixel * 4 + 2], 1.0e-6f);
        EXPECT_NEAR(lumaRgbaOutput[pixel * 4 + 1],
                    lumaBgraOutput[pixel * 4 + 1], 1.0e-6f);
        EXPECT_NEAR(lumaRgbaOutput[pixel * 4 + 2],
                    lumaBgraOutput[pixel * 4], 1.0e-6f);
        EXPECT_NEAR(differenceRgbaOutput[pixel * 4],
                    differenceBgraOutput[pixel * 4 + 2], 1.0e-6f);
        EXPECT_NEAR(differenceRgbaOutput[pixel * 4 + 1],
                    differenceBgraOutput[pixel * 4 + 1], 1.0e-6f);
        EXPECT_NEAR(differenceRgbaOutput[pixel * 4 + 2],
                    differenceBgraOutput[pixel * 4], 1.0e-6f);
    }
}

TEST(KeyingEffectContractTest, ChromaKeyMatchesEquivalentStraightAndPremultipliedCpuInputs)
{
    const std::array<float, 36> straightSource{
        0.0f, 0.8f, 0.0f, 0.5f,  0.1f, 0.9f, 0.1f, 0.7f,  0.8f, 0.2f, 0.1f, 0.5f,
        0.0f, 1.0f, 0.0f, 1.0f,  0.7f, 0.3f, 0.2f, 0.6f,  0.0f, 0.7f, 0.0f, 0.8f,
        0.2f, 0.8f, 0.1f, 0.3f,  0.9f, 0.1f, 0.05f, 1.0f, 0.0f, 0.9f, 0.0f, 0.4f};
    std::array<float, 36> premultipliedSource{};
    for (std::size_t i = 0; i < 9; ++i) {
        const float alpha = straightSource[i * 4 + 3];
        for (std::size_t channel = 0; channel < 3; ++channel) {
            premultipliedSource[i * 4 + channel] =
                straightSource[i * 4 + channel] * alpha;
        }
        premultipliedSource[i * 4 + 3] = alpha;
    }
    std::array<float, 36> straightOutput{};
    std::array<float, 36> premultipliedOutput{};
    ChromaKeyParams params;
    params.despillStrength = 0.0f;
    params.matteBlur = 1.0f;
    params.choke = 0.25f;
    params.lightWrapIntensity = 0.5f;
    params.lightWrapWidth = 2.0f;

    ASSERT_TRUE(processChromaKey(
        ChromaKeyBuffers{straightSource.data(), straightOutput.data(), 3, 3,
                         SurfaceChannelOrder::RGBA, SurfaceAlphaMode::Straight},
        params));
    ASSERT_TRUE(processChromaKey(
        ChromaKeyBuffers{premultipliedSource.data(), premultipliedOutput.data(), 3, 3,
                         SurfaceChannelOrder::RGBA, SurfaceAlphaMode::Premultiplied},
        params));

    for (std::size_t i = 0; i < 9; ++i) {
        const float alpha = straightOutput[i * 4 + 3];
        EXPECT_NEAR(alpha, premultipliedOutput[i * 4 + 3], 1.0e-6f)
            << "pixel " << i;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(premultipliedOutput[i * 4 + channel],
                        straightOutput[i * 4 + channel] * alpha, 1.0e-6f)
                << "pixel " << i << ", channel " << channel;
        }
    }
    EXPECT_FLOAT_EQ(
        ChromaKeyGpuParams::fromParams(params, SurfaceAlphaMode::Straight)
            .sourcePremultiplied,
        0.0f);
    EXPECT_FLOAT_EQ(
        ChromaKeyGpuParams::fromParams(params, SurfaceAlphaMode::Premultiplied)
            .sourcePremultiplied,
        1.0f);
}

TEST(KeyingEffectContractTest, ChromaGpuParameterConversionSanitizesInvalidValues)
{
    ChromaKeyParams params;
    params.keyColor.x = -0.5f;
    params.keyColor.y = std::numeric_limits<float>::quiet_NaN();
    params.keyColor.z = 1.5f;
    params.hueTolerance = std::numeric_limits<float>::quiet_NaN();
    params.edgeSoftness = 0.0f;
    params.clipBlack = 0.8f;
    params.clipWhite = 0.2f;
    params.despillStrength = std::numeric_limits<float>::infinity();
    params.despillMode = 99;
    params.viewMode = -1;
    params.choke = -2.0f;
    params.matteBlur = std::numeric_limits<float>::quiet_NaN();
    params.detailRecovery = std::numeric_limits<float>::infinity();
    params.lightWrapIntensity = -1.0f;
    params.lightWrapWidth = std::numeric_limits<float>::infinity();

    const auto gpuParams = ChromaKeyGpuParams::fromParams(
        params, SurfaceAlphaMode::Premultiplied);
    EXPECT_FLOAT_EQ(gpuParams.keyR, 0.0f);
    EXPECT_FLOAT_EQ(gpuParams.keyG, 1.0f);
    EXPECT_FLOAT_EQ(gpuParams.keyB, 1.0f);
    EXPECT_FLOAT_EQ(gpuParams.sourcePremultiplied, 1.0f);
    EXPECT_FLOAT_EQ(gpuParams.hueTolerance, 0.28f);
    EXPECT_FLOAT_EQ(gpuParams.edgeSoftness, 1.0e-4f);
    EXPECT_FLOAT_EQ(gpuParams.clipBlack, 0.8f);
    EXPECT_NEAR(gpuParams.clipWhite, 0.8001f, 1.0e-7f);
    EXPECT_FLOAT_EQ(gpuParams.despillStrength, 0.7f);
    EXPECT_EQ(gpuParams.despillMode, 3);
    EXPECT_EQ(gpuParams.viewMode, 0);
    EXPECT_FLOAT_EQ(gpuParams.choke, -1.0f);
    EXPECT_FLOAT_EQ(gpuParams.matteBlur, 0.0f);
    EXPECT_FLOAT_EQ(gpuParams.detailRecovery, 0.0f);
    EXPECT_FLOAT_EQ(gpuParams.lightWrapIntensity, 0.0f);
    EXPECT_FLOAT_EQ(gpuParams.lightWrapWidth, 4.0f);
}

TEST(KeyingEffectContractTest, ChromaCoreTargetNeutralDespillUsesTheConfiguredMode)
{
    const std::array<float, 4> source{0.2f, 0.75f, 0.25f, 1.0f};
    std::array<float, 4> luminancePreserveOutput{};
    std::array<float, 4> targetNeutralOutput{};
    ChromaKeyParams params;
    params.despillStrength = 1.0f;
    params.viewMode = 2;

    params.despillMode = 2;
    ASSERT_TRUE(processChromaKey(
        ChromaKeyBuffers{source.data(), luminancePreserveOutput.data(), 1, 1},
        params));
    params.despillMode = 3;
    ASSERT_TRUE(processChromaKey(
        ChromaKeyBuffers{source.data(), targetNeutralOutput.data(), 1, 1}, params));

    EXPECT_FLOAT_EQ(targetNeutralOutput[3], 1.0f);
    EXPECT_NEAR(targetNeutralOutput[0], source[0], 1.0e-6f);
    EXPECT_NEAR(targetNeutralOutput[1], (source[0] + source[2]) * 0.5f, 1.0e-6f);
    EXPECT_NEAR(targetNeutralOutput[2], source[2], 1.0e-6f);
    EXPECT_GT(std::abs(luminancePreserveOutput[1] - targetNeutralOutput[1]), 0.1f);
}

TEST(KeyingEffectContractTest, ChromaKeyMapsPropertiesToResidentGpuNodeParameters)
{
    ChromaKeyEffect effect;
    effect.setHueTolerance(0.31f);
    effect.setEdgeSoftness(0.17f);
    effect.setClipBlack(0.08f);
    effect.setClipWhite(0.91f);
    effect.setDespillStrength(0.63f);
    effect.setDespillMode(2);
    effect.setViewMode(3);
    effect.setChoke(0.0f);
    effect.setMatteBlur(0.0f);
    effect.setDetailRecovery(0.4f);
    effect.setLightWrapIntensity(0.0f);
    effect.setLightWrapWidth(6.0f);

    EXPECT_EQ(effect.gpuRasterEffectDomain(),
              Artifact::GpuRasterEffectDomain::Spatial);
    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    const auto& node = stack.nodes[0];
    EXPECT_EQ(node.kind, Artifact::GpuSpatialEffectKind::ChromaKey);
    EXPECT_FLOAT_EQ(node.parameters[0], 0.0f); // key R
    EXPECT_FLOAT_EQ(node.parameters[1], 1.0f); // key G
    EXPECT_FLOAT_EQ(node.parameters[2], 0.0f); // key B
    EXPECT_FLOAT_EQ(node.parameters[3], 0.31f);
    EXPECT_FLOAT_EQ(node.parameters[4], 0.17f);
    EXPECT_FLOAT_EQ(node.parameters[5], 0.08f);
    EXPECT_FLOAT_EQ(node.parameters[6], 0.91f);
    EXPECT_FLOAT_EQ(node.parameters[7], 0.63f);
    EXPECT_FLOAT_EQ(node.parameters[8], 2.0f);
    EXPECT_FLOAT_EQ(node.parameters[9], 3.0f);
    EXPECT_FLOAT_EQ(node.parameters[10], 0.0f);
    EXPECT_FLOAT_EQ(node.parameters[11], 0.0f);
    EXPECT_FLOAT_EQ(node.parameters[12], 0.4f);
    EXPECT_FLOAT_EQ(node.parameters[13], 0.0f);
    EXPECT_FLOAT_EQ(node.parameters[14], 6.0f);

    effect.setChoke(0.25f);
    EXPECT_EQ(effect.gpuRasterEffectDomain(),
              Artifact::GpuRasterEffectDomain::None);
}

TEST(KeyingEffectContractTest, ChromaKeyProcessesRealLiveActionGreenScreenFrame)
{
    const QString fixturePath =
        qEnvironmentVariable("ARTIFACT_KEYING_REAL_FIXTURE");
    ASSERT_FALSE(fixturePath.isEmpty())
        << "CMake must provide the bundled green-screen fixture path";
    QFile fixtureFile(fixturePath);
    ASSERT_TRUE(fixtureFile.open(QIODevice::ReadOnly))
        << "Could not open the bundled green-screen fixture for hash verification";
    ASSERT_EQ(QCryptographicHash::hash(fixtureFile.readAll(),
                                       QCryptographicHash::Sha256).toHex(),
              QByteArrayLiteral(
                  "132819fcfd733c007edbd6cd1e9c276c3dd0b8b1b56230989e67ef01a107d1a3"))
        << "Update the documented coordinates and expected keying samples if the fixture changes";

    ImageF32x4_RGBA sourceImage;
    ASSERT_TRUE(sourceImage.load(fixturePath))
        << "Could not load the green-screen fixture with Artifact's image loader";
    ASSERT_EQ(sourceImage.width(), 540);
    ASSERT_EQ(sourceImage.height(), 960);
    ImageF32x4RGBAWithCache sourceSurface(sourceImage);
    ImageF32x4RGBAWithCache outputSurface;
    ChromaKeyEffect effect;
    effect.setDespillStrength(0.0f);
    effect.applyCPUOnly(sourceSurface, outputSurface);

    const auto alphaAt = [&](int x, int y) {
        return outputSurface.image().getPixel(x, y).a();
    };
    EXPECT_LT(alphaAt(300, 300), 0.10f)
        << "the green backing should be removed";
    EXPECT_GT(alphaAt(380, 450), 0.85f)
        << "the presenter's dark jacket should remain opaque";
    EXPECT_GT(alphaAt(80, 300), 0.85f)
        << "the non-green studio wall should remain opaque";
    // This fixed curl-fringe sample is meaningful only for the hash-pinned fixture.
    const float hairEdgeAlpha = alphaAt(369, 282);
    EXPECT_GT(hairEdgeAlpha, 0.10f)
        << "the presenter's hair edge should retain partial coverage";
    EXPECT_LT(hairEdgeAlpha, 0.90f)
        << "the hair-edge sample should remain a soft matte transition";
}

TEST(KeyingEffectContractTest, ChromaKeyEffectPropagatesPremultipliedAlphaMode)
{
    const std::array<float, 4> straightPixels{0.3f, 0.55f, 0.35f, 0.5f};
    const std::array<float, 4> premultipliedPixels{0.15f, 0.275f, 0.175f, 0.5f};
    ImageF32x4_RGBA straightImage;
    ImageF32x4_RGBA premultipliedImage;
    straightImage.setFromRGBA32F(straightPixels.data(), 1, 1);
    premultipliedImage.setFromRGBA32F(premultipliedPixels.data(), 1, 1);
    auto straightDescriptor =
        ArtifactCore::SurfaceColorDescriptor::canonicalLinearPremultiplied();
    straightDescriptor.alphaMode = SurfaceAlphaMode::Straight;
    straightImage.setColorDescriptor(straightDescriptor);
    premultipliedImage.setColorDescriptor(
        ArtifactCore::SurfaceColorDescriptor::canonicalLinearPremultiplied());

    ChromaKeyEffect effect;
    effect.setHueTolerance(0.2f);
    effect.setEdgeSoftness(0.5f);
    effect.setDespillStrength(0.0f);
    ImageF32x4RGBAWithCache straightOutput;
    ImageF32x4RGBAWithCache premultipliedOutput;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(straightImage), straightOutput);
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(premultipliedImage), premultipliedOutput);

    const auto straightPixel = straightOutput.image().getPixel(0, 0);
    const auto premultipliedPixel = premultipliedOutput.image().getPixel(0, 0);
    EXPECT_GT(straightPixel.a(), 0.0f);
    EXPECT_LT(straightPixel.a(), 0.5f);
    EXPECT_NEAR(straightPixel.a(), premultipliedPixel.a(), 1.0e-6f);
    EXPECT_NEAR(premultipliedPixel.r(), straightPixel.r() * straightPixel.a(), 1.0e-6f);
    EXPECT_NEAR(premultipliedPixel.g(), straightPixel.g() * straightPixel.a(), 1.0e-6f);
    EXPECT_NEAR(premultipliedPixel.b(), straightPixel.b() * straightPixel.a(), 1.0e-6f);
    EXPECT_EQ(premultipliedOutput.image().colorDescriptor().alphaMode,
              SurfaceAlphaMode::Premultiplied);
}

TEST(KeyingEffectContractTest, LumaKeySelectsTheConfiguredBrightnessBand)
{
    const std::array<float, 12> source{
        0.0f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f};
    std::array<float, 12> output{};
    LumaKeyParams params;
    params.low = 0.3f;
    params.high = 0.7f;
    params.softness = 0.05f;

    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{source.data(), output.data(), 3, 1}, params));

    EXPECT_LT(alphaAt(output, 0), 0.01f);
    EXPECT_GT(alphaAt(output, 1), 0.99f);
    EXPECT_LT(alphaAt(output, 2), 0.01f);
}

TEST(KeyingEffectContractTest, LumaKeySwapsReversedThresholdsAndClampsViewMode)
{
    const std::array<float, 4> source{0.5f, 0.5f, 0.5f, 1.0f};
    std::array<float, 4> output{};
    LumaKeyParams params;
    params.low = 0.8f;
    params.high = 0.2f;
    params.softness = 0.4f;
    params.viewMode = 99;

    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{source.data(), output.data(), 1, 1}, params));

    EXPECT_NEAR(output[0], 0.75f, 1.0e-6f);
    EXPECT_NEAR(output[1], 0.75f, 1.0e-6f);
    EXPECT_NEAR(output[2], 0.75f, 1.0e-6f);
    EXPECT_FLOAT_EQ(output[3], 1.0f);
}

TEST(KeyingEffectContractTest, LumaKeyHandlesEquivalentStraightAndPremultipliedSamples)
{
    const std::array<float, 4> straightSource{0.8f, 0.7f, 0.2f, 0.5f};
    const std::array<float, 4> premultipliedSource{0.4f, 0.35f, 0.1f, 0.5f};
    std::array<float, 4> straightOutput{};
    std::array<float, 4> premultipliedOutput{};
    LumaKeyParams params;
    params.low = 0.2f;
    params.high = 0.8f;
    params.softness = 0.2f;

    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{straightSource.data(), straightOutput.data(), 1, 1,
                       SurfaceChannelOrder::RGBA, SurfaceAlphaMode::Straight},
        params));
    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{premultipliedSource.data(), premultipliedOutput.data(), 1, 1,
                       SurfaceChannelOrder::RGBA, SurfaceAlphaMode::Premultiplied},
        params));

    EXPECT_GT(straightOutput[3], 0.0f);
    EXPECT_LT(straightOutput[3], straightSource[3]);
    EXPECT_NEAR(straightOutput[3], premultipliedOutput[3], 1.0e-6f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(premultipliedOutput[channel],
                    straightOutput[channel] * straightOutput[3], 1.0e-6f);
    }
}

TEST(KeyingEffectContractTest, LumaKeyEffectPropagatesPremultipliedAlphaMode)
{
    const std::array<float, 4> straightPixels{0.2f, 0.2f, 0.2f, 0.5f};
    const std::array<float, 4> premultipliedPixels{0.1f, 0.1f, 0.1f, 0.5f};
    ImageF32x4_RGBA straightImage;
    ImageF32x4_RGBA premultipliedImage;
    straightImage.setFromRGBA32F(straightPixels.data(), 1, 1);
    premultipliedImage.setFromRGBA32F(premultipliedPixels.data(), 1, 1);
    auto straightDescriptor =
        ArtifactCore::SurfaceColorDescriptor::canonicalLinearPremultiplied();
    straightDescriptor.alphaMode = SurfaceAlphaMode::Straight;
    straightImage.setColorDescriptor(straightDescriptor);
    premultipliedImage.setColorDescriptor(
        ArtifactCore::SurfaceColorDescriptor::canonicalLinearPremultiplied());

    LumaKeyEffect effect;
    ImageF32x4RGBAWithCache straightOutput;
    ImageF32x4RGBAWithCache premultipliedOutput;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(straightImage), straightOutput);
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(premultipliedImage), premultipliedOutput);

    const auto straightPixel = straightOutput.image().getPixel(0, 0);
    const auto premultipliedPixel = premultipliedOutput.image().getPixel(0, 0);
    EXPECT_NEAR(straightPixel.a(), premultipliedPixel.a(), 1.0e-6f);
    EXPECT_NEAR(premultipliedPixel.r(), straightPixel.r() * straightPixel.a(), 1.0e-6f);
    EXPECT_NEAR(premultipliedPixel.g(), straightPixel.g() * straightPixel.a(), 1.0e-6f);
    EXPECT_NEAR(premultipliedPixel.b(), straightPixel.b() * straightPixel.a(), 1.0e-6f);
    EXPECT_EQ(premultipliedOutput.image().colorDescriptor().alphaMode,
              SurfaceAlphaMode::Premultiplied);
}

TEST(KeyingEffectContractTest, DifferenceKeyRemovesTheReferenceColor)
{
    const std::array<float, 12> source{
        0.0f, 1.0f, 0.0f, 1.0f,
        0.9f, 0.08f, 0.04f, 1.0f,
        0.0f, 0.5f, 0.0f, 1.0f};
    std::array<float, 12> output{};
    DifferenceKeyParams params;
    params.refR = 0.0f;
    params.refG = 1.0f;
    params.refB = 0.0f;
    params.threshold = 0.08f;
    params.softness = 0.04f;

    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{source.data(), output.data(), 3, 1}, params));

    EXPECT_LT(alphaAt(output, 0), 0.01f);
    EXPECT_GT(alphaAt(output, 1), 0.99f);
    EXPECT_GT(alphaAt(output, 2), 0.99f);
}

TEST(KeyingEffectContractTest, DifferenceKeySanitizesInvalidParametersLikeItsFiniteFallbacks)
{
    const std::array<float, 4> source{0.2f, 0.4f, 0.6f, 1.0f};
    std::array<float, 4> sanitizedOutput{};
    std::array<float, 4> explicitFallbackOutput{};
    DifferenceKeyParams invalid;
    invalid.refR = std::numeric_limits<float>::infinity();
    invalid.refG = -1.0f;
    invalid.refB = 2.0f;
    invalid.threshold = std::numeric_limits<float>::quiet_NaN();
    invalid.softness = std::numeric_limits<float>::infinity();
    invalid.choke = std::numeric_limits<float>::quiet_NaN();
    invalid.matteBlur = std::numeric_limits<float>::infinity();
    invalid.viewMode = 99;
    DifferenceKeyParams fallback;
    fallback.refR = 0.0f;
    fallback.refG = 0.0f;
    fallback.refB = 1.0f;
    fallback.threshold = 0.1f;
    fallback.softness = 0.08f;
    fallback.viewMode = 1;

    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{source.data(), sanitizedOutput.data(), 1, 1}, invalid));
    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{source.data(), explicitFallbackOutput.data(), 1, 1}, fallback));

    for (std::size_t channel = 0; channel < sanitizedOutput.size(); ++channel) {
        EXPECT_NEAR(sanitizedOutput[channel], explicitFallbackOutput[channel], 1.0e-6f);
        EXPECT_TRUE(std::isfinite(sanitizedOutput[channel]));
    }
}

TEST(KeyingEffectContractTest, DifferenceKeyHandlesEquivalentStraightAndPremultipliedSamples)
{
    const std::array<float, 4> straightSource{0.8f, 0.2f, 0.1f, 0.5f};
    const std::array<float, 4> premultipliedSource{0.4f, 0.1f, 0.05f, 0.5f};
    std::array<float, 4> straightOutput{};
    std::array<float, 4> premultipliedOutput{};
    DifferenceKeyParams params;
    params.refR = 0.2f;
    params.refG = 0.2f;
    params.refB = 0.1f;
    params.threshold = 0.1f;
    params.softness = 0.8f;

    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{straightSource.data(), straightOutput.data(), 1, 1,
                             SurfaceChannelOrder::RGBA, SurfaceAlphaMode::Straight},
        params));
    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{premultipliedSource.data(), premultipliedOutput.data(), 1, 1,
                             SurfaceChannelOrder::RGBA, SurfaceAlphaMode::Premultiplied},
        params));

    EXPECT_GT(straightOutput[3], 0.0f);
    EXPECT_LT(straightOutput[3], straightSource[3]);
    EXPECT_NEAR(straightOutput[3], premultipliedOutput[3], 1.0e-6f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(premultipliedOutput[channel],
                    straightOutput[channel] * straightOutput[3], 1.0e-6f);
    }
}

TEST(KeyingEffectContractTest, DifferenceKeyEffectPropagatesPremultipliedAlphaMode)
{
    const std::array<float, 4> straightPixels{0.15f, 0.0f, 0.0f, 0.5f};
    const std::array<float, 4> premultipliedPixels{0.075f, 0.0f, 0.0f, 0.5f};
    ImageF32x4_RGBA straightImage;
    ImageF32x4_RGBA premultipliedImage;
    straightImage.setFromRGBA32F(straightPixels.data(), 1, 1);
    premultipliedImage.setFromRGBA32F(premultipliedPixels.data(), 1, 1);
    auto straightDescriptor =
        ArtifactCore::SurfaceColorDescriptor::canonicalLinearPremultiplied();
    straightDescriptor.alphaMode = SurfaceAlphaMode::Straight;
    straightImage.setColorDescriptor(straightDescriptor);
    premultipliedImage.setColorDescriptor(
        ArtifactCore::SurfaceColorDescriptor::canonicalLinearPremultiplied());

    DifferenceKeyEffect effect;
    ImageF32x4RGBAWithCache straightOutput;
    ImageF32x4RGBAWithCache premultipliedOutput;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(straightImage), straightOutput);
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(premultipliedImage), premultipliedOutput);

    const auto straightPixel = straightOutput.image().getPixel(0, 0);
    const auto premultipliedPixel = premultipliedOutput.image().getPixel(0, 0);
    EXPECT_NEAR(straightPixel.a(), premultipliedPixel.a(), 1.0e-6f);
    EXPECT_NEAR(premultipliedPixel.r(), straightPixel.r() * straightPixel.a(), 1.0e-6f);
    EXPECT_NEAR(premultipliedPixel.g(), straightPixel.g() * straightPixel.a(), 1.0e-6f);
    EXPECT_NEAR(premultipliedPixel.b(), straightPixel.b() * straightPixel.a(), 1.0e-6f);
    EXPECT_EQ(premultipliedOutput.image().colorDescriptor().alphaMode,
              SurfaceAlphaMode::Premultiplied);
}

TEST(KeyingEffectContractTest, RejectsAliasedBuffersAndUnknownChannelOrder)
{
    std::array<float, 4> pixels{0.0f, 1.0f, 0.0f, 1.0f};
    std::array<float, 4> output{};
    constexpr auto unknownOrder = SurfaceChannelOrder::Unknown;

    EXPECT_FALSE(processChromaKey(
        ChromaKeyBuffers{pixels.data(), pixels.data(), 1, 1}));
    EXPECT_FALSE(processLumaKey(
        LumaKeyBuffers{pixels.data(), pixels.data(), 1, 1}));
    EXPECT_FALSE(processDifferenceKey(
        DifferenceKeyBuffers{pixels.data(), pixels.data(), 1, 1}));

    EXPECT_FALSE(processChromaKey(
        ChromaKeyBuffers{pixels.data(), output.data(), 1, 1, unknownOrder}));
    EXPECT_FALSE(processLumaKey(
        LumaKeyBuffers{pixels.data(), output.data(), 1, 1, unknownOrder}));
    EXPECT_FALSE(processDifferenceKey(
        DifferenceKeyBuffers{pixels.data(), output.data(), 1, 1, unknownOrder}));

    EXPECT_FALSE(processIBK(
        IBKBuffers{pixels.data(), pixels.data(), output.data(), 1, 1,
                   unknownOrder, SurfaceChannelOrder::RGBA}));
    EXPECT_FALSE(processIBK(
        IBKBuffers{pixels.data(), pixels.data(), output.data(), 1, 1,
                   SurfaceChannelOrder::RGBA, unknownOrder}));
    EXPECT_FLOAT_EQ(output[0], 0.0f);
    EXPECT_FLOAT_EQ(output[1], 0.0f);
    EXPECT_FLOAT_EQ(output[2], 0.0f);
    EXPECT_FLOAT_EQ(output[3], 0.0f);
}

TEST(KeyingEffectContractTest, RejectsMissingBuffersAndInvalidDimensionsWithoutWriting)
{
    const std::array<float, 4> pixels{0.2f, 0.4f, 0.6f, 1.0f};
    std::array<float, 4> output{0.11f, 0.22f, 0.33f, 0.44f};
    const auto expectUnchanged = [&] {
        EXPECT_FLOAT_EQ(output[0], 0.11f);
        EXPECT_FLOAT_EQ(output[1], 0.22f);
        EXPECT_FLOAT_EQ(output[2], 0.33f);
        EXPECT_FLOAT_EQ(output[3], 0.44f);
    };

    EXPECT_FALSE(processChromaKey(
        ChromaKeyBuffers{nullptr, output.data(), 1, 1}));
    expectUnchanged();
    EXPECT_FALSE(processChromaKey(
        ChromaKeyBuffers{pixels.data(), nullptr, 1, 1}));
    expectUnchanged();
    EXPECT_FALSE(processChromaKey(
        ChromaKeyBuffers{pixels.data(), output.data(), 0, 1}));
    expectUnchanged();
    EXPECT_FALSE(processChromaKey(
        ChromaKeyBuffers{pixels.data(), output.data(), 1, -1}));
    expectUnchanged();

    EXPECT_FALSE(processLumaKey(
        LumaKeyBuffers{nullptr, output.data(), 1, 1}));
    expectUnchanged();
    EXPECT_FALSE(processLumaKey(
        LumaKeyBuffers{pixels.data(), nullptr, 1, 1}));
    expectUnchanged();
    EXPECT_FALSE(processLumaKey(
        LumaKeyBuffers{pixels.data(), output.data(), 0, 1}));
    expectUnchanged();
    EXPECT_FALSE(processLumaKey(
        LumaKeyBuffers{pixels.data(), output.data(), 1, -1}));
    expectUnchanged();

    EXPECT_FALSE(processDifferenceKey(
        DifferenceKeyBuffers{nullptr, output.data(), 1, 1}));
    expectUnchanged();
    EXPECT_FALSE(processDifferenceKey(
        DifferenceKeyBuffers{pixels.data(), nullptr, 1, 1}));
    expectUnchanged();
    EXPECT_FALSE(processDifferenceKey(
        DifferenceKeyBuffers{pixels.data(), output.data(), 0, 1}));
    expectUnchanged();
    EXPECT_FALSE(processDifferenceKey(
        DifferenceKeyBuffers{pixels.data(), output.data(), 1, -1}));
    expectUnchanged();

    EXPECT_FALSE(processIBK(
        IBKBuffers{nullptr, pixels.data(), output.data(), 1, 1}));
    expectUnchanged();
    EXPECT_FALSE(processIBK(
        IBKBuffers{pixels.data(), nullptr, output.data(), 1, 1}));
    expectUnchanged();
    EXPECT_FALSE(processIBK(
        IBKBuffers{pixels.data(), pixels.data(), nullptr, 1, 1}));
    expectUnchanged();
    EXPECT_FALSE(processIBK(
        IBKBuffers{pixels.data(), pixels.data(), output.data(), 0, 1}));
    expectUnchanged();
    EXPECT_FALSE(processIBK(
        IBKBuffers{pixels.data(), pixels.data(), output.data(), 1, -1}));
    expectUnchanged();
}

TEST(KeyingEffectContractTest,
     ChromaLumaAndDifferenceKeyExposeFinalMatteAsOpaqueGrayscale)
{
    const std::array<float, 4> chromaSource{0.25f, 0.65f, 0.3f, 1.0f};
    const std::array<float, 4> lumaSource{0.5f, 0.5f, 0.5f, 1.0f};
    const std::array<float, 4> differenceSource{0.5f, 0.25f, 0.1f, 1.0f};
    std::array<float, 4> chromaFinal{};
    std::array<float, 4> chromaMatte{};
    std::array<float, 4> chromaDespill{};
    std::array<float, 4> chromaStatus{};
    std::array<float, 4> lumaFinal{};
    std::array<float, 4> lumaMatte{};
    std::array<float, 4> differenceFinal{};
    std::array<float, 4> differenceMatte{};

    ChromaKeyParams chromaParams;
    chromaParams.despillStrength = 0.0f;
    ASSERT_TRUE(processChromaKey(
        ChromaKeyBuffers{chromaSource.data(), chromaFinal.data(), 1, 1},
        chromaParams));
    chromaParams.viewMode = 1;
    ASSERT_TRUE(processChromaKey(
        ChromaKeyBuffers{chromaSource.data(), chromaMatte.data(), 1, 1},
        chromaParams));
    chromaParams.viewMode = 2;
    ASSERT_TRUE(processChromaKey(
        ChromaKeyBuffers{chromaSource.data(), chromaDespill.data(), 1, 1},
        chromaParams));
    chromaParams.viewMode = 3;
    ASSERT_TRUE(processChromaKey(
        ChromaKeyBuffers{chromaSource.data(), chromaStatus.data(), 1, 1},
        chromaParams));

    LumaKeyParams lumaParams;
    lumaParams.low = 0.2f;
    lumaParams.high = 0.8f;
    lumaParams.softness = 0.4f;
    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{lumaSource.data(), lumaFinal.data(), 1, 1}, lumaParams));
    lumaParams.viewMode = 1;
    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{lumaSource.data(), lumaMatte.data(), 1, 1}, lumaParams));

    DifferenceKeyParams differenceParams;
    differenceParams.refR = 0.5f;
    differenceParams.refG = 0.25f;
    differenceParams.refB = 0.2f;
    differenceParams.threshold = 0.05f;
    differenceParams.softness = 0.2f;
    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{differenceSource.data(), differenceFinal.data(), 1, 1},
        differenceParams));
    differenceParams.viewMode = 1;
    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{differenceSource.data(), differenceMatte.data(), 1, 1},
        differenceParams));

    const auto expectMattePreview = [](const std::array<float, 4>& finalPixel,
                                       const std::array<float, 4>& mattePixel) {
        EXPECT_GT(finalPixel[3], 0.0f);
        EXPECT_LT(finalPixel[3], 1.0f);
        EXPECT_NEAR(mattePixel[0], finalPixel[3], 1.0e-6f);
        EXPECT_NEAR(mattePixel[1], finalPixel[3], 1.0e-6f);
        EXPECT_NEAR(mattePixel[2], finalPixel[3], 1.0e-6f);
        EXPECT_FLOAT_EQ(mattePixel[3], 1.0f);
    };
    expectMattePreview(chromaFinal, chromaMatte);
    expectMattePreview(lumaFinal, lumaMatte);
    expectMattePreview(differenceFinal, differenceMatte);

    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(chromaDespill[channel], chromaFinal[channel], 1.0e-6f);
    }
    EXPECT_FLOAT_EQ(chromaDespill[3], 1.0f);
    EXPECT_NEAR(chromaStatus[0], 1.0f - chromaFinal[3], 1.0e-6f);
    EXPECT_NEAR(chromaStatus[1], chromaFinal[3], 1.0e-6f);
    EXPECT_NEAR(chromaStatus[2],
                0.15f * (1.0f - std::abs(chromaFinal[3] * 2.0f - 1.0f)),
                1.0e-6f);
    EXPECT_FLOAT_EQ(chromaStatus[3], 1.0f);
}

TEST(KeyingEffectContractTest, ArtifactWrappersRouteMatteViewPropertyToCpuProcessing)
{
    const std::array<float, 4> chromaPixel{0.25f, 0.65f, 0.3f, 1.0f};
    const std::array<float, 4> lumaPixel{0.2f, 0.2f, 0.2f, 1.0f};
    const std::array<float, 4> differencePixel{0.15f, 0.0f, 0.0f, 1.0f};
    ImageF32x4_RGBA chromaImage;
    ImageF32x4_RGBA lumaImage;
    ImageF32x4_RGBA differenceImage;
    chromaImage.setFromRGBA32F(chromaPixel.data(), 1, 1);
    lumaImage.setFromRGBA32F(lumaPixel.data(), 1, 1);
    differenceImage.setFromRGBA32F(differencePixel.data(), 1, 1);

    ChromaKeyEffect chromaEffect;
    ImageF32x4RGBAWithCache chromaFinal;
    ImageF32x4RGBAWithCache chromaMatte;
    chromaEffect.applyCPUOnly(ImageF32x4RGBAWithCache(chromaImage), chromaFinal);
    chromaEffect.setPropertyValue(ArtifactCore::UniString("viewMode"), QVariant(1));
    chromaEffect.applyCPUOnly(ImageF32x4RGBAWithCache(chromaImage), chromaMatte);

    LumaKeyEffect lumaEffect;
    ImageF32x4RGBAWithCache lumaFinal;
    ImageF32x4RGBAWithCache lumaMatte;
    lumaEffect.applyCPUOnly(ImageF32x4RGBAWithCache(lumaImage), lumaFinal);
    lumaEffect.setPropertyValue(ArtifactCore::UniString("viewMode"), QVariant(1));
    lumaEffect.applyCPUOnly(ImageF32x4RGBAWithCache(lumaImage), lumaMatte);

    DifferenceKeyEffect differenceEffect;
    ImageF32x4RGBAWithCache differenceFinal;
    ImageF32x4RGBAWithCache differenceMatte;
    differenceEffect.applyCPUOnly(
        ImageF32x4RGBAWithCache(differenceImage), differenceFinal);
    differenceEffect.setPropertyValue(
        ArtifactCore::UniString("viewMode"), QVariant(1));
    differenceEffect.applyCPUOnly(
        ImageF32x4RGBAWithCache(differenceImage), differenceMatte);

    const auto expectPreviewMatchesFinalAlpha = [](const auto& finalSurface,
                                                   const auto& matteSurface) {
        const auto finalPixel = finalSurface.image().getPixel(0, 0);
        const auto mattePixel = matteSurface.image().getPixel(0, 0);
        EXPECT_GT(finalPixel.a(), 0.0f);
        EXPECT_LT(finalPixel.a(), 1.0f);
        EXPECT_NEAR(mattePixel.r(), finalPixel.a(), 1.0e-6f);
        EXPECT_NEAR(mattePixel.g(), finalPixel.a(), 1.0e-6f);
        EXPECT_NEAR(mattePixel.b(), finalPixel.a(), 1.0e-6f);
        EXPECT_FLOAT_EQ(mattePixel.a(), 1.0f);
    };
    expectPreviewMatchesFinalAlpha(chromaFinal, chromaMatte);
    expectPreviewMatchesFinalAlpha(lumaFinal, lumaMatte);
    expectPreviewMatchesFinalAlpha(differenceFinal, differenceMatte);
}

TEST(KeyingEffectContractTest, SanitizesNonFiniteAndZeroAlphaPremultipliedSamples)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const std::array<float, 12> source{
        0.7f, 0.1f, 0.2f, 0.0f,
        nan, 0.2f, 0.3f, 0.5f,
        0.2f, 0.1f, 0.1f, nan};
    std::array<float, 12> chromaOutput{};
    std::array<float, 12> lumaOutput{};
    std::array<float, 12> differenceOutput{};

    ASSERT_TRUE(processChromaKey(
        ChromaKeyBuffers{source.data(), chromaOutput.data(), 3, 1,
                         SurfaceChannelOrder::RGBA, SurfaceAlphaMode::Premultiplied},
        {}));
    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{source.data(), lumaOutput.data(), 3, 1,
                       SurfaceChannelOrder::RGBA, SurfaceAlphaMode::Premultiplied},
        {}));
    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{source.data(), differenceOutput.data(), 3, 1,
                             SurfaceChannelOrder::RGBA, SurfaceAlphaMode::Premultiplied},
        {}));

    const std::array<const std::array<float, 12>*, 3> outputs{
        &chromaOutput, &lumaOutput, &differenceOutput};
    for (const auto* output : outputs) {
        EXPECT_FLOAT_EQ((*output)[3], 0.0f);
        EXPECT_FLOAT_EQ((*output)[7], 0.0f);
        EXPECT_FLOAT_EQ((*output)[11], 0.0f);
        for (float channel : *output) {
            EXPECT_TRUE(std::isfinite(channel));
        }
    }
}

TEST(KeyingEffectContractTest, IBKGpuMorphologySupportMatchesShaderSamplingWindow)
{
    IBKParams params;
    EXPECT_TRUE(IBKGpuParams::hasGpuSupportedMorphologyRadius(params));

    params.erodePixels = -1;
    params.dilatePixels = -2;
    EXPECT_TRUE(IBKGpuParams::hasGpuSupportedMorphologyRadius(params));
    auto gpuParams = IBKGpuParams::fromParams(params);
    EXPECT_EQ(gpuParams.erodePixels, 0u);
    EXPECT_EQ(gpuParams.dilatePixels, 0u);

    params.erodePixels = 64;
    params.dilatePixels = 64;
    EXPECT_FALSE(IBKGpuParams::hasGpuSupportedMorphologyRadius(params));
    gpuParams = IBKGpuParams::fromParams(params);
    EXPECT_EQ(gpuParams.erodePixels, 64u);
    EXPECT_EQ(gpuParams.dilatePixels, 64u);

    params.erodePixels = IBKGpuParams::MaxGpuMorphologyRadius;
    params.dilatePixels = IBKGpuParams::MaxGpuMorphologyRadius;
    EXPECT_TRUE(IBKGpuParams::hasGpuSupportedMorphologyRadius(params));

    params.erodePixels++;
    EXPECT_FALSE(IBKGpuParams::hasGpuSupportedMorphologyRadius(params));
    params.erodePixels = 1;
    params.dilatePixels++;
    EXPECT_FALSE(IBKGpuParams::hasGpuSupportedMorphologyRadius(params));
}

TEST(KeyingEffectContractTest, IBKGpuParameterConversionSanitizesInvalidValues)
{
    IBKParams params;
    params.screenCorrection = std::numeric_limits<float>::quiet_NaN();
    params.coreMatteClip = std::numeric_limits<float>::infinity();
    params.edgeMatteSoftness = -std::numeric_limits<float>::infinity();
    params.despillStrength = std::numeric_limits<float>::quiet_NaN();
    params.garbageMatteGamma = 9.0f;
    params.detailRecovery = -2.0f;
    params.erodePixels = -8;
    params.dilatePixels = 100;

    const auto gpuParams = IBKGpuParams::fromParams(params);

    EXPECT_FLOAT_EQ(gpuParams.screenCorrection, 1.0f);
    EXPECT_FLOAT_EQ(gpuParams.coreMatteClip, 0.5f);
    EXPECT_FLOAT_EQ(gpuParams.edgeMatteSoftness, 0.2f);
    EXPECT_FLOAT_EQ(gpuParams.despillStrength, 0.5f);
    EXPECT_FLOAT_EQ(gpuParams.garbageMatteGamma, 4.0f);
    EXPECT_FLOAT_EQ(gpuParams.detailRecovery, 0.0f);
    EXPECT_EQ(gpuParams.erodePixels, 0u);
    EXPECT_EQ(gpuParams.dilatePixels, 64u);

    params.screenCorrection = 9.0f;
    const auto upperClampedGpuParams = IBKGpuParams::fromParams(params);
    EXPECT_FLOAT_EQ(upperClampedGpuParams.screenCorrection, 4.0f);
}

TEST(KeyingEffectContractTest, IBKPublicPropertyRangesMatchSetterClamps)
{
    IBKKeyerEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("screenCorrection"), QVariant(9.0));
    effect.setPropertyValue(ArtifactCore::UniString("coreMatteClip"), QVariant(-0.5));
    effect.setPropertyValue(ArtifactCore::UniString("edgeMatteSoftness"), QVariant(0.0));
    effect.setPropertyValue(ArtifactCore::UniString("despillStrength"), QVariant(1.5));
    effect.setPropertyValue(ArtifactCore::UniString("garbageMatteGamma"), QVariant(9.0));
    effect.setPropertyValue(ArtifactCore::UniString("detailRecovery"), QVariant(-1.0));
    effect.setPropertyValue(ArtifactCore::UniString("erodePixels"), QVariant(99));
    effect.setPropertyValue(ArtifactCore::UniString("dilatePixels"), QVariant(-3));
    effect.setPropertyValue(ArtifactCore::UniString("previewMode"), QVariant(99));

    struct ExpectedProperty {
        QString name;
        double value;
        double minimum;
        double maximum;
    };
    const std::array expected{
        ExpectedProperty{QStringLiteral("screenCorrection"), 4.0, 0.0, 4.0},
        ExpectedProperty{QStringLiteral("coreMatteClip"), 0.0, 0.0, 1.0},
        ExpectedProperty{QStringLiteral("edgeMatteSoftness"), 1.0e-5, 1.0e-5, 1.0},
        ExpectedProperty{QStringLiteral("despillStrength"), 1.0, 0.0, 1.0},
        ExpectedProperty{QStringLiteral("garbageMatteGamma"), 4.0, 1.0e-5, 4.0},
        ExpectedProperty{QStringLiteral("detailRecovery"), 0.0, 0.0, 1.0},
        ExpectedProperty{QStringLiteral("erodePixels"), 64.0, 0.0, 64.0},
        ExpectedProperty{QStringLiteral("dilatePixels"), 0.0, 0.0, 64.0},
        ExpectedProperty{QStringLiteral("previewMode"), 2.0, 0.0, 2.0},
    };
    const auto properties = effect.getProperties();
    for (const auto& expectedProperty : expected) {
        const ArtifactCore::AbstractProperty* actual = nullptr;
        for (const auto& property : properties) {
            if (property.getName() == expectedProperty.name) {
                actual = &property;
                break;
            }
        }
        ASSERT_NE(actual, nullptr) << expectedProperty.name.toStdString();
        EXPECT_NEAR(actual->getValue().toDouble(), expectedProperty.value, 1.0e-6)
            << expectedProperty.name.toStdString();
        const auto metadata = actual->metadata();
        EXPECT_DOUBLE_EQ(metadata.hardMin.toDouble(), expectedProperty.minimum)
            << expectedProperty.name.toStdString();
        EXPECT_DOUBLE_EQ(metadata.hardMax.toDouble(), expectedProperty.maximum)
            << expectedProperty.name.toStdString();
    }
}

TEST(KeyingEffectContractTest, IBKEffectUsesSpatiallyVaryingCleanPlate)
{
    const std::array<float, 12> foreground{
        0.0f, 0.72f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.90f, 0.0f, 1.0f};
    const std::array<float, 12> cleanPlate{
        0.0f, 0.72f, 0.0f, 1.0f,
        0.0f, 0.85f, 0.0f, 1.0f,
        0.0f, 0.90f, 0.0f, 1.0f};

    ImageF32x4_RGBA sourceImage;
    ImageF32x4_RGBA plateImage;
    sourceImage.setFromRGBA32F(foreground.data(), 3, 1);
    plateImage.setFromRGBA32F(cleanPlate.data(), 3, 1);

    IBKKeyerEffect effect;
    effect.setCleanPlate(plateImage);
    effect.setPropertyValue(ArtifactCore::UniString("erodePixels"), QVariant(0));
    effect.setPropertyValue(ArtifactCore::UniString("dilatePixels"), QVariant(0));
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), output);

    ASSERT_EQ(output.image().width(), 3);
    EXPECT_LT(output.image().getPixel(0, 0).a(), 0.01f);
    EXPECT_GT(output.image().getPixel(1, 0).a(), 0.7f);
    EXPECT_LT(output.image().getPixel(2, 0).a(), 0.01f);
}

TEST(KeyingEffectContractTest, IBKMatchesAllForegroundAndCleanPlateChannelOrders)
{
    const std::array<float, 8> foregroundRgba{
        0.0f, 0.8f, 0.0f, 1.0f,
        0.8f, 0.2f, 0.1f, 1.0f};
    const std::array<float, 8> cleanPlateRgba{
        0.0f, 0.8f, 0.0f, 1.0f,
        0.0f, 0.7f, 0.0f, 1.0f};
    const auto toBgra = [](const std::array<float, 8>& rgba) {
        std::array<float, 8> bgra{};
        for (std::size_t pixel = 0; pixel < 2; ++pixel) {
            bgra[pixel * 4] = rgba[pixel * 4 + 2];
            bgra[pixel * 4 + 1] = rgba[pixel * 4 + 1];
            bgra[pixel * 4 + 2] = rgba[pixel * 4];
            bgra[pixel * 4 + 3] = rgba[pixel * 4 + 3];
        }
        return bgra;
    };
    const auto foregroundBgra = toBgra(foregroundRgba);
    const auto cleanPlateBgra = toBgra(cleanPlateRgba);
    const std::array<SurfaceChannelOrder, 2> orders{
        SurfaceChannelOrder::RGBA, SurfaceChannelOrder::BGRA};
    std::array<float, 8> referenceOutput{};
    IBKParams params;
    params.erodePixels = 0;
    params.dilatePixels = 0;
    params.despillStrength = 0.0f;

    for (const auto foregroundOrder : orders) {
        for (const auto cleanPlateOrder : orders) {
            const float* foreground = foregroundOrder == SurfaceChannelOrder::RGBA
                ? foregroundRgba.data() : foregroundBgra.data();
            const float* cleanPlate = cleanPlateOrder == SurfaceChannelOrder::RGBA
                ? cleanPlateRgba.data() : cleanPlateBgra.data();
            std::array<float, 8> output{};
            ASSERT_TRUE(processIBK(
                IBKBuffers{foreground, cleanPlate, output.data(), 2, 1,
                           foregroundOrder, cleanPlateOrder}, params));

            const int rIndex = foregroundOrder == SurfaceChannelOrder::RGBA ? 0 : 2;
            const int gIndex = 1;
            const int bIndex = foregroundOrder == SurfaceChannelOrder::RGBA ? 2 : 0;
            for (std::size_t pixel = 0; pixel < 2; ++pixel) {
                const std::array<float, 4> logicalOutput{
                    output[pixel * 4 + rIndex], output[pixel * 4 + gIndex],
                    output[pixel * 4 + bIndex], output[pixel * 4 + 3]};
                if (foregroundOrder == SurfaceChannelOrder::RGBA &&
                    cleanPlateOrder == SurfaceChannelOrder::RGBA) {
                    std::copy(logicalOutput.begin(), logicalOutput.end(),
                              referenceOutput.begin() + pixel * 4);
                } else {
                    for (std::size_t channel = 0; channel < 4; ++channel) {
                        EXPECT_NEAR(logicalOutput[channel],
                                    referenceOutput[pixel * 4 + channel],
                                    1.0e-6f);
                    }
                }
            }
        }
    }
    EXPECT_LT(referenceOutput[3], 0.01f);
    EXPECT_GT(referenceOutput[7], 0.7f);
}

TEST(KeyingEffectContractTest, IBKClampsForegroundAlphaBeforeWritingOutput)
{
    const std::array<float, 4> cleanPlate{0.0f, 1.0f, 0.0f, 1.0f};
    IBKParams params;
    params.coreMatteClip = 0.0f;
    params.edgeMatteSoftness = 1.0f;
    params.despillStrength = 0.0f;
    params.detailRecovery = 1.0f;
    params.erodePixels = 0;
    params.dilatePixels = 0;

    std::array<float, 4> foreground{1.0f, 0.0f, 0.0f, 2.0f};
    std::array<float, 4> output{};
    ASSERT_TRUE(processIBK(
        IBKBuffers{foreground.data(), cleanPlate.data(), output.data(), 1, 1},
        params));
    EXPECT_GT(output[3], 0.9f);
    EXPECT_LE(output[3], 1.0f);

    foreground[3] = -1.0f;
    ASSERT_TRUE(processIBK(
        IBKBuffers{foreground.data(), cleanPlate.data(), output.data(), 1, 1},
        params));
    EXPECT_FLOAT_EQ(output[3], 0.0f);
    EXPECT_FLOAT_EQ(output[0], 0.0f);
    EXPECT_FLOAT_EQ(output[1], 0.0f);
    EXPECT_FLOAT_EQ(output[2], 0.0f);
}

TEST(KeyingEffectContractTest, IBKCleanPlateGenerationUsesTemporalMedian)
{
    const std::array<float, 4> first{0.0f, 1.0f, 0.0f, 1.0f};
    const std::array<float, 4> subject{1.0f, 0.0f, 0.2f, 1.0f};
    const std::array<float, 4> last{0.4f, 0.6f, 0.4f, 1.0f};
    std::vector<ImageF32x4_RGBA> frames(3);
    frames[0].setFromRGBA32F(first.data(), 1, 1);
    frames[1].setFromRGBA32F(subject.data(), 1, 1);
    frames[2].setFromRGBA32F(last.data(), 1, 1);

    ImageF32x4_RGBA cleanPlate;
    ASSERT_TRUE(autoGenerateCleanPlate(frames, cleanPlate));

    const auto pixel = cleanPlate.getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), 0.4f);
    EXPECT_FLOAT_EQ(pixel.g(), 0.6f);
    EXPECT_FLOAT_EQ(pixel.b(), 0.2f);
}

TEST(KeyingEffectContractTest, IBKCleanPlateGenerationAveragesTheMiddlePairForEvenFrameCounts)
{
    const std::array<float, 4> first{0.0f, 1.0f, 0.0f, 0.2f};
    const std::array<float, 4> second{0.4f, 0.6f, 0.2f, 0.8f};
    std::vector<ImageF32x4_RGBA> frames(2);
    frames[0].setFromRGBA32F(first.data(), 1, 1);
    frames[1].setFromRGBA32F(second.data(), 1, 1);

    ImageF32x4_RGBA cleanPlate;
    ASSERT_TRUE(autoGenerateCleanPlate(frames, cleanPlate));

    const auto pixel = cleanPlate.getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), 0.2f);
    EXPECT_FLOAT_EQ(pixel.g(), 0.8f);
    EXPECT_FLOAT_EQ(pixel.b(), 0.1f);
    EXPECT_FLOAT_EQ(pixel.a(), 0.5f);
}

TEST(KeyingEffectContractTest, IBKCleanPlateGenerationNormalizesRgbaAndBgraIngress)
{
    const std::array<float, 4> rgbaFrame{0.1f, 0.2f, 0.3f, 1.0f};
    const std::array<float, 4> bgraFrame{0.5f, 0.6f, 0.7f, 1.0f};
    std::vector<ImageF32x4_RGBA> frames(2);
    frames[0].setFromRGBA32F(rgbaFrame.data(), 1, 1);
    frames[1].setFromBGRA32F(bgraFrame.data(), 1, 1);

    ImageF32x4_RGBA cleanPlate;
    ASSERT_TRUE(autoGenerateCleanPlate(frames, cleanPlate));

    const auto pixel = cleanPlate.getPixel(0, 0);
    EXPECT_NEAR(pixel.r(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 0.4f, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
    EXPECT_EQ(cleanPlate.colorDescriptor().channelOrder, SurfaceChannelOrder::BGRA);
}

TEST(KeyingEffectContractTest, IBKCleanPlateGenerationReplacesNonFiniteSamplesWithZero)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const std::array<float, 4> source{nan, 0.4f, infinity, nan};
    std::vector<ImageF32x4_RGBA> frames(1);
    frames[0].setFromRGBA32F(source.data(), 1, 1);

    ImageF32x4_RGBA cleanPlate;
    ASSERT_TRUE(autoGenerateCleanPlate(frames, cleanPlate));

    const auto pixel = cleanPlate.getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), 0.0f);
    EXPECT_FLOAT_EQ(pixel.g(), 0.4f);
    EXPECT_FLOAT_EQ(pixel.b(), 0.0f);
    EXPECT_FLOAT_EQ(pixel.a(), 0.0f);
}

TEST(KeyingEffectContractTest, IBKCleanPlateGenerationRejectsEmptyAndMismatchedFramesWithoutChangingOutput)
{
    const std::array<float, 4> sentinel{0.3f, 0.2f, 0.1f, 0.7f};
    const std::array<float, 4> first{0.0f, 1.0f, 0.0f, 1.0f};
    const std::array<float, 8> differentSize{
        0.0f, 1.0f, 0.0f, 1.0f,
        0.1f, 0.8f, 0.0f, 1.0f};
    ImageF32x4_RGBA output;
    output.setFromRGBA32F(sentinel.data(), 1, 1);
    std::vector<ImageF32x4_RGBA> frames;

    EXPECT_FALSE(autoGenerateCleanPlate(frames, output));
    auto pixel = output.getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), sentinel[0]);
    EXPECT_FLOAT_EQ(pixel.g(), sentinel[1]);
    EXPECT_FLOAT_EQ(pixel.b(), sentinel[2]);
    EXPECT_FLOAT_EQ(pixel.a(), sentinel[3]);

    frames.resize(2);
    frames[0].setFromRGBA32F(first.data(), 1, 1);
    frames[1].setFromRGBA32F(differentSize.data(), 2, 1);
    EXPECT_FALSE(autoGenerateCleanPlate(frames, output));
    pixel = output.getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), sentinel[0]);
    EXPECT_FLOAT_EQ(pixel.g(), sentinel[1]);
    EXPECT_FLOAT_EQ(pixel.b(), sentinel[2]);
    EXPECT_FLOAT_EQ(pixel.a(), sentinel[3]);
}

TEST(KeyingEffectContractTest, IBKCleanPlateGenerationEnforcesThe4096FrameLimit)
{
    const std::array<float, 4> source{0.2f, 0.3f, 0.4f, 1.0f};
    ImageF32x4_RGBA frame;
    frame.setFromRGBA32F(source.data(), 1, 1);
    std::vector<ImageF32x4_RGBA> frames(4096, frame);
    ImageF32x4_RGBA cleanPlate;

    ASSERT_TRUE(autoGenerateCleanPlate(frames, cleanPlate));
    const auto acceptedPixel = cleanPlate.getPixel(0, 0);
    EXPECT_FLOAT_EQ(acceptedPixel.r(), source[0]);
    EXPECT_FLOAT_EQ(acceptedPixel.g(), source[1]);
    EXPECT_FLOAT_EQ(acceptedPixel.b(), source[2]);
    EXPECT_FLOAT_EQ(acceptedPixel.a(), source[3]);

    frames.emplace_back(frame);
    EXPECT_FALSE(autoGenerateCleanPlate(frames, cleanPlate));
    const auto preservedPixel = cleanPlate.getPixel(0, 0);
    EXPECT_FLOAT_EQ(preservedPixel.r(), acceptedPixel.r());
    EXPECT_FLOAT_EQ(preservedPixel.g(), acceptedPixel.g());
    EXPECT_FLOAT_EQ(preservedPixel.b(), acceptedPixel.b());
    EXPECT_FLOAT_EQ(preservedPixel.a(), acceptedPixel.a());
}

TEST(KeyingEffectContractTest, EmptyCleanPlatePathClearsTheLoadedPlate)
{
    const std::array<float, 4> foreground{0.0f, 0.85f, 0.0f, 1.0f};
    ImageF32x4_RGBA sourceImage;
    ImageF32x4_RGBA plateImage;
    sourceImage.setFromRGBA32F(foreground.data(), 1, 1);
    plateImage.setFromRGBA32F(foreground.data(), 1, 1);

    IBKKeyerEffect effect;
    effect.setCleanPlate(plateImage);
    effect.setPropertyValue(ArtifactCore::UniString("cleanPlatePath"), QVariant(QString()));
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), output);

    EXPECT_FLOAT_EQ(output.image().getPixel(0, 0).a(), 1.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(0, 0).g(), 0.85f);
}

TEST(KeyingEffectContractTest, IBKLoadsCleanPlateFromTheConfiguredImagePath)
{
    const QString fixturePath =
        qEnvironmentVariable("ARTIFACT_KEYING_REAL_FIXTURE");
    ASSERT_FALSE(fixturePath.isEmpty())
        << "CMake must provide the bundled clean-plate fixture path";

    ImageF32x4_RGBA sourceImage;
    ASSERT_TRUE(sourceImage.load(fixturePath));
    ASSERT_EQ(sourceImage.width(), 540);
    ASSERT_EQ(sourceImage.height(), 960);

    IBKKeyerEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("erodePixels"), QVariant(0));
    effect.setPropertyValue(ArtifactCore::UniString("dilatePixels"), QVariant(0));
    effect.setPropertyValue(ArtifactCore::UniString("cleanPlatePath"),
                            QVariant(fixturePath));

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), output);

    ASSERT_EQ(output.image().width(), sourceImage.width());
    ASSERT_EQ(output.image().height(), sourceImage.height());
    EXPECT_LT(output.image().getPixel(270, 480).a(), 0.01f)
        << "a successfully loaded identical clean plate should remove the frame";
}

TEST(KeyingEffectContractTest, InvalidCleanPlatePathKeepsThePreviouslyLoadedPlate)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const std::array<float, 4> foreground{0.0f, 0.85f, 0.0f, 1.0f};
    ImageF32x4_RGBA sourceImage;
    ImageF32x4_RGBA plateImage;
    sourceImage.setFromRGBA32F(foreground.data(), 1, 1);
    plateImage.setFromRGBA32F(foreground.data(), 1, 1);

    IBKKeyerEffect effect;
    effect.setCleanPlate(plateImage);
    effect.setPropertyValue(
        ArtifactCore::UniString("cleanPlatePath"),
        QVariant(temporaryDirectory.filePath(QStringLiteral("missing.png"))));

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), output);

    EXPECT_LT(output.image().getPixel(0, 0).a(), 0.01f)
        << "a failed path update should preserve the previously loaded plate";
}

TEST(KeyingEffectContractTest, IBKPassesThroughWhenCleanPlateDimensionsDoNotMatch)
{
    const std::array<float, 8> foreground{
        0.15f, 0.7f, 0.2f, 0.6f,
        0.8f, 0.1f, 0.05f, 1.0f};
    const std::array<float, 4> cleanPlate{0.0f, 0.8f, 0.0f, 1.0f};
    ImageF32x4_RGBA sourceImage;
    ImageF32x4_RGBA plateImage;
    sourceImage.setFromRGBA32F(foreground.data(), 2, 1);
    plateImage.setFromRGBA32F(cleanPlate.data(), 1, 1);

    IBKKeyerEffect effect;
    effect.setCleanPlate(plateImage);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), output);

    ASSERT_EQ(output.image().width(), 2);
    ASSERT_EQ(output.image().height(), 1);
    for (int x = 0; x < 2; ++x) {
        const auto actual = output.image().getPixel(x, 0);
        const std::size_t offset = static_cast<std::size_t>(x) * 4;
        EXPECT_FLOAT_EQ(actual.r(), foreground[offset]);
        EXPECT_FLOAT_EQ(actual.g(), foreground[offset + 1]);
        EXPECT_FLOAT_EQ(actual.b(), foreground[offset + 2]);
        EXPECT_FLOAT_EQ(actual.a(), foreground[offset + 3]);
    }
}

TEST(KeyingEffectContractTest, IBKMattePreviewDisplaysTheFinalAlphaAsOpaqueGrayscale)
{
    const std::array<float, 4> foreground{1.0f, 0.0f, 0.0f, 1.0f};
    const std::array<float, 4> cleanPlate{0.0f, 0.85f, 0.0f, 1.0f};
    ImageF32x4_RGBA sourceImage;
    ImageF32x4_RGBA plateImage;
    sourceImage.setFromRGBA32F(foreground.data(), 1, 1);
    plateImage.setFromRGBA32F(cleanPlate.data(), 1, 1);

    IBKKeyerEffect effect;
    effect.setCleanPlate(plateImage);
    effect.setPropertyValue(ArtifactCore::UniString("erodePixels"), QVariant(0));
    effect.setPropertyValue(ArtifactCore::UniString("dilatePixels"), QVariant(0));
    ImageF32x4RGBAWithCache finalOutput;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), finalOutput);
    effect.setPropertyValue(ArtifactCore::UniString("previewMode"), QVariant(1));
    ImageF32x4RGBAWithCache mattePreview;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), mattePreview);

    const float matte = finalOutput.image().getPixel(0, 0).a();
    ASSERT_GT(matte, 0.0f);
    const auto preview = mattePreview.image().getPixel(0, 0);
    EXPECT_NEAR(preview.r(), matte, 1.0e-6f);
    EXPECT_NEAR(preview.g(), matte, 1.0e-6f);
    EXPECT_NEAR(preview.b(), matte, 1.0e-6f);
    EXPECT_FLOAT_EQ(preview.a(), 1.0f);
}

\n