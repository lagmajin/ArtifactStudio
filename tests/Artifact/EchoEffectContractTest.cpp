#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.Echo;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 3;
constexpr int kHeight = 3;

ImageF32x4RGBAWithCache makeUniformSurface(float value, float alpha)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = value;
        pixels[i + 1] = value;
        pixels[i + 2] = value;
        pixels[i + 3] = alpha;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class PreviousFrameSampler final : public IEffectFrameSampler {
public:
    explicit PreviousFrameSampler(const ImageF32x4RGBAWithCache& previous)
        : previous_(previous)
    {
    }

    bool sampleCurrentLayerFrame(
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleCurrentLayerFrameRelative(
        std::int64_t frameOffset,
        ImageF32x4RGBAWithCache& out) override
    {
        lastFrameOffset = frameOffset;
        out = previous_;
        return frameOffset == -1;
    }

    bool sampleNamedInput(
        const QString&,
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    std::int64_t lastFrameOffset = 0;

private:
    ImageF32x4RGBAWithCache previous_;
};

} // namespace

TEST(EchoEffectContractTest, DecayedPreviousFrameContributesToNormalizedAfterimage)
{
    const auto current = makeUniformSurface(0.2f, 0.3f);
    const auto previous = makeUniformSurface(0.8f, 0.7f);
    PreviousFrameSampler sampler(previous);
    EffectContext context;
    context.sampler = &sampler;

    EchoEffect effect;
    effect.setEchoCount(1);
    effect.setDecay(0.5f);
    effect.setBlendOperator(0.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.3f / 1.5f + 0.7f / 3.0f,
                1.0e-6f);
}

TEST(EchoEffectContractTest, MissingFrameSamplerPreservesCurrentFrame)
{
    const auto current = makeUniformSurface(0.37f, 0.42f);
    ImageF32x4RGBAWithCache output;
    EchoEffect effect;

    effect.applyCPUOnly(current, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = current.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(after.r(), before.r());
            EXPECT_FLOAT_EQ(after.g(), before.g());
            EXPECT_FLOAT_EQ(after.b(), before.b());
            EXPECT_FLOAT_EQ(after.a(), before.a());
        }
    }
}

TEST(EchoEffectContractTest, BlendOperatorInterpolatesTowardDecayedEcho)
{
    const auto current = makeUniformSurface(0.2f, 0.3f);
    const auto previous = makeUniformSurface(0.8f, 0.7f);
    PreviousFrameSampler sampler(previous);
    EffectContext context;
    context.sampler = &sampler;

    EchoEffect effect;
    effect.setEchoCount(1);
    effect.setDecay(0.5f);
    effect.setBlendOperator(1.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    const auto pixel = output.image().getPixel(1, 1);
    EXPECT_NEAR(pixel.r(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(pixel.a(), 0.5f, 1.0e-6f);
}
