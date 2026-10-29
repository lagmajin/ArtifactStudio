#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.AnisotropicFlowBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

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

TEST(AnisotropicFlowBlurEffectContractTest, ZeroBlurAmountPreservesRgba)
{
    std::array<float, 36> pixels{};
    for (int pixel = 0; pixel < 9; ++pixel) {
        pixels[pixel * 4] = 0.1f + 0.1f * static_cast<float>(pixel);
        pixels[pixel * 4 + 1] = 0.8f - 0.05f * static_cast<float>(pixel);
        pixels[pixel * 4 + 2] = 0.2f + 0.07f * static_cast<float>(pixel);
        pixels[pixel * 4 + 3] = 0.25f + 0.05f * static_cast<float>(pixel);
    }
    const auto source = makeSurface(pixels.data(), 3, 3);
    ImageF32x4RGBAWithCache output;
    AnisotropicFlowBlurEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Blur Amount")), QVariant(0.0));

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 3);
    ASSERT_EQ(output.image().height(), 3);
    const float* outputPixels = output.image().rgba32fData();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    ASSERT_NE(sourcePixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_NEAR(outputPixels[channel], pixels[channel], 1.0e-6f)
            << "output channel=" << channel;
        EXPECT_FLOAT_EQ(sourcePixels[channel], pixels[channel])
            << "source channel=" << channel;
    }
}


TEST(AnisotropicFlowBlurEffectContractTest,
     PropertyUpdatesClampValuesAndRestoreNonFiniteDefaults)
{
    AnisotropicFlowBlurEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Blur Amount")), QVariant(150.0));
    effect.setPropertyValue(UniString(QStringLiteral("Tensor Noise Scale")), QVariant(-1.0));
    effect.setPropertyValue(UniString(QStringLiteral("Tensor Integration Scale")),
                            QVariant(30.0));
    effect.setPropertyValue(UniString(QStringLiteral("Edge Adherence")), QVariant(2.0));

    auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 4u);
    EXPECT_FLOAT_EQ(properties[0].getValue().toFloat(), 100.0f);
    EXPECT_FLOAT_EQ(properties[1].getValue().toFloat(), 0.1f);
    EXPECT_FLOAT_EQ(properties[2].getValue().toFloat(), 20.0f);
    EXPECT_FLOAT_EQ(properties[3].getValue().toFloat(), 1.0f);

    effect.setPropertyValue(UniString(QStringLiteral("Blur Amount")),
                            QVariant(std::numeric_limits<float>::infinity()));
    effect.setPropertyValue(UniString(QStringLiteral("Tensor Noise Scale")),
                            QVariant(std::numeric_limits<float>::quiet_NaN()));
    effect.setPropertyValue(UniString(QStringLiteral("Tensor Integration Scale")),
                            QVariant(std::numeric_limits<float>::infinity()));
    effect.setPropertyValue(UniString(QStringLiteral("Edge Adherence")),
                            QVariant(std::numeric_limits<float>::quiet_NaN()));
    properties = effect.getProperties();
    EXPECT_FLOAT_EQ(properties[0].getValue().toFloat(), 10.0f);
    EXPECT_FLOAT_EQ(properties[1].getValue().toFloat(), 1.0f);
    EXPECT_FLOAT_EQ(properties[2].getValue().toFloat(), 4.0f);
    EXPECT_FLOAT_EQ(properties[3].getValue().toFloat(), 0.8f);
}
