#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.FilmDamage;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace {

constexpr int kWidth = 32;
constexpr int kHeight = 16;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeFilmSurface()
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4;
            pixels[offset] = 0.2f + 0.01f * static_cast<float>(x);
            pixels[offset + 1] = 0.3f + 0.01f * static_cast<float>(y);
            pixels[offset + 2] = 0.7f - 0.005f * static_cast<float>(x + y);
            pixels[offset + 3] = 0.1f + 0.02f * static_cast<float>((x + y) % 10);
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void setFilmProperty(Artifact::FilmDamageEffect& effect,
                     const char* name, const QVariant& value)
{
    effect.setPropertyValue(ArtifactCore::UniString(name), value);
}

void configureDeterministicDamage(Artifact::FilmDamageEffect& effect)
{
    setFilmProperty(effect, "Grain", QVariant(0.2));
    setFilmProperty(effect, "Dust", QVariant(0.0));
    setFilmProperty(effect, "Scratches", QVariant(0.0));
    setFilmProperty(effect, "Gate Weave", QVariant(0.0));
    setFilmProperty(effect, "Flicker", QVariant(0.0));
    setFilmProperty(effect, "Film Burn", QVariant(0.0));
    setFilmProperty(effect, "Evolution", QVariant(0.125));
    setFilmProperty(effect, "Seed", QVariant(314));
}

} // namespace

TEST(FilmDamageEffectContractTest, FixedSeedAndEvolutionRepeatRgbAndPreserveAlpha)
{
    const auto source = makeFilmSurface();
    ArtifactCore::ImageF32x4RGBAWithCache first;
    ArtifactCore::ImageF32x4RGBAWithCache second;
    Artifact::FilmDamageEffect effect;
    configureDeterministicDamage(effect);

    effect.applyCPUOnly(source, first);
    effect.applyCPUOnly(source, second);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto a = first.image().getPixel(x, y);
            const auto b = second.image().getPixel(x, y);
            const auto input = source.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(a.r(), b.r());
            EXPECT_FLOAT_EQ(a.g(), b.g());
            EXPECT_FLOAT_EQ(a.b(), b.b());
            EXPECT_FLOAT_EQ(a.a(), input.a());
        }
    }
}

TEST(FilmDamageEffectContractTest,
     EvolutionUsesDeterministicTwentyFourStepPerSecondFrameKeys)
{
    const auto source = makeFilmSurface();
    ArtifactCore::ImageF32x4RGBAWithCache first;
    ArtifactCore::ImageF32x4RGBAWithCache sameFrameKey;
    ArtifactCore::ImageF32x4RGBAWithCache nextFrameKey;
    Artifact::FilmDamageEffect effect;
    setFilmProperty(effect, "Grain", QVariant(0.35));
    setFilmProperty(effect, "Dust", QVariant(0.0));
    setFilmProperty(effect, "Scratches", QVariant(0.0));
    setFilmProperty(effect, "Gate Weave", QVariant(0.0));
    setFilmProperty(effect, "Flicker", QVariant(0.0));
    setFilmProperty(effect, "Film Burn", QVariant(0.0));
    setFilmProperty(effect, "Seed", QVariant(271));
    setFilmProperty(effect, "Evolution", QVariant(0.125)); // frame key 3
    effect.applyCPUOnly(source, first);

    setFilmProperty(effect, "Evolution", QVariant(0.13)); // still frame key 3
    effect.applyCPUOnly(source, sameFrameKey);
    setFilmProperty(effect, "Evolution", QVariant(0.17)); // frame key 4
    effect.applyCPUOnly(source, nextFrameKey);

    const float* firstPixels = first.image().rgba32fData();
    const float* samePixels = sameFrameKey.image().rgba32fData();
    const float* nextPixels = nextFrameKey.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(samePixels, nullptr);
    ASSERT_NE(nextPixels, nullptr);
    bool changedAtNextFrameKey = false;
    for (std::size_t pixel = 0; pixel < source.image().totalPixels(); ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::size_t index = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(firstPixels[index], samePixels[index])
                << "pixel=" << pixel << " channel=" << channel;
            changedAtNextFrameKey = changedAtNextFrameKey ||
                std::abs(firstPixels[index] - nextPixels[index]) > 1.0e-6f;
        }
        EXPECT_FLOAT_EQ(firstPixels[pixel * 4 + 3],
                        source.image().rgba32fData()[pixel * 4 + 3]);
        EXPECT_FLOAT_EQ(samePixels[pixel * 4 + 3],
                        source.image().rgba32fData()[pixel * 4 + 3]);
        EXPECT_FLOAT_EQ(nextPixels[pixel * 4 + 3],
                        source.image().rgba32fData()[pixel * 4 + 3]);
    }
    EXPECT_TRUE(changedAtNextFrameKey);
}

