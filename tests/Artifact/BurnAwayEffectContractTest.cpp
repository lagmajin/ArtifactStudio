#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.BurnAway;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;

ArtifactCore::ImageF32x4RGBAWithCache makeSurface()
{
    // Every pixel is opaque so the char band can be judged on colour only.
    constexpr std::array<float, kWidth * kHeight * 4> pixels{
        0.80f, 0.70f, 0.60f, 1.00f,
        0.20f, 0.40f, 0.60f, 1.00f,
        0.10f, 0.20f, 0.30f, 1.00f,
        0.50f, 0.50f, 0.50f, 1.00f,
        0.90f, 0.10f, 0.20f, 1.00f,
        0.30f, 0.60f, 0.20f, 1.00f,
        0.70f, 0.20f, 0.40f, 1.00f,
        0.40f, 0.30f, 0.90f, 1.00f,
        0.60f, 0.60f, 0.10f, 1.00f,
        0.10f, 0.70f, 0.30f, 1.00f,
        0.20f, 0.20f, 0.80f, 1.00f,
        0.80f, 0.80f, 0.20f, 1.00f,
        0.55f, 0.45f, 0.35f, 1.00f,
        0.15f, 0.25f, 0.35f, 1.00f,
        0.35f, 0.15f, 0.45f, 1.00f,
        0.45f, 0.35f, 0.15f, 1.00f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

ArtifactCore::ImageF32x4RGBAWithCache makeGraySurface()
{
    std::vector<float> pixels(static_cast<std::size_t>(kWidth * kHeight * 4), 0.0f);
    for (std::size_t i = 0; i < static_cast<std::size_t>(kWidth * kHeight); ++i) {
        pixels[i * 4 + 0] = 0.80f;
        pixels[i * 4 + 1] = 0.80f;
        pixels[i * 4 + 2] = 0.80f;
        pixels[i * 4 + 3] = 1.00f;
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

float luminanceOf(float red, float green, float blue)
{
    return 0.2126f * red + 0.7152f * green + 0.0722f * blue;
}

} // namespace

TEST(BurnAwayEffectContractTest, ZeroProgressKeepsTheSourceUntouched)
{
    const auto source = makeSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::BurnAwayEffect effect;
    effect.setProgress(0.0f);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto expected = source.image().getPixel(x, y);
            const auto actual = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), expected.r());
            EXPECT_FLOAT_EQ(actual.g(), expected.g());
            EXPECT_FLOAT_EQ(actual.b(), expected.b());
            EXPECT_FLOAT_EQ(actual.a(), expected.a());
        }
    }
}

TEST(BurnAwayEffectContractTest, FullProgressRemovesEveryPixel)
{
    const auto source = makeSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::BurnAwayEffect effect;
    effect.setProgress(1.0f);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.a(), 0.0f);
        }
    }
}

TEST(BurnAwayEffectContractTest, HalfProgressErodesTheBottomOfTheLayer)
{
    const auto source = makeSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::BurnAwayEffect effect;
    effect.setProgress(0.5f);
    effect.setSpread(0.0f);
    effect.setEmberIntensity(0.0f);
    effect.setCharAmount(0.0f);

    effect.applyCPUOnly(source, output);

    // The burn starts at the bottom edge, so the bottom row is consumed while
    // the top row survives.
    const auto top = output.image().getPixel(0, 0);
    const auto bottom = output.image().getPixel(0, kHeight - 1);
    EXPECT_FLOAT_EQ(top.a(), source.image().getPixel(0, 0).a());
    EXPECT_LT(bottom.a(), source.image().getPixel(0, kHeight - 1).a());
}

TEST(BurnAwayEffectContractTest, CharBandDarkensWarmChannelsInTheRightOrder)
{
    // A neutral grey surface makes the multiplicative char tint comparable
    // across channels: the warm order R > G > B only holds for a neutral
    // source, so a coloured pixel cannot be used as the reference.
    const auto source = makeGraySurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::BurnAwayEffect effect;
    effect.setProgress(0.5f);
    effect.setSpread(0.0f);
    effect.setEdgeWidth(0.3f);
    effect.setEmberIntensity(0.0f);
    effect.setCharAmount(1.0f);

    effect.applyCPUOnly(source, output);

    // The surface buffer is B,G,R,A, so a wrong channel order shows up as a
    // cool cast instead of the warm char tint.  Find a pixel the char band
    // touched and compare its channels.
    bool found = false;
    float bestRed = 0.0f;
    float bestGreen = 0.0f;
    float bestBlue = 0.0f;
    float bestSourceBlue = 0.0f;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto sourcePixel = source.image().getPixel(x, y);
            const auto pixel = output.image().getPixel(x, y);
            if (pixel.a() < 0.999f) {
                continue;  // burnt pixels carry no char information
            }
            if (luminanceOf(pixel.r(), pixel.g(), pixel.b()) >=
                luminanceOf(sourcePixel.r(), sourcePixel.g(), sourcePixel.b()) - 1e-3f) {
                continue;  // untouched pixels carry no char information
            }
            bestRed = pixel.r();
            bestGreen = pixel.g();
            bestBlue = pixel.b();
            bestSourceBlue = sourcePixel.b();
            found = true;
        }
    }
    ASSERT_TRUE(found) << "no pixel was darkened by the char band";
    EXPECT_GT(bestRed, bestGreen);
    EXPECT_GT(bestGreen, bestBlue);
    EXPECT_LT(bestBlue, bestSourceBlue);
}

