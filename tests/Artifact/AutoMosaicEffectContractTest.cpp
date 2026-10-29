#include <gtest/gtest.h>

#include <limits>

#include <QColor>
#include <QImage>
#include <QRect>
#include <QVector>

import Artifact.Effect.AutoMosaic;

namespace {

QImage makePatternImage()
{
    QImage image(8, 8, QImage::Format_RGBA8888);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixelColor(x, y, QColor((x * 31) % 256,
                                             (y * 37) % 256,
                                             ((x + y) * 19) % 256,
                                             255));
        }
    }
    return image;
}

} // namespace

TEST(AutoMosaicEffectContractTest, EmptyOrFullyOutsideRegionsPreserveImage)
{
    Artifact::AutoMosaicEffect effect;
    const QImage input = makePatternImage();

    EXPECT_EQ(effect.applyMosaic(input, {}), input);
    const QImage outside = effect.applyMosaic(
        input, QVector<QRect>{QRect(20, 20, 3, 3), QRect()});
    EXPECT_EQ(outside, input);
}

TEST(AutoMosaicEffectContractTest, CustomRegionsAreClippedToImageBounds)
{
    Artifact::AutoMosaicEffect effect;
    effect.setMosaicStrength(4);
    const QImage input = makePatternImage();
    const QImage output = effect.applyMosaic(
        input, QVector<QRect>{QRect(-3, -2, 7, 6)});

    ASSERT_EQ(output.size(), input.size());
    EXPECT_EQ(output.pixelColor(7, 7), input.pixelColor(7, 7));
    EXPECT_NE(output, input);
}

TEST(AutoMosaicEffectContractTest, MosaicStrengthAndFeatherRejectInvalidLowerValues)
{
    Artifact::AutoMosaicEffect effect;
    effect.setMosaicStrength(0);
    EXPECT_EQ(effect.mosaicStrength(), 1);
    effect.setFeather(-4.0f);
    EXPECT_FLOAT_EQ(effect.feather(), 0.0f);
    effect.setFeather(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.feather(), 0.0f);
}
