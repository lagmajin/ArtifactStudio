#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.DifferenceMatte;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

ArtifactCore::ImageF32x4RGBAWithCache makeImage(
    const std::array<float, 8>& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 2, 1);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

class FixedReferenceSampler final : public Artifact::IEffectFrameSampler {
public:
    explicit FixedReferenceSampler(ArtifactCore::ImageF32x4RGBAWithCache reference)
        : reference_(std::move(reference))
    {
    }

    bool sampleCurrentLayerFrame(
        std::int64_t, ArtifactCore::ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleCurrentLayerFrameRelative(
        std::int64_t offset, ArtifactCore::ImageF32x4RGBAWithCache& out) override
    {
        requestedOffset = offset;
        out = reference_;
        return true;
    }

    bool sampleNamedInput(
        const QString&, std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    std::int64_t requestedOffset = 0;

private:
    ArtifactCore::ImageF32x4RGBAWithCache reference_;
};

} // namespace

TEST(DifferenceMatteEffectContractTest, WritesMaximumRgbDifferenceAsOpaqueGrayscale)
{
    const auto source = makeImage({
        0.8f, 0.2f, 0.1f, 0.25f,
        0.3f, 0.6f, 0.9f, 0.0f,
    });
    const auto reference = makeImage({
        0.5f, 0.4f, 0.1f, 1.0f,
        0.3f, 0.2f, 0.5f, 1.0f,
    });
    FixedReferenceSampler sampler(reference);
    Artifact::EffectContext context;
    context.sampler = &sampler;

    Artifact::DifferenceMatteEffect effect;
    effect.setContext(context);
    effect.setThreshold(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    EXPECT_EQ(sampler.requestedOffset, -1);
    const auto first = output.image().getPixel(0, 0);
    EXPECT_NEAR(first.r(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(first.g(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(first.b(), 0.3f, 1.0e-6f);
    EXPECT_FLOAT_EQ(first.a(), 1.0f);

    const auto second = output.image().getPixel(1, 0);
    EXPECT_NEAR(second.r(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(second.g(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(second.b(), 0.4f, 1.0e-6f);
    EXPECT_FLOAT_EQ(second.a(), 1.0f);
}

TEST(DifferenceMatteEffectContractTest, ThresholdIsInclusiveAndProducesBinaryMatte)
{
    const auto source = makeImage({
        0.5f, 0.5f, 0.5f, 0.1f,
        0.5f, 0.5f, 0.5f, 0.2f,
    });
    const auto reference = makeImage({
        0.375f, 0.5f, 0.5f, 0.9f,
        0.376f, 0.5f, 0.5f, 0.8f,
    });
    FixedReferenceSampler sampler(reference);
    Artifact::EffectContext context;
    context.sampler = &sampler;

    Artifact::DifferenceMatteEffect effect;
    effect.setContext(context);
    effect.setThreshold(0.125f);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    EXPECT_FLOAT_EQ(output.image().getPixel(0, 0).r(), 1.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(1, 0).r(), 0.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(0, 0).a(), 1.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(1, 0).a(), 1.0f);
}

TEST(DifferenceMatteEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::DifferenceMatteEffect effect;
    effect.setReferenceOffset(0);
    EXPECT_EQ(effect.referenceOffset(), 1);
    effect.setReferenceOffset(120);
    EXPECT_EQ(effect.referenceOffset(), 60);

    effect.setThreshold(-1.0f);
    EXPECT_FLOAT_EQ(effect.threshold(), 0.0f);
    effect.setThreshold(2.0f);
    EXPECT_FLOAT_EQ(effect.threshold(), 1.0f);
    effect.setThreshold(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.threshold(), 0.0f);
}