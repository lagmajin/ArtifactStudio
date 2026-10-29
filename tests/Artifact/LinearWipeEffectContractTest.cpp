#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Rasterizer.LinearWipe;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(LinearWipeEffectContractTest, LinearWipeReducesAlphaAcrossImageAndKeepsRgb)
{
    constexpr int width = 4;
    constexpr std::array<float, width * 4> pixels = {
        0.8f, 0.4f, 0.2f, 0.75f,
        0.8f, 0.4f, 0.2f, 0.75f,
        0.8f, 0.4f, 0.2f, 0.75f,
        0.8f, 0.4f, 0.2f, 0.75f,
    };
    const auto source = makeSurface(pixels.data(), width, 1);
    ImageF32x4RGBAWithCache output;
    LinearWipeEffect effect;
    effect.setAngle(0.0f);
    effect.setSoftness(1.0f);

    effect.applyCPUOnly(source, output);

    const auto first = output.image().getPixel(0, 0);
    const auto middle = output.image().getPixel(2, 0);
    const auto last = output.image().getPixel(3, 0);
    EXPECT_NEAR(first.a(), 0.75f, 1.0e-6f);
    EXPECT_NEAR(middle.a(), 0.375f, 1.0e-6f);
    EXPECT_NEAR(last.a(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(middle.r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(middle.g(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(2, 0).a(), 0.75f, 1.0e-6f);
}


TEST(LinearWipeEffectContractTest, HorizontalWipeKeepsRgbAndMonotonicallyReducesAlphaPerRow)
{
    constexpr int width = 5;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (int index = 0; index < width * height; ++index) {
        pixels[index * 4] = 0.1f + static_cast<float>(index) * 0.01f;
        pixels[index * 4 + 1] = 0.3f;
        pixels[index * 4 + 2] = 0.7f;
        pixels[index * 4 + 3] = 0.8f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    LinearWipeEffect effect;
    effect.setAngle(0.0f);
    effect.setSoftness(0.0f);
    effect.setFeather(0.0f);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < height; ++y) {
        float previousAlpha = 1.0f;
        for (int x = 0; x < width; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto original = source.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), original.r());
            EXPECT_FLOAT_EQ(actual.g(), original.g());
            EXPECT_FLOAT_EQ(actual.b(), original.b());
            EXPECT_GE(actual.a(), 0.0f);
            EXPECT_LE(actual.a(), original.a());
            EXPECT_LE(actual.a(), previousAlpha + 1.0e-6f);
            previousAlpha = actual.a();
        }
    }
}

TEST(LinearWipeEffectContractTest, EquivalentWrappedAnglesProduceIdenticalWipes)
{
    constexpr int width = 6;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (int index = 0; index < width * height; ++index) {
        pixels[index * 4] = 0.2f;
        pixels[index * 4 + 1] = 0.4f;
        pixels[index * 4 + 2] = 0.6f;
        pixels[index * 4 + 3] = 0.75f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache negativeAngleOutput;
    ImageF32x4RGBAWithCache wrappedAngleOutput;
    LinearWipeEffect effect;
    effect.setSoftness(0.6f);
    effect.setFeather(0.1f);
    effect.setAngle(-90.0f);
    effect.applyCPUOnly(source, negativeAngleOutput);

    effect.setAngle(270.0f);
    effect.applyCPUOnly(source, wrappedAngleOutput);

    EXPECT_FLOAT_EQ(effect.angle(), 270.0f);
    const float* negativeAngle = negativeAngleOutput.image().rgba32fData();
    const float* wrappedAngle = wrappedAngleOutput.image().rgba32fData();
    ASSERT_NE(negativeAngle, nullptr);
    ASSERT_NE(wrappedAngle, nullptr);
    for (std::size_t component = 0; component < pixels.size(); ++component) {
        EXPECT_FLOAT_EQ(negativeAngle[component], wrappedAngle[component])
            << "component=" << component;
    }
}
