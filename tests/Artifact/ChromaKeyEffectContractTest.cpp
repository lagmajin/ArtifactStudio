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
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

using namespace ArtifactCore::Keying;
using ArtifactCore::SurfaceAlphaMode;
using ArtifactCore::SurfaceChannelOrder;
using Artifact::ChromaKeyEffect;
using ArtifactCore::ImageF32x4RGBAWithCache;
using ArtifactCore::ImageF32x4_RGBA;

namespace {

float alphaAt(const std::array<float, 12>& pixels, std::size_t pixelIndex)
{
    return pixels[pixelIndex * 4 + 3];
}

} // namespace

TEST(ChromaKeyEffectContractTest, ChromaKeyRemovesScreenAndKeepsForeground)
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

TEST(ChromaKeyEffectContractTest, ChromaKeyTreatsRgbaAndBgraPixelsEquivalently)
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

TEST(ChromaKeyEffectContractTest, ChromaKeyMatchesEquivalentStraightAndPremultipliedCpuInputs)
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

TEST(ChromaKeyEffectContractTest, ChromaCoreTargetNeutralDespillUsesTheConfiguredMode)
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

TEST(ChromaKeyEffectContractTest, ChromaKeyMapsPropertiesToResidentGpuNodeParameters)
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

TEST(ChromaKeyEffectContractTest, ChromaKeyProcessesRealLiveActionGreenScreenFrame)
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

TEST(ChromaKeyEffectContractTest, ChromaKeyEffectPropagatesPremultipliedAlphaMode)
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

TEST(ChromaKeyEffectContractTest, ChromaKeyRemovesConfiguredScreenColorAndKeepsForegroundAlpha)
{
    constexpr std::array<float, 8> pixels = {
        0.0f, 1.0f, 0.0f, 0.8f,
        1.0f, 0.0f, 0.0f, 0.8f,
    };
    const auto source = makeSurface(pixels.data(), 2, 1);
    ImageF32x4RGBAWithCache output;
    ChromaKeyEffect effect;
    effect.setKeyColor(FloatRGBA(0.0f, 1.0f, 0.0f, 1.0f));
    effect.setHueTolerance(0.1f);
    effect.setEdgeSoftness(0.1f);
    effect.setDespillStrength(0.0f);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(0, 0).a(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 0).a(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 0).r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(0, 0).a(), 0.8f, 1.0e-6f);
}
