#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Distort.ImageMorph;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 7;
constexpr int kHeight = 5;

ImageF32x4RGBAWithCache makePatternedImage()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4u;
            pixels[offset] = static_cast<float>(x) / static_cast<float>(kWidth);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(kHeight);
            pixels[offset + 2] = 0.35f;
            pixels[offset + 3] = 0.25f + 0.1f * static_cast<float>(x);
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(ImageMorphEffectContractTest, MissingTargetImageLeavesInputUnchanged)
{
    const auto source = makePatternedImage();
    ImageF32x4RGBAWithCache output;
    ImageMorphEffect effect;
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
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

TEST(ImageMorphEffectContractTest, UnreadableTargetFileLeavesInputUnchanged)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString targetPath = temporaryDirectory.filePath(QStringLiteral("empty-target.png"));
    QFile targetFile(targetPath);
    ASSERT_TRUE(targetFile.open(QIODevice::WriteOnly));
    targetFile.close();

    const auto source = makePatternedImage();
    ImageF32x4RGBAWithCache output;
    ImageMorphEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Target Image Path")), targetPath);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* actual = output.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(original, nullptr);
    for (std::size_t channel = 0; channel <
         static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], original[channel])
            << "channel=" << channel;
    }
}