TEST(FilmDamageEffectContractTest,
     InPlaceMatchesOutOfPlaceAndPreservesDescriptorAndSource)
{
    const auto descriptor =
        ArtifactCore::SurfaceColorDescriptor::linearStraightRgba32Float();
    auto source = makeFilmSurface();
    source.image().setColorDescriptor(descriptor);
    auto baseline = source.image().DeepCopy();
    auto inPlace = source.image().DeepCopy();
    inPlace.setColorDescriptor(descriptor);
    ArtifactCore::ImageF32x4RGBAWithCache inPlaceSurface(inPlace);
    ArtifactCore::ImageF32x4RGBAWithCache outOfPlace;
    Artifact::FilmDamageEffect inPlaceEffect;
    Artifact::FilmDamageEffect outOfPlaceEffect;
    const auto configureDamagedEffect = [](Artifact::FilmDamageEffect& effect) {
        configureDeterministicDamage(effect);
        setFilmProperty(effect, "Dust", QVariant(0.25));
        setFilmProperty(effect, "Scratches", QVariant(0.2));
        setFilmProperty(effect, "Gate Weave", QVariant(1.25));
        setFilmProperty(effect, "Flicker", QVariant(0.1));
        setFilmProperty(effect, "Film Burn", QVariant(0.15));
    };
    configureDamagedEffect(inPlaceEffect);
    configureDamagedEffect(outOfPlaceEffect);

    outOfPlaceEffect.applyCPUOnly(source, outOfPlace);
    inPlaceEffect.applyCPUOnly(inPlaceSurface, inPlaceSurface);

    const float* original = source.image().rgba32fData();
    const float* baselinePixels = baseline.rgba32fData();
    const float* expected = outOfPlace.image().rgba32fData();
    const float* actual = inPlaceSurface.image().rgba32fData();
    ASSERT_NE(original, nullptr);
    ASSERT_NE(baselinePixels, nullptr);
    ASSERT_NE(expected, nullptr);
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(source.image().colorDescriptor(), descriptor);
    EXPECT_EQ(outOfPlace.image().colorDescriptor(), descriptor);
    EXPECT_EQ(inPlaceSurface.image().colorDescriptor(), descriptor);
    EXPECT_NE(actual, original);
    for (std::size_t channel = 0;
         channel < source.image().totalPixels() * 4; ++channel) {
        EXPECT_FLOAT_EQ(original[channel], baselinePixels[channel])
            << "source channel=" << channel;
        EXPECT_FLOAT_EQ(actual[channel], expected[channel])
            << "output channel=" << channel;
    }
}

TEST(FilmDamageEffectContractTest,
     PublishedControlsClampFiniteValuesAndResetNonFiniteValues)
{
    Artifact::FilmDamageEffect effect;
    setFilmProperty(effect, "Grain", QVariant(-1.0));
    setFilmProperty(effect, "Dust", QVariant(2.0));
    setFilmProperty(effect, "Scratches",
                    QVariant(std::numeric_limits<double>::quiet_NaN()));
    setFilmProperty(effect, "Gate Weave", QVariant(25.0));
    setFilmProperty(effect, "Flicker",
                    QVariant(std::numeric_limits<double>::infinity()));
    setFilmProperty(effect, "Film Burn", QVariant(-3.0));
    setFilmProperty(effect, "Evolution",
                    QVariant(std::numeric_limits<double>::quiet_NaN()));
    setFilmProperty(effect, "Seed", QVariant(-99));

    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 8u);
    constexpr std::array<const char*, 8> expectedNames{
        "Grain", "Dust", "Scratches", "Gate Weave", "Flicker",
        "Film Burn", "Evolution", "Seed"};
    constexpr std::array<float, 8> expectedValues{
        0.0f, 1.0f, 0.1f, 20.0f, 0.08f, 0.0f, 0.0f, -99.0f};
    for (std::size_t index = 0; index < properties.size(); ++index) {
        EXPECT_EQ(properties[index].name(), QString::fromLatin1(expectedNames[index]))
            << "property=" << index;
        EXPECT_FLOAT_EQ(properties[index].getValue().toFloat(), expectedValues[index])
            << "property=" << expectedNames[index];
    }
}

TEST(FilmDamageEffectContractTest, ZeroDamageControlsLeaveTheSurfaceUnchanged)
{
    const auto source = makeFilmSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::FilmDamageEffect effect;
    setFilmProperty(effect, "Grain", QVariant(0.0));
    setFilmProperty(effect, "Dust", QVariant(0.0));
    setFilmProperty(effect, "Scratches", QVariant(0.0));
    setFilmProperty(effect, "Gate Weave", QVariant(0.0));
    setFilmProperty(effect, "Flicker", QVariant(0.0));
    setFilmProperty(effect, "Film Burn", QVariant(0.0));

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto expected = source.image().getPixel(x, y);
            EXPECT_NEAR(actual.r(), expected.r(), 1.0e-6f);
            EXPECT_NEAR(actual.g(), expected.g(), 1.0e-6f);
            EXPECT_NEAR(actual.b(), expected.b(), 1.0e-6f);
            EXPECT_FLOAT_EQ(actual.a(), expected.a());
        }
    }
}
