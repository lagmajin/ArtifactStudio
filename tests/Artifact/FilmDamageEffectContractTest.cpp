#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
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

    int changedRgbPixels = 0;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto a = first.image().getPixel(x, y);
            const auto b = second.image().getPixel(x, y);
            const auto input = source.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(a.r(), b.r());
            EXPECT_FLOAT_EQ(a.g(), b.g());
            EXPECT_FLOAT_EQ(a.b(), b.b());
            EXPECT_FLOAT_EQ(a.a(), input.a());
            changedRgbPixels +=
                (std::abs(a.r() - input.r()) > 1.0e-6f ||
                 std::abs(a.g() - input.g()) > 1.0e-6f ||
                 std::abs(a.b() - input.b()) > 1.0e-6f) ? 1 : 0;
        }
    }
    EXPECT_GT(changedRgbPixels, 0);
}

TEST(FilmDamageEffectContractTest, ChangingSeedChangesTheGrainPattern)
{
    const auto source = makeFilmSurface();
    ArtifactCore::ImageF32x4RGBAWithCache first;
    ArtifactCore::ImageF32x4RGBAWithCache second;
    Artifact::FilmDamageEffect effect;
    configureDeterministicDamage(effect);
    effect.applyCPUOnly(source, first);

    setFilmProperty(effect, "Seed", QVariant(315));
    effect.applyCPUOnly(source, second);

    const float* firstPixels = first.image().rgba32fData();
    const float* secondPixels = second.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(secondPixels, nullptr);
    int changedRgbChannels = 0;
    for (std::size_t pixel = 0; pixel < source.image().totalPixels(); ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::size_t index = pixel * 4 + channel;
            changedRgbChannels += std::abs(firstPixels[index] - secondPixels[index]) >
                1.0e-6f ? 1 : 0;
        }
        EXPECT_FLOAT_EQ(firstPixels[pixel * 4 + 3],
                        secondPixels[pixel * 4 + 3]);
    }
    EXPECT_GT(changedRgbChannels, 0);
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
    ArtifactCore::ImageF32x4RGBAWithCache repeatedOutOfPlace;
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
    outOfPlaceEffect.applyCPUOnly(source, repeatedOutOfPlace);
    inPlaceEffect.applyCPUOnly(inPlaceSurface, inPlaceSurface);

    const float* original = source.image().rgba32fData();
    const float* baselinePixels = baseline.rgba32fData();
    const float* expected = outOfPlace.image().rgba32fData();
    const float* repeated = repeatedOutOfPlace.image().rgba32fData();
    const float* actual = inPlaceSurface.image().rgba32fData();
    ASSERT_NE(original, nullptr);
    ASSERT_NE(baselinePixels, nullptr);
    ASSERT_NE(expected, nullptr);
    ASSERT_NE(repeated, nullptr);
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(source.image().colorDescriptor(), descriptor);
    EXPECT_EQ(outOfPlace.image().colorDescriptor(), descriptor);
    EXPECT_EQ(repeatedOutOfPlace.image().colorDescriptor(), descriptor);
    EXPECT_EQ(inPlaceSurface.image().colorDescriptor(), descriptor);
    EXPECT_NE(actual, original);
    for (std::size_t channel = 0;
         channel < source.image().totalPixels() * 4; ++channel) {
        EXPECT_FLOAT_EQ(original[channel], baselinePixels[channel])
            << "source channel=" << channel;
        EXPECT_FLOAT_EQ(repeated[channel], expected[channel])
            << "repeat output channel=" << channel;
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

TEST(FilmDamageEffectContractTest,
     EmptyInputReplacesPreviouslyPopulatedOutput)
{
    ArtifactCore::ImageF32x4_RGBA emptyImage;
    emptyImage.setFromRGBA32F(nullptr, 0, 4);
    const ArtifactCore::ImageF32x4RGBAWithCache source(emptyImage);
    auto output = makeFilmSurface();
    ASSERT_FALSE(output.image().isEmpty());
    Artifact::FilmDamageEffect effect;

    effect.applyCPUOnly(source, output);

    EXPECT_TRUE(output.image().isEmpty());
    EXPECT_EQ(output.image().width(), source.image().width());
    EXPECT_EQ(output.image().height(), source.image().height());
    EXPECT_EQ(output.image().colorDescriptor(), source.image().colorDescriptor());
}

TEST(FilmDamageEffectContractTest,
     DegenerateSurfacesRemainFiniteAndPreserveDimensionsAndAlpha)
{
    constexpr std::array<std::array<int, 2>, 3> dimensions{{
        {{1, 1}}, {{1, 9}}, {{9, 1}}}};
    Artifact::FilmDamageEffect effect;
    setFilmProperty(effect, "Grain", QVariant(1.0));
    setFilmProperty(effect, "Dust", QVariant(1.0));
    setFilmProperty(effect, "Scratches", QVariant(1.0));
    setFilmProperty(effect, "Gate Weave", QVariant(20.0));
    setFilmProperty(effect, "Flicker", QVariant(1.0));
    setFilmProperty(effect, "Film Burn", QVariant(1.0));
    setFilmProperty(effect, "Evolution", QVariant(0.75));
    setFilmProperty(effect, "Seed", QVariant(812));

    for (const auto& size : dimensions) {
        const int width = size[0];
        const int height = size[1];
        std::array<float, 9 * 4> pixels{};
        for (int pixel = 0; pixel < width * height; ++pixel) {
            const std::size_t offset = static_cast<std::size_t>(pixel) * 4;
            pixels[offset] = 0.15f + 0.03f * static_cast<float>(pixel);
            pixels[offset + 1] = 0.3f + 0.02f * static_cast<float>(pixel);
            pixels[offset + 2] = 0.65f - 0.01f * static_cast<float>(pixel);
            pixels[offset + 3] = 1.25f;
        }
        ArtifactCore::ImageF32x4_RGBA image;
        image.setFromRGBA32F(pixels.data(), width, height);
        const ArtifactCore::ImageF32x4RGBAWithCache source(image);
        auto baseline = source.image().DeepCopy();
        ArtifactCore::ImageF32x4RGBAWithCache output;

        effect.applyCPUOnly(source, output);

        ASSERT_EQ(output.image().width(), width);
        ASSERT_EQ(output.image().height(), height);
        EXPECT_EQ(output.image().colorDescriptor(), source.image().colorDescriptor());
        const float* actual = output.image().rgba32fData();
        const float* original = source.image().rgba32fData();
        const float* baselinePixels = baseline.rgba32fData();
        ASSERT_NE(actual, nullptr);
        ASSERT_NE(original, nullptr);
        ASSERT_NE(baselinePixels, nullptr);
        for (std::size_t pixel = 0; pixel < source.image().totalPixels(); ++pixel) {
            for (std::size_t channel = 0; channel < 3; ++channel) {
                EXPECT_TRUE(std::isfinite(actual[pixel * 4 + channel]))
                    << "size=" << width << "x" << height
                    << " pixel=" << pixel << " channel=" << channel;
            }
            EXPECT_FLOAT_EQ(actual[pixel * 4 + 3], 1.25f)
                << "size=" << width << "x" << height << " pixel=" << pixel;
        }
        for (std::size_t channel = 0;
             channel < source.image().totalPixels() * 4; ++channel) {
            EXPECT_FLOAT_EQ(original[channel], baselinePixels[channel])
                << "size=" << width << "x" << height
                << " source channel=" << channel;
        }
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

TEST(FilmDamageEffectContractTest,
     DustAddsLocalizedRgbDamageAndPreservesAlpha)
{
    const auto source = makeFilmSurface();
    ArtifactCore::ImageF32x4RGBAWithCache cleanOutput;
    ArtifactCore::ImageF32x4RGBAWithCache dustyOutput;
    Artifact::FilmDamageEffect cleanEffect;
    Artifact::FilmDamageEffect dustyEffect;
    for (auto* effect : {&cleanEffect, &dustyEffect}) {
        setFilmProperty(*effect, "Grain", QVariant(0.0));
        setFilmProperty(*effect, "Dust", QVariant(0.0));
        setFilmProperty(*effect, "Scratches", QVariant(0.0));
        setFilmProperty(*effect, "Gate Weave", QVariant(0.0));
        setFilmProperty(*effect, "Flicker", QVariant(0.0));
        setFilmProperty(*effect, "Film Burn", QVariant(0.0));
        setFilmProperty(*effect, "Evolution", QVariant(0.125));
        setFilmProperty(*effect, "Seed", QVariant(943));
    }
    setFilmProperty(dustyEffect, "Dust", QVariant(1.0));

    cleanEffect.applyCPUOnly(source, cleanOutput);
    dustyEffect.applyCPUOnly(source, dustyOutput);

    int changedPixels = 0;
    int unchangedPixels = 0;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto clean = cleanOutput.image().getPixel(x, y);
            const auto dusty = dustyOutput.image().getPixel(x, y);
            const auto input = source.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(clean.r(), input.r());
            EXPECT_FLOAT_EQ(clean.g(), input.g());
            EXPECT_FLOAT_EQ(clean.b(), input.b());
            EXPECT_FLOAT_EQ(dusty.a(), input.a());
            const bool changed =
                std::abs(dusty.r() - input.r()) > 1.0e-6f ||
                std::abs(dusty.g() - input.g()) > 1.0e-6f ||
                std::abs(dusty.b() - input.b()) > 1.0e-6f;
            changed ? ++changedPixels : ++unchangedPixels;
        }
    }
    EXPECT_GT(changedPixels, 0);
    EXPECT_GT(unchangedPixels, 0);
}

TEST(FilmDamageEffectContractTest,
     FilmBurnAddsWarmRgbAndPreservesAlpha)
{
    const auto source = makeFilmSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::FilmDamageEffect effect;
    setFilmProperty(effect, "Grain", QVariant(0.0));
    setFilmProperty(effect, "Dust", QVariant(0.0));
    setFilmProperty(effect, "Scratches", QVariant(0.0));
    setFilmProperty(effect, "Gate Weave", QVariant(0.0));
    setFilmProperty(effect, "Flicker", QVariant(0.0));
    setFilmProperty(effect, "Film Burn", QVariant(1.0));
    setFilmProperty(effect, "Evolution", QVariant(0.125));
    setFilmProperty(effect, "Seed", QVariant(47));

    effect.applyCPUOnly(source, output);

    int burnPixels = 0;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto input = source.image().getPixel(x, y);
            const float redDelta = actual.r() - input.r();
            const float greenDelta = actual.g() - input.g();
            const float blueDelta = actual.b() - input.b();
            EXPECT_TRUE(std::isfinite(actual.r()));
            EXPECT_TRUE(std::isfinite(actual.g()));
            EXPECT_TRUE(std::isfinite(actual.b()));
            EXPECT_FLOAT_EQ(actual.a(), input.a());
            EXPECT_GE(blueDelta, -1.0e-6f);
            EXPECT_GE(greenDelta + 1.0e-6f, blueDelta);
            EXPECT_GE(redDelta + 1.0e-6f, greenDelta);
            burnPixels += redDelta > 1.0e-5f ? 1 : 0;
        }
    }
    EXPECT_GT(burnPixels, 0);
}

