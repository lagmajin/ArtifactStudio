#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.Echo;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixel = std::array<float, 4>;

ArtifactCore::ImageF32x4RGBAWithCache makePixel(const Pixel& pixel)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixel.data(), 1, 1);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

class FixedEchoSampler final : public Artifact::IEffectFrameSampler {
public:
    FixedEchoSampler(Pixel first, Pixel second)
        : frames_{first, second}
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
        requestedOffsets.push_back(offset);
        if (offset == -1) {
            out = makePixel(frames_[0]);
            return true;
        }
        if (offset == -2) {
            out = makePixel(frames_[1]);
            return true;
        }
        return false;
    }

    bool sampleNamedInput(
        const QString&, std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    std::vector<std::int64_t> requestedOffsets;

private:
    std::array<Pixel, 2> frames_;
};

void applyWithSampler(
    Artifact::EchoEffect& effect,
    FixedEchoSampler& sampler,
    ArtifactCore::ImageF32x4RGBAWithCache& output)
{
    Artifact::EffectContext context;
    context.sampler = &sampler;
    effect.setContext(context);
    effect.applyCPUOnly(makePixel(Pixel{0.2f, 0.2f, 0.2f, 0.4f}), output);
}

} // namespace

TEST(EchoEffectContractTest, AdditiveModeNormalizesWeightedHistoryAndCurrentFrame)
{
    FixedEchoSampler sampler(
        Pixel{0.8f, 0.8f, 0.8f, 0.2f},
        Pixel{0.4f, 0.4f, 0.4f, 0.8f});
    Artifact::EchoEffect effect;
    effect.setEchoCount(2);
    effect.setDecay(0.5f);
    effect.setBlendOperator(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, sampler, output);

    EXPECT_EQ(sampler.requestedOffsets,
              (std::vector<std::int64_t>{-1, -2}));
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_NEAR(result[0], 0.4f, 1.0e-6f);
    EXPECT_NEAR(result[1], 0.4f, 1.0e-6f);
    EXPECT_NEAR(result[2], 0.4f, 1.0e-6f);
    EXPECT_NEAR(result[3], 0.4f, 1.0e-6f);
}

TEST(EchoEffectContractTest, MixModeAppliesDecayWeightedSequentialBlends)
{
    FixedEchoSampler sampler(
        Pixel{0.8f, 0.8f, 0.8f, 0.2f},
        Pixel{0.4f, 0.4f, 0.4f, 0.8f});
    Artifact::EchoEffect effect;
    effect.setEchoCount(2);
    effect.setDecay(0.5f);
    effect.setBlendOperator(1.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, sampler, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_NEAR(result[0], 0.475f, 1.0e-6f);
    EXPECT_NEAR(result[1], 0.475f, 1.0e-6f);
    EXPECT_NEAR(result[2], 0.475f, 1.0e-6f);
    EXPECT_NEAR(result[3], 0.475f, 1.0e-6f);
}

TEST(EchoEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::EchoEffect effect;
    effect.setEchoCount(0);
    EXPECT_EQ(effect.echoCount(), 1);
    effect.setEchoCount(20);
    EXPECT_EQ(effect.echoCount(), 16);

    effect.setDecay(-1.0f);
    EXPECT_FLOAT_EQ(effect.decay(), 0.0f);
    effect.setDecay(2.0f);
    EXPECT_FLOAT_EQ(effect.decay(), 1.0f);
    effect.setDecay(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.decay(), 0.5f);

    effect.setBlendOperator(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.blendOperator(), 0.0f);
}