#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.AnisotropicFlowBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

namespace {

using Pixels = std::array<float, 36>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 3, 3);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

Pixels makePattern()
{
    Pixels pixels{};
    for (std::size_t pixel = 0; pixel < 9; ++pixel) {
        const float value = static_cast<float>((pixel * 7) % 9) / 8.0f;
        pixels[pixel * 4] = value;
        pixels[pixel * 4 + 1] = 1.0f - value;
        pixels[pixel * 4 + 2] = static_cast<float>(pixel) / 8.0f;
        pixels[pixel * 4 + 3] = 0.65f;
    }
    return pixels;
}

void expectPixelsEqual(const ArtifactCore::ImageF32x4RGBAWithCache& image,
                       const Pixels& expected)
{
    const float* actual = image.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < expected.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel]) << "channel=" << channel;
    }
}

} // namespace

TEST(AnisotropicFlowBlurEffectContractTest,
     DefaultBlurChangesRgbAndPreservesUniformAlphaAndSource)
{
    const Pixels pixels = makePattern();
    const auto source = makeImage(pixels);
    Artifact::AnisotropicFlowBlurEffect effect;
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 3);
    ASSERT_EQ(output.image().height(), 3);
    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    bool rgbChanged = false;
    for (std::size_t pixel = 0; pixel < pixels.size() / 4; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(actual[pixel * 4 + channel]));
            rgbChanged = rgbChanged ||
                std::abs(actual[pixel * 4 + channel] -
                         pixels[pixel * 4 + channel]) > 1.0e-5f;
        }
        EXPECT_NEAR(actual[pixel * 4 + 3], 0.65f, 1.0e-6f);
    }
    EXPECT_TRUE(rgbChanged);
    expectPixelsEqual(source, pixels);
}

TEST(AnisotropicFlowBlurEffectContractTest, ZeroBlurAmountPreservesEveryPixel)
{
    const Pixels pixels = makePattern();
    Artifact::AnisotropicFlowBlurEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Blur Amount"), QVariant(0.0));
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    expectPixelsEqual(output, pixels);
}

TEST(AnisotropicFlowBlurEffectContractTest, PropertySetterClampsAndRecoversNonFiniteValues)
{
    Artifact::AnisotropicFlowBlurEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Blur Amount"), QVariant(-4.0));
    EXPECT_FLOAT_EQ(effect.getProperties()[0].getValue().toFloat(), 0.0f);
    effect.setPropertyValue(ArtifactCore::UniString("Blur Amount"), QVariant(101.0));
    EXPECT_FLOAT_EQ(effect.getProperties()[0].getValue().toFloat(), 100.0f);
    effect.setPropertyValue(ArtifactCore::UniString("Blur Amount"),
                            QVariant(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FLOAT_EQ(effect.getProperties()[0].getValue().toFloat(), 10.0f);

    effect.setPropertyValue(ArtifactCore::UniString("Edge Adherence"), QVariant(2.0));
    EXPECT_FLOAT_EQ(effect.getProperties()[3].getValue().toFloat(), 1.0f);
}