TEST(FilmDamageEffectContractTest,
     ScratchesCreateVerticalDamageAndPreserveAlpha)
{
    const auto source = makeFilmSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::FilmDamageEffect effect;
    setFilmProperty(effect, "Grain", QVariant(0.0));
    setFilmProperty(effect, "Dust", QVariant(0.0));
    setFilmProperty(effect, "Scratches", QVariant(1.0));
    setFilmProperty(effect, "Gate Weave", QVariant(0.0));
    setFilmProperty(effect, "Flicker", QVariant(0.0));
    setFilmProperty(effect, "Film Burn", QVariant(0.0));
    setFilmProperty(effect, "Evolution", QVariant(0.125));
    setFilmProperty(effect, "Seed", QVariant(552));

    effect.applyCPUOnly(source, output);

    int changedPixels = 0;
    int changedRows = 0;
    for (int y = 0; y < kHeight; ++y) {
        bool rowChanged = false;
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto input = source.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.a(), input.a());
            const bool changed =
                std::abs(actual.r() - input.r()) > 1.0e-6f ||
                std::abs(actual.g() - input.g()) > 1.0e-6f ||
                std::abs(actual.b() - input.b()) > 1.0e-6f;
            if (changed) {
                ++changedPixels;
                rowChanged = true;
            }
        }
        changedRows += rowChanged ? 1 : 0;
    }
    EXPECT_GT(changedPixels, 0);
    EXPECT_GT(changedRows, 1);
}

