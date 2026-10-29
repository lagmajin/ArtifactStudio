#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.SurfaceFX;
import Graphics.Effect.SurfaceFX;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 32;
constexpr int kHeight = 24;

ImageF32x4RGBAWithCache makeNeutralSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = 0.2f;
        pixels[i + 1] = 0.25f;
        pixels[i + 2] = 0.3f;
        pixels[i + 3] = 0.5f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(SurfaceFXEffectContractTest, DrawsConfiguredSurfaceMarkAndPreservesAlpha)
{
    const auto source = makeNeutralSurface();
    ImageF32x4RGBAWithCache output;
    SurfaceFXEffect effect;
    SurfaceFXData data;
    SurfaceFXElement scratch;
    scratch.type = SurfaceFXElementType::Scratch;
    scratch.x = 0.2f;
    scratch.y = 0.3f;
    scratch.width = 0.6f;
    scratch.height = 0.15f;
    scratch.opacity = 1.0f;
    scratch.intensity = 1.0f;
    data.elements.push_back(scratch);
    effect.setData(data);

    effect.applyCPUOnly(source, output);

    bool changedRgb = false;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            changedRgb = changedRgb || after.r() != before.r() ||
                         after.g() != before.g() || after.b() != before.b();
            EXPECT_NEAR(after.a(), before.a(), 1.0e-6f);
        }
    }
    EXPECT_TRUE(changedRgb);
    EXPECT_NEAR(source.image().getPixel(16, 12).r(), 0.2f, 1.0e-6f);
}

TEST(SurfaceFXEffectContractTest, SkipsElementBeforeItsInTime)
{
    const auto source = makeNeutralSurface();
    ImageF32x4RGBAWithCache output;
    SurfaceFXEffect effect;
    SurfaceFXData data;
    SurfaceFXElement futureScratch;
    futureScratch.type = SurfaceFXElementType::Scratch;
    futureScratch.x = 0.2f;
    futureScratch.y = 0.3f;
    futureScratch.width = 0.6f;
    futureScratch.height = 0.15f;
    futureScratch.inTime = 1.0f;
    data.elements.push_back(futureScratch);
    effect.setData(data);
    EffectContext context;
    context.timeSeconds = 0.0;
    effect.setContext(context);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(after.r(), before.r());
            EXPECT_FLOAT_EQ(after.g(), before.g());
            EXPECT_FLOAT_EQ(after.b(), before.b());
            EXPECT_FLOAT_EQ(after.a(), before.a());
        }
    }
}

TEST(SurfaceFXEffectContractTest, IncludesOutTimeBoundaryAndSkipsImmediatelyAfter)
{
    const auto source = makeNeutralSurface();
    ImageF32x4RGBAWithCache output;
    SurfaceFXEffect effect;
    SurfaceFXData data;
    SurfaceFXElement scratch;
    scratch.type = SurfaceFXElementType::Scratch;
    scratch.x = 0.2f;
    scratch.y = 0.3f;
    scratch.width = 0.6f;
    scratch.height = 0.15f;
    scratch.outTime = 0.0f;
    data.elements.push_back(scratch);
    effect.setData(data);
    EffectContext context;
    context.timeSeconds = 0.0;
    effect.setContext(context);

    effect.applyCPUOnly(source, output);

    bool changedAtBoundary = false;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto atBoundary = output.image().getPixel(x, y);
            changedAtBoundary = changedAtBoundary || atBoundary.r() != before.r() ||
                atBoundary.g() != before.g() || atBoundary.b() != before.b();
        }
    }
    EXPECT_TRUE(changedAtBoundary);

    context.timeSeconds = 0.001;
    effect.setContext(context);
    effect.applyCPUOnly(source, output);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(after.r(), before.r());
            EXPECT_FLOAT_EQ(after.g(), before.g());
            EXPECT_FLOAT_EQ(after.b(), before.b());
            EXPECT_FLOAT_EQ(after.a(), before.a());
        }
    }
}
