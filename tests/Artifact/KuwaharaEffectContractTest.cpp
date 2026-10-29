#include <gtest/gtest.h>

#include <array>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Kuwahara;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using namespace ArtifactCore;

constexpr int kWidth = 5;
constexpr int kHeight = 4;
constexpr std::array<float, 4> kColor{0.2f, 0.45f, 0.7f, 0.35f};

ImageF32x4RGBAWithCache makeConstantImage()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixels.size() / 4; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel)
            pixels[pixel * 4 + channel] = kColor[channel];
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(KuwaharaEffectContractTest, ConstantSurfaceKeepsEveryChannelAndDimensions)
{
    const auto source = makeConstantImage();
    ImageF32x4RGBAWithCache output;
    Artifact::KuwaharaEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setRadius(3.0f);
    effect.setSharpness(0.4f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* pixels = output.image().rgba32fData();
    ASSERT_NE(pixels, nullptr);
    for (std::size_t pixel = 0; pixel < kWidth * kHeight; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            EXPECT_NEAR(pixels[pixel * 4 + channel], kColor[channel], 1.0e-6f)
                << "pixel=" << pixel << " channel=" << channel;
        }
    }
}

TEST(KuwaharaEffectContractTest, SettersClampParametersAndResetNonFiniteValues)
{
    Artifact::KuwaharaEffect effect;

    effect.setRadius(-1.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 1.0f);
    effect.setRadius(80.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 50.0f);
    effect.setRadius(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.radius(), 5.0f);

    effect.setSharpness(-0.5f);
    EXPECT_FLOAT_EQ(effect.sharpness(), 0.0f);
    effect.setSharpness(2.0f);
    EXPECT_FLOAT_EQ(effect.sharpness(), 1.0f);
    effect.setSharpness(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.sharpness(), 0.5f);
}