TEST(BurnAwayEffectContractTest, GpuPlanCarriesTheBurnParameters)
{
    Artifact::BurnAwayEffect effect;
    effect.setProgress(0.25f);
    effect.setDirection(1.0f);
    effect.setEdgeWidth(0.05f);
    effect.setCharAmount(0.5f);
    effect.setEmberIntensity(1.5f);
    effect.setEmberWidth(0.01f);
    effect.setEdgeSoftness(0.002f);
    effect.setNoiseScale(0.1f);
    effect.setOctaves(3);
    effect.setSeed(7);
    effect.setSpread(0.2f);
    effect.setFlicker(0.1f);
    effect.setResidue(0.3f);
    effect.setSparkDensity(0.4f);
    Artifact::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, Artifact::GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey,
              Artifact::gpuGenericKeyFromString("burn_away"));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 0.25f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 1.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 0.05f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], 0.5f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[4], 1.5f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[6], 0.01f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[7], 0.002f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[8], 0.1f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[9], 3.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[10], 7.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[11], 0.2f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[12], 0.1f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[14], 0.3f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[15], 0.4f);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 0u);
}

TEST(BurnAwayEffectContractTest, PackedEmberColourKeepsTheChannelsApart)
{
    Artifact::BurnAwayEffect effect;
    effect.setEmberColor(QColor(255, 140, 40));
    Artifact::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    const float packed = stack.nodes[0].parameters[5];
    // The shader unpacks red from the high byte and blue from the low one.
    const int red = static_cast<int>(std::floor(packed / 65536.0f));
    const int green = static_cast<int>(std::floor(std::fmod(packed, 65536.0f) / 256.0f));
    const int blue = static_cast<int>(std::fmod(packed, 256.0f));
    EXPECT_EQ(red, 255);
    EXPECT_EQ(green, 140);
    EXPECT_EQ(blue, 40);
}

TEST(BurnAwayEffectContractTest, ResidueKeepsAFaintAshFilm)
{
    const auto source = makeGraySurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::BurnAwayEffect effect;
    effect.setProgress(1.0f);
    effect.setResidue(0.5f);

    effect.applyCPUOnly(source, output);

    // Progress 1 burns the layer away, but an explicit residue keeps a faint
    // ash film instead of a fully transparent hole.
    const auto pixel = output.image().getPixel(0, 0);
    EXPECT_GT(pixel.a(), 0.0f);
    EXPECT_LT(pixel.a(), 1.0f);
}

TEST(BurnAwayEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::BurnAwayEffect effect;

    effect.setProgress(-1.0f);
    EXPECT_FLOAT_EQ(effect.progress(), 0.0f);
    effect.setProgress(2.0f);
    EXPECT_FLOAT_EQ(effect.progress(), 1.0f);
    effect.setProgress(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.progress(), 0.0f);

    effect.setDirection(-3.0f);
    EXPECT_FLOAT_EQ(effect.direction(), 0.0f);
    effect.setDirection(9.0f);
    EXPECT_FLOAT_EQ(effect.direction(), 1.0f);

    effect.setEdgeWidth(9.0f);
    EXPECT_FLOAT_EQ(effect.edgeWidth(), 0.5f);
    effect.setEdgeWidth(-1.0f);
    EXPECT_FLOAT_EQ(effect.edgeWidth(), 0.0f);

    effect.setCharAmount(5.0f);
    EXPECT_FLOAT_EQ(effect.charAmount(), 1.0f);
    effect.setEmberIntensity(-1.0f);
    EXPECT_FLOAT_EQ(effect.emberIntensity(), 0.0f);
    effect.setEmberIntensity(100.0f);
    EXPECT_FLOAT_EQ(effect.emberIntensity(), 8.0f);
    effect.setEmberWidth(4.0f);
    EXPECT_FLOAT_EQ(effect.emberWidth(), 0.5f);

    effect.setEdgeSoftness(3.0f);
    EXPECT_FLOAT_EQ(effect.edgeSoftness(), 0.2f);
    effect.setNoiseScale(0.0f);
    EXPECT_FLOAT_EQ(effect.noiseScale(), 0.001f);
    effect.setNoiseScale(100.0f);
    EXPECT_FLOAT_EQ(effect.noiseScale(), 1.0f);

    effect.setOctaves(0);
    EXPECT_EQ(effect.octaves(), 1);
    effect.setOctaves(99);
    EXPECT_EQ(effect.octaves(), 8);

    effect.setSeed(-5);
    EXPECT_EQ(effect.seed(), 0);
    effect.setSeed(999999);
    EXPECT_EQ(effect.seed(), 9999);

    effect.setSpread(2.0f);
    EXPECT_FLOAT_EQ(effect.spread(), 1.0f);
    effect.setFlicker(-1.0f);
    EXPECT_FLOAT_EQ(effect.flicker(), 0.0f);
}