TEST(FilmDamageEffectContractTest,
     GateWeaveDisplacesColorAndAlphaTogether)
{
    const auto source = makeFilmSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::FilmDamageEffect effect;
    setFilmProperty(effect, "Grain", QVariant(0.0));
    setFilmProperty(effect, "Dust", QVariant(0.0));
    setFilmProperty(effect, "Scratches", QVariant(0.0));
    setFilmProperty(effect, "Gate Weave", QVariant(20.0));
    setFilmProperty(effect, "Flicker", QVariant(0.0));
    setFilmProperty(effect, "Film Burn", QVariant(0.0));
    setFilmProperty(effect, "Evolution", QVariant(0.125));
    setFilmProperty(effect, "Seed", QVariant(624));

    effect.applyCPUOnly(source, output);

    int changedColorPixels = 0;
    int changedAlphaPixels = 0;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto input = source.image().getPixel(x, y);
            EXPECT_TRUE(std::isfinite(actual.a()));
            changedAlphaPixels += std::abs(actual.a() - input.a()) > 1.0e-6f
                ? 1 : 0;
            const bool changed =
                std::abs(actual.r() - input.r()) > 1.0e-6f ||
                std::abs(actual.g() - input.g()) > 1.0e-6f ||
                std::abs(actual.b() - input.b()) > 1.0e-6f;
            changedColorPixels += changed ? 1 : 0;
        }
    }
    EXPECT_GT(changedColorPixels, 0);
    EXPECT_GT(changedAlphaPixels, 0);
}

