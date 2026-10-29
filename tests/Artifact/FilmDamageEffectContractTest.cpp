#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.FilmDamage;
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
