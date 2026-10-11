#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>
#include <QString>

#include <cmath>
#include <vector>

import Image.OpenEXR;

using ArtifactCore::OpenExr;

TEST(OpenExrRoundTripTest, WritesAndReadsFloatRgbaPixels)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString path = QDir(temporaryDirectory.path()).filePath(QStringLiteral("roundtrip.exr"));
    const std::vector<float> expected{
        0.125f, 0.25f, 0.5f, 1.0f,
        2.0f, 4.0f, 8.0f, 0.75f,
    };

    OpenExr exr;
    ASSERT_TRUE(exr.writeRGBA32F(path, expected.data(), 2, 1, QStringLiteral("zip")));

    std::vector<float> actual;
    int width = 0;
    int height = 0;
    ASSERT_TRUE(exr.readRGBA32F(path, actual, width, height));

    EXPECT_EQ(width, 2);
    EXPECT_EQ(height, 1);
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
        EXPECT_NEAR(actual[index], expected[index], 1.0e-6f) << "component " << index;
}

TEST(OpenExrRoundTripTest, RejectsInvalidWriteInputs)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    OpenExr exr;
    const QString path = QDir(temporaryDirectory.path()).filePath(QStringLiteral("invalid.exr"));
    const float pixel[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const float nonFinitePixel[] = {0.0f, 0.0f, std::nanf(""), 1.0f};

    EXPECT_FALSE(exr.writeRGBA32F(path, nullptr, 1, 1));
    EXPECT_FALSE(exr.writeRGBA32F(path, pixel, 0, 1));
    EXPECT_FALSE(exr.writeRGBA32F(path, nonFinitePixel, 1, 1));
    EXPECT_FALSE(exr.writeRGBA32F(QString(), pixel, 1, 1));
}
