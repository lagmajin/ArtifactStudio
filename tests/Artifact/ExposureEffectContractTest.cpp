#include <gtest/gtest.h>

#include <QRectF>

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>

import Artifact.Render.PointwiseEffectFusion;
import ExposureEffect;
import FloatRGBA;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(int width, int height, const float* rgba)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

void expectPixelNear(const ImageF32x4RGBAWithCache& surface, int x, int y,
                     const FloatRGBA& expected, float tolerance = 1e-6f)
{
    const FloatRGBA actual = surface.image().getPixel(x, y);
    EXPECT_NEAR(actual.r(), expected.r(), tolerance);
    EXPECT_NEAR(actual.g(), expected.g(), tolerance);
    EXPECT_NEAR(actual.b(), expected.b(), tolerance);
    EXPECT_NEAR(actual.a(), expected.a(), tolerance);
}

} // namespace

TEST(ExposureEffectContractTest, CpuExposureDoublesRgbAndPreservesAlpha)
{
    const float pixels[] = {0.10f, 0.25f, 0.40f, 0.0f,
                            0.20f, 0.35f, 0.80f, 0.37f};
    auto source = makeSurface(2, 1, pixels);
    ImageF32x4RGBAWithCache output;
    ExposureEffect effect;
    effect.setExposure(1.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.20f, 0.50f, 0.80f, 0.0f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.40f, 0.70f, 1.0f, 0.37f));
    expectPixelNear(source, 1, 0, FloatRGBA(0.20f, 0.35f, 0.80f, 0.37f));
}

TEST(ExposureEffectContractTest, OffsetAndGammaApplyAfterExposure)
{
    const float pixels[] = {0.25f, 0.50f, 0.81f, 0.60f};
    auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    ExposureEffect effect;
    effect.setExposure(0.0f);
    effect.setOffset(0.10f);
    effect.setGammaCorrection(2.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0,
                    FloatRGBA(std::sqrt(0.35f), std::sqrt(0.60f),
                              std::sqrt(0.91f), 0.60f));
}

TEST(ExposureEffectContractTest, DisabledEffectPreservesTransparentPixelExactly)
{
    const float pixels[] = {0.80f, 0.20f, 0.60f, 0.0f};
    auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    ExposureEffect effect;
    effect.setExposure(4.0f);
    effect.setEnabled(false);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.80f, 0.20f, 0.60f, 0.0f));
}

TEST(ExposureEffectContractTest, MixAndEffectRegionBlendAgainstOriginalPixels)
{
    const float pixels[] = {0.20f, 0.30f, 0.40f, 0.25f,
                            0.25f, 0.35f, 0.45f, 0.75f};
    auto source = makeSurface(2, 1, pixels);
    ImageF32x4RGBAWithCache output;
    ExposureEffect effect;
    effect.setExposure(1.0f);
    effect.setMix(0.5f);
    effect.setEffectRegion(QRectF(0.0, 0.0, 1.0, 1.0));

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.30f, 0.45f, 0.60f, 0.25f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.25f, 0.35f, 0.45f, 0.75f));
}

TEST(ExposureEffectContractTest, EffectRegionUsesPixelCentersForFractionalAndEdgeBounds)
{
    const float pixels[] = {
        0.10f, 0.20f, 0.30f, 0.1f,
        0.20f, 0.30f, 0.40f, 0.2f,
        0.30f, 0.40f, 0.50f, 0.3f,
        0.40f, 0.50f, 0.60f, 0.4f,
    };
    auto source = makeSurface(2, 2, pixels);
    ImageF32x4RGBAWithCache output;
    ExposureEffect effect;
    effect.setExposure(1.0f);
    effect.setEffectRegion(QRectF(1.1, 1.1, 0.8, 0.8));

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.10f, 0.20f, 0.30f, 0.1f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.20f, 0.30f, 0.40f, 0.2f));
    expectPixelNear(output, 0, 1, FloatRGBA(0.30f, 0.40f, 0.50f, 0.3f));
    expectPixelNear(output, 1, 1, FloatRGBA(0.80f, 1.0f, 1.0f, 0.4f));
}

TEST(ExposureEffectContractTest, ZeroMixDominatesAnEnabledEffectRegion)
{
    const float pixels[] = {0.25f, 0.5f, 0.75f, 0.4f};
    auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    ExposureEffect effect;
    effect.setExposure(2.0f);
    effect.setMix(0.0f);
    effect.setEffectRegion(QRectF(0.0, 0.0, 1.0, 1.0));

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.25f, 0.5f, 0.75f, 0.4f));
}

