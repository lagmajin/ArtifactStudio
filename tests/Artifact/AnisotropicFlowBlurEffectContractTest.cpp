#include <gtest/gtest.h>

#include <array>
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
        pixels[pixel * 4 + 3] = 0.25f + static_cast<float>(pixel) / 12.0f;
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

TEST(AnisotropicFlowBlurEffectContractTest, DefaultBlurAmountPreservesPixels)
{
    const Pixels pixels = makePattern();
    Artifact::AnisotropicFlowBlurEffect effect;
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
