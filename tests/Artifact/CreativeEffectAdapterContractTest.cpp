#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Creative;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

TEST(CreativeEffectAdapterContractTest,
     CoreMirrorMapsRgbaChannelsAndForwardsParameters)
{
    constexpr int width = 4;
    constexpr int height = 1;
    constexpr std::array<float, width * height * 4> pixels{
        0.1f, 0.9f, 0.2f, 0.0f,
        0.3f, 0.7f, 0.4f, 0.2f,
        0.6f, 0.4f, 0.6f, 0.4f,
        0.9f, 0.1f, 0.8f, 0.6f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::ArtifactCoreCreativeEffect effect(
        "Mirror", "builtin.test_mirror", "Mirror Contract");
    effect.setPropertyValue(ArtifactCore::UniString(QStringLiteral("Angle")),
                            QVariant(0.0));
    effect.setPropertyValue(ArtifactCore::UniString(QStringLiteral("CenterX")),
                            QVariant(0.5));
    effect.setPropertyValue(ArtifactCore::UniString(QStringLiteral("CenterY")),
                            QVariant(0.5));

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* actual = output.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(original, nullptr);
    constexpr std::array<int, width> expectedSources{0, 1, 2, 1};
    for (int x = 0; x < width; ++x) {
        const std::size_t outputOffset = static_cast<std::size_t>(x) * 4;
        const std::size_t sourceOffset =
            static_cast<std::size_t>(expectedSources[static_cast<std::size_t>(x)]) * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(actual[outputOffset + channel],
                            pixels[sourceOffset + channel])
                << "x=" << x << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(actual[outputOffset + 3], pixels[outputOffset + 3])
            << "alpha x=" << x;
    }
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(original[channel], pixels[channel])
            << "source channel=" << channel;
    }
}
