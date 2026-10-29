#include <gtest/gtest.h>

#include <array>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.FreezeFrame;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace {

ArtifactCore::ImageF32x4RGBAWithCache makePixel(float r, float g, float b, float a)
{
    const std::array<float, 4> pixels{r, g, b, a};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 1, 1);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void expectPixel(const ArtifactCore::ImageF32x4RGBAWithCache& image,
                 float r, float g, float b, float a)
{
    const auto pixel = image.image().getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), r);
    EXPECT_FLOAT_EQ(pixel.g(), g);
    EXPECT_FLOAT_EQ(pixel.b(), b);
    EXPECT_FLOAT_EQ(pixel.a(), a);
}

} // namespace

TEST(FreezeFrameEffectContractTest, HoldsMostRecentLiveFrameUntilReleased)
{
    const auto captured = makePixel(0.2f, 0.4f, 0.6f, 0.3f);
    const auto live = makePixel(0.8f, 0.1f, 0.25f, 0.9f);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::FreezeFrameEffect effect;

    effect.applyCPUOnly(captured, output);
    expectPixel(output, 0.2f, 0.4f, 0.6f, 0.3f);

    effect.freeze();
    EXPECT_TRUE(effect.isFrozen());
    effect.applyCPUOnly(live, output);
    expectPixel(output, 0.2f, 0.4f, 0.6f, 0.3f);

    effect.release();
    EXPECT_FALSE(effect.isFrozen());
    effect.applyCPUOnly(live, output);
    expectPixel(output, 0.8f, 0.1f, 0.25f, 0.9f);
}

TEST(FreezeFrameEffectContractTest, FrozenPropertyControlsCaptureState)
{
    const auto captured = makePixel(0.1f, 0.3f, 0.5f, 0.7f);
    const auto live = makePixel(0.9f, 0.6f, 0.3f, 0.1f);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::FreezeFrameEffect effect;
    effect.applyCPUOnly(captured, output);

    effect.setPropertyValue(ArtifactCore::UniString("frozen"), QVariant(true));
    ASSERT_TRUE(effect.isFrozen());
    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 1u);
    EXPECT_TRUE(properties[0].getValue().toBool());

    effect.applyCPUOnly(live, output);
    expectPixel(output, 0.1f, 0.3f, 0.5f, 0.7f);

    effect.setPropertyValue(ArtifactCore::UniString("frozen"), QVariant(false));
    EXPECT_FALSE(effect.isFrozen());
    effect.applyCPUOnly(live, output);
    expectPixel(output, 0.9f, 0.6f, 0.3f, 0.1f);
}