TEST(FilmDamageEffectContractTest,
     FlickerAppliesOneFrameWideRgbGainAndPreservesAlpha)
{
    const auto source = makeFilmSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    ArtifactCore::ImageF32x4RGBAWithCache sameFrameKeyOutput;
    ArtifactCore::ImageF32x4RGBAWithCache nextFrameKeyOutput;
    Artifact::FilmDamageEffect effect;
    setFilmProperty(effect, "Grain", QVariant(0.0));
    setFilmProperty(effect, "Dust", QVariant(0.0));
    setFilmProperty(effect, "Scratches", QVariant(0.0));
    setFilmProperty(effect, "Gate Weave", QVariant(0.0));
    setFilmProperty(effect, "Flicker", QVariant(1.0));
    setFilmProperty(effect, "Film Burn", QVariant(0.0));
    setFilmProperty(effect, "Evolution", QVariant(0.125));
    setFilmProperty(effect, "Seed", QVariant(212));

    effect.applyCPUOnly(source, output);
    setFilmProperty(effect, "Evolution", QVariant(0.13));
    effect.applyCPUOnly(source, sameFrameKeyOutput);
    setFilmProperty(effect, "Evolution", QVariant(0.17));
    effect.applyCPUOnly(source, nextFrameKeyOutput);

    float expectedGain = 0.0f;
    float expectedNextFrameGain = 0.0f;
    bool hasExpectedGain = false;
    bool hasExpectedNextFrameGain = false;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto sameFrameKey = sameFrameKeyOutput.image().getPixel(x, y);
            const auto nextFrameKey = nextFrameKeyOutput.image().getPixel(x, y);
            const auto input = source.image().getPixel(x, y);
            const std::array<float, 3> inputRgb{input.r(), input.g(), input.b()};
            const std::array<float, 3> outputRgb{actual.r(), actual.g(), actual.b()};
            EXPECT_FLOAT_EQ(actual.a(), input.a());
            EXPECT_FLOAT_EQ(sameFrameKey.a(), input.a());
            EXPECT_FLOAT_EQ(nextFrameKey.a(), input.a());
            for (std::size_t channel = 0; channel < inputRgb.size(); ++channel) {
                const float gain = outputRgb[channel] / inputRgb[channel];
                if (!hasExpectedGain) {
                    expectedGain = gain;
                    hasExpectedGain = true;
                }
                EXPECT_NEAR(gain, expectedGain, 1.0e-6f)
                    << "pixel=" << y * kWidth + x
                    << " channel=" << channel;
                EXPECT_FLOAT_EQ(outputRgb[channel],
                                channel == 0 ? sameFrameKey.r() :
                                channel == 1 ? sameFrameKey.g() : sameFrameKey.b())
                    << "same frameKey pixel=" << y * kWidth + x
                    << " channel=" << channel;

                const float nextGain =
                    (channel == 0 ? nextFrameKey.r() :
                     channel == 1 ? nextFrameKey.g() : nextFrameKey.b()) /
                    inputRgb[channel];
                if (!hasExpectedNextFrameGain) {
                    expectedNextFrameGain = nextGain;
                    hasExpectedNextFrameGain = true;
                }
                EXPECT_NEAR(nextGain, expectedNextFrameGain, 1.0e-6f)
                    << "next frameKey pixel=" << y * kWidth + x
                    << " channel=" << channel;
            }
        }
    }
    EXPECT_TRUE(hasExpectedGain);
    EXPECT_GT(std::abs(expectedGain - 1.0f), 1.0e-4f);
    EXPECT_TRUE(hasExpectedNextFrameGain);
    EXPECT_GT(std::abs(expectedNextFrameGain - expectedGain), 1.0e-4f);
}
