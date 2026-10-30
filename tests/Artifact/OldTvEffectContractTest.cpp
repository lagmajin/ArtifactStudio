#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Creative;
import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 8;

ArtifactCore::ImageF32x4RGBAWithCache makePatternSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * kWidth + x) * 4);
            const float value = (x + y) % 2 == 0 ? 0.2f : 0.8f;
            pixels[offset + 0] = value;
            pixels[offset + 1] = 1.0f - value;
            pixels[offset + 2] = static_cast<float>(x + y) / 14.0f;
            pixels[offset + 3] = static_cast<float>((x + y) % 5) * 0.25f;
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(OldTvEffectContractTest, OutputIsStableAcrossCompositionFrames)
{
    const auto source = makePatternSurface();
    ArtifactCore::ImageF32x4RGBAWithCache frameZero;
    ArtifactCore::ImageF32x4RGBAWithCache laterFrame;
    Artifact::ArtifactOldTVEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);

    Artifact::EffectContext context;
    context.compositionFrame = 0;
    effect.setContext(context);
    effect.applyCPUOnly(source, frameZero);
    context.compositionFrame = 17;
    effect.setContext(context);
    effect.applyCPUOnly(source, laterFrame);

    const float* first = frameZero.image().rgba32fData();
    const float* later = laterFrame.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(later, nullptr);
    ASSERT_NE(original, nullptr);
    bool changedRgb = false;
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(kWidth * kHeight); ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(first[pixel * 4 + channel]))
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(first[pixel * 4 + channel],
                            later[pixel * 4 + channel])
                << "pixel=" << pixel << " channel=" << channel;
            changedRgb = changedRgb ||
                first[pixel * 4 + channel] != original[pixel * 4 + channel];
        }
        EXPECT_FLOAT_EQ(first[pixel * 4 + 3], original[pixel * 4 + 3])
            << "frame zero alpha pixel=" << pixel;
        EXPECT_FLOAT_EQ(later[pixel * 4 + 3], original[pixel * 4 + 3])
            << "later frame alpha pixel=" << pixel;
    }
    EXPECT_TRUE(changedRgb);
}
