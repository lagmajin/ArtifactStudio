#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Distort.TimeDisplacement;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 3;
constexpr int kHeight = 1;

ImageF32x4RGBAWithCache makeMapFrame()
{
    constexpr std::array<float, kWidth * 4> pixels{
        0.0f, 0.0f, 0.0f, 0.6f,
        0.5f, 0.5f, 0.5f, 0.6f,
        1.0f, 1.0f, 1.0f, 0.6f,
    };
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

ImageF32x4RGBAWithCache makeUniformFrame(float value)
{
    const std::array<float, kWidth * 4> pixels{
        value, value, value, 0.7f,
        value, value, value, 0.7f,
        value, value, value, 0.7f,
    };
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class OffsetFrameSampler final : public IEffectFrameSampler {
public:
    bool sampleCurrentLayerFrame(
        std::int64_t frame,
        ImageF32x4RGBAWithCache& out) override
    {
        if (requestCount < static_cast<int>(requestedFrames.size())) {
            requestedFrames[static_cast<std::size_t>(requestCount++)] = frame;
        }
        if (frame == 96) out = makeUniformFrame(0.2f);
        else if (frame == 100) out = makeUniformFrame(0.5f);
        else if (frame == 104) out = makeUniformFrame(0.8f);
        else return false;
        return true;
    }

    bool sampleCurrentLayerFrameRelative(
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleNamedInput(
        const QString&,
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    std::array<std::int64_t, 3> requestedFrames{};
    int requestCount = 0;
};

} // namespace

TEST(TimeDisplacementEffectContractTest, MapsSourceChannelToRoundedFrameOffsets)
{
    const auto map = makeMapFrame();
    OffsetFrameSampler sampler;
    EffectContext context;
    context.compositionFrame = 100;
    context.sampler = &sampler;

    TimeDisplacementEffect effect;
    effect.setMaxOffsetFrames(4.0f);
    effect.setSourceChannel(TimeDisplaceChannel::Red);
    effect.setFrameBlend(false);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(map, output);

    ASSERT_EQ(sampler.requestCount, 3);
    EXPECT_EQ(sampler.requestedFrames[0], 96);
    EXPECT_EQ(sampler.requestedFrames[1], 100);
    EXPECT_EQ(sampler.requestedFrames[2], 104);
    EXPECT_NEAR(output.image().getPixel(0, 0).r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 0).r(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(2, 0).r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(0, 0).a(), 0.7f, 1.0e-6f);
}

TEST(TimeDisplacementEffectContractTest, MissingOffsetFramesPreserveMapPixels)
{
    const auto map = makeMapFrame();
    OffsetFrameSampler sampler;
    EffectContext context;
    context.compositionFrame = 200;
    context.sampler = &sampler;

    TimeDisplacementEffect effect;
    effect.setMaxOffsetFrames(4.0f);
    effect.setSourceChannel(TimeDisplaceChannel::Red);
    effect.setFrameBlend(false);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(map, output);

    ASSERT_EQ(sampler.requestCount, 3);
    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int x = 0; x < kWidth; ++x) {
        const auto before = map.image().getPixel(x, 0);
        const auto after = output.image().getPixel(x, 0);
        EXPECT_FLOAT_EQ(after.r(), before.r());
        EXPECT_FLOAT_EQ(after.g(), before.g());
        EXPECT_FLOAT_EQ(after.b(), before.b());
        EXPECT_FLOAT_EQ(after.a(), before.a());
    }
}
