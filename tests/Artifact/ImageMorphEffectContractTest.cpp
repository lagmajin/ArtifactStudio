#include <gtest/gtest.h>

#include <QTemporaryDir>
#include <QString>
#include <QVariant>

#include <array>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Distort.ImageMorph;
import FloatRGBA;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

namespace {

using namespace ArtifactCore;

constexpr int kWidth = 3;
constexpr int kHeight = 2;
constexpr std::array<float, kWidth * kHeight * 4> kPixels{
    0.1f, 0.2f, 0.3f, 0.4f,
    0.2f, 0.3f, 0.4f, 0.5f,
    0.3f, 0.4f, 0.5f, 0.6f,
    0.4f, 0.5f, 0.6f, 0.7f,
    0.5f, 0.6f, 0.7f, 0.8f,
    0.6f, 0.7f, 0.8f, 0.9f,
};

ImageF32x4RGBAWithCache makeSource()
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(kPixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(ImageMorphEffectContractTest, MissingTargetImagePassesSourceThrough)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString missingTarget =
        temporaryDirectory.filePath(QStringLiteral("missing-target.png"));
    const auto source = makeSource();
    ImageF32x4RGBAWithCache output;
    Artifact::ImageMorphEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Target Image Path"),
                            QVariant(missingTarget));
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* pixels = output.image().rgba32fData();
    ASSERT_NE(pixels, nullptr);
    for (std::size_t channel = 0; channel < kPixels.size(); ++channel)
        EXPECT_FLOAT_EQ(pixels[channel], kPixels[channel]) << "channel=" << channel;
}

TEST(ImageMorphEffectContractTest, FullAmountWithoutControlPointsUsesTargetPixels)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString targetPath =
        temporaryDirectory.filePath(QStringLiteral("target.exr"));
    constexpr std::array<float, kWidth * kHeight * 4> targetPixels{
        0.8f, 0.1f, 0.3f, 0.6f,
        0.8f, 0.1f, 0.3f, 0.6f,
        0.8f, 0.1f, 0.3f, 0.6f,
        0.8f, 0.1f, 0.3f, 0.6f,
        0.8f, 0.1f, 0.3f, 0.6f,
        0.8f, 0.1f, 0.3f, 0.6f,
    };
    ImageF32x4_RGBA target;
    target.setFromRGBA32F(targetPixels.data(), kWidth, kHeight);
    ASSERT_TRUE(target.save(targetPath));

    Artifact::ImageMorphEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Target Image Path"),
                            QVariant(targetPath));
    effect.setPropertyValue(ArtifactCore::UniString("Amount"), QVariant(1.0));
    effect.setPropertyValue(ArtifactCore::UniString("Control Points JSON"),
                            QVariant(QStringLiteral("[]")));
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(makeSource(), output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const FloatRGBA pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), 0.8f, 1.0e-6f);
            EXPECT_NEAR(pixel.g(), 0.1f, 1.0e-6f);
            EXPECT_NEAR(pixel.b(), 0.3f, 1.0e-6f);
            EXPECT_NEAR(pixel.a(), 0.6f, 1.0e-6f);
        }
    }
}

TEST(ImageMorphEffectContractTest, AmountPropertyClampsAndResetsNonFiniteValues)
{
    Artifact::ImageMorphEffect effect;

    effect.setPropertyValue(ArtifactCore::UniString("Amount"), QVariant(2.0));
    auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 3u);
    EXPECT_FLOAT_EQ(properties[1].getValue().toFloat(), 1.0f);

    effect.setPropertyValue(
        ArtifactCore::UniString("Amount"),
        QVariant(static_cast<double>(std::numeric_limits<float>::infinity())));
    properties = effect.getProperties();
    EXPECT_FLOAT_EQ(properties[1].getValue().toFloat(), 0.0f);
}
