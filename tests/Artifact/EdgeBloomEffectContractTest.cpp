#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow.EdgeBloom;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 5;
constexpr int kHeight = 5;

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

} // namespace

TEST(EdgeBloomEffectContractTest, BrightUniformSurfaceReceivesBloomWithoutChangingAlpha)
{
    const auto source = makeUniformSurface(0.5f, 0.4f);
    ImageF32x4RGBAWithCache output;
    EdgeBloomEffect effect;
    effect.setThreshold(0.1f);
    effect.setThresholdSoftness(0.0f);
    effect.setRadius(0.5f);
    effect.setQuality(1);
    effect.setAmount(1.0f);
    effect.setEdgeBoost(0.0f);
    effect.setTintMix(1.0f);

    effect.applyCPUOnly(source, output);

    const auto pixel = output.image().getPixel(2, 2);
    EXPECT_GT(pixel.r(), 0.5f);
    EXPECT_NEAR(pixel.r(), pixel.g(), 1.0e-6f);
    EXPECT_NEAR(pixel.g(), pixel.b(), 1.0e-6f);
    EXPECT_NEAR(pixel.a(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(2, 2).r(), 0.5f, 1.0e-6f);
}


TEST(EdgeBloomEffectContractTest, SubthresholdUniformSurfaceProducesNoBloom)
{
    const auto source = makeUniformSurface(0.2f, 0.6f);
    ImageF32x4RGBAWithCache output;
    EdgeBloomEffect effect;
    effect.setThreshold(0.8f);
    effect.setThresholdSoftness(0.0f);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto original = source.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), original.r());
            EXPECT_FLOAT_EQ(actual.g(), original.g());
            EXPECT_FLOAT_EQ(actual.b(), original.b());
            EXPECT_FLOAT_EQ(actual.a(), original.a());
        }
    }
}

TEST(EdgeBloomEffectContractTest, ZeroTintMixAddsEqualWhiteBloomToEveryColorChannel)
{
    constexpr int width = 5;
    constexpr int height = 5;
    constexpr float red = 0.8f;
    constexpr float green = 0.6f;
    constexpr float blue = 0.4f;
    constexpr float alpha = 0.35f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = red;
        pixels[offset + 1] = green;
        pixels[offset + 2] = blue;
        pixels[offset + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    EdgeBloomEffect effect;
    effect.setThreshold(0.1f);
    effect.setThresholdSoftness(0.0f);
    effect.setRadius(0.5f);
    effect.setAmount(1.0f);
    effect.setEdgeBoost(0.0f);
    effect.setTintMix(0.0f);

    effect.applyCPUOnly(source, output);

    const auto before = source.image().getPixel(2, 2);
    const auto after = output.image().getPixel(2, 2);
    const float redIncrease = after.r() - before.r();
    const float greenIncrease = after.g() - before.g();
    const float blueIncrease = after.b() - before.b();
    EXPECT_GT(redIncrease, 0.0f);
    EXPECT_NEAR(redIncrease, greenIncrease, 1.0e-6f);
    EXPECT_NEAR(greenIncrease, blueIncrease, 1.0e-6f);
    EXPECT_NEAR(after.a(), alpha, 1.0e-6f);
}