TEST(ExposureEffectContractTest, InvalidRegionIsClearedAndDoesNotRestrictEffect)
{
    const float pixels[] = {0.20f, 0.30f, 0.40f, 0.5f};
    auto source = makeSurface(1, 1, pixels);
    ImageF32x4RGBAWithCache output;
    ExposureEffect effect;
    effect.setExposure(1.0f);
    effect.setEffectRegion(QRectF(0.0, 0.0, -1.0, 1.0));

    EXPECT_FALSE(effect.hasEffectRegion());
    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.40f, 0.60f, 0.80f, 0.5f));
}

TEST(ExposureEffectContractTest, SettersClampFiniteValuesAndResetNonFiniteValues)
{
    ExposureEffect effect;
    effect.setExposure(20.0f);
    EXPECT_FLOAT_EQ(effect.exposure(), 5.0f);
    effect.setExposure(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.exposure(), 0.0f);

    effect.setOffset(-2.0f);
    EXPECT_FLOAT_EQ(effect.offset(), -0.5f);
    effect.setOffset(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.offset(), 0.0f);

    effect.setGammaCorrection(10.0f);
    EXPECT_FLOAT_EQ(effect.gammaCorrection(), 5.0f);
    effect.setGammaCorrection(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.gammaCorrection(), 1.0f);
}

TEST(ExposureEffectContractTest, PointwiseGpuDescriptionMatchesConfiguredParameters)
{
    ExposureEffect effect;
    effect.setExposure(1.5f);
    effect.setOffset(0.125f);
    effect.setGammaCorrection(2.0f);
    PointwiseEffectStack stack;
    std::uint32_t slot = 0;

    ASSERT_TRUE(effect.supportsCPU());
    ASSERT_TRUE(effect.supportsGPU());
    EXPECT_EQ(effect.gpuRasterEffectDomain(), GpuRasterEffectDomain::Pointwise);
    EXPECT_TRUE(effect.appendGpuPointwiseNodes(stack, slot));

    ASSERT_EQ(stack.nodes().size(), 3u);
    EXPECT_EQ(stack.nodes()[0].kind, PointwiseNodeKind::Exposure);
    EXPECT_EQ(stack.nodes()[1].kind, PointwiseNodeKind::Offset);
    EXPECT_EQ(stack.nodes()[2].kind, PointwiseNodeKind::Gamma);
    EXPECT_FLOAT_EQ(stack.parameters()[0][0], 1.5f);
    EXPECT_FLOAT_EQ(stack.parameters()[1][0], 0.125f);
    EXPECT_FLOAT_EQ(stack.parameters()[2][0], 2.0f);
    EXPECT_EQ(slot, 3u);
}

TEST(ExposureEffectContractTest, CpuFormulaAndMixMatchParameterGridForEveryPixel)
{
    const float pixels[] = {
        0.0f, 0.125f, 0.25f, 0.0f,
        0.10f, 0.30f, 0.60f, 0.2f,
        0.25f, 0.50f, 0.75f, 0.4f,
        0.40f, 0.60f, 0.80f, 0.6f,
        0.70f, 0.85f, 1.00f, 0.8f,
        1.00f, 0.50f, 0.00f, 1.0f,
    };
    const float exposureValues[] = {-3.0f, 0.0f, 2.0f};
    const float offsets[] = {-0.3f, 0.0f, 0.4f};
    const float gammas[] = {0.5f, 1.0f, 2.0f};
    const float mixes[] = {0.0f, 0.3f, 1.0f};
    const auto source = makeSurface(2, 3, pixels);

    for (const float exposure : exposureValues) {
        for (const float offset : offsets) {
            for (const float gamma : gammas) {
                for (const float mix : mixes) {
                    ExposureEffect effect;
                    effect.setExposure(exposure);
                    effect.setOffset(offset);
                    effect.setGammaCorrection(gamma);
                    effect.setMix(mix);
                    ImageF32x4RGBAWithCache output;
                    effect.applyCPUOnly(source, output);

                    ASSERT_EQ(output.width(), 2);
                    ASSERT_EQ(output.height(), 3);
                    for (int y = 0; y < 3; ++y) {
                        for (int x = 0; x < 2; ++x) {
                            const FloatRGBA input = source.image().getPixel(x, y);
                            const auto transform = [&](const float channel) {
                                const float adjusted = std::max(
                                    0.0f, channel * std::pow(2.0f, exposure) + offset);
                                const float graded = std::clamp(
                                    std::pow(adjusted, 1.0f / gamma), 0.0f, 1.0f);
                                return channel * (1.0f - mix) + graded * mix;
                            };
                            expectPixelNear(
                                output, x, y,
                                FloatRGBA(transform(input.r()), transform(input.g()),
                                          transform(input.b()), input.a()),
                                2e-6f);
                        }
                    }
                }
            }
        }
    }
}
