#include <gtest/gtest.h>

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QString>
#include <QTemporaryDir>

import Color.Grading.LUTLoader;

namespace {

using ArtifactCore::LUT3D;

const QByteArray kIdentityLut =
    "LUT_3D_SIZE 2\n"
    "0 0 0\n"
    "0 0 1\n"
    "0 1 0\n"
    "0 1 1\n"
    "1 0 0\n"
    "1 0 1\n"
    "1 1 0\n"
    "1 1 1\n";

const QByteArray kInvertLut =
    "TITLE \"Invert\"\n"
    "LUT_3D_SIZE 2\n"
    "1 1 1\n"
    "1 1 0\n"
    "1 0 1\n"
    "1 0 0\n"
    "0 1 1\n"
    "0 1 0\n"
    "0 0 1\n"
    "0 0 0\n";

auto writeTextFile(const QString& path, const QByteArray& contents) -> bool
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

} // namespace

TEST(LUTLoaderContractTest, LoadsCubeAndAppliesTrilinearIdentityWithoutChangingAlpha)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("identity.cube"));
    ASSERT_TRUE(writeTextFile(path, kIdentityLut));

    LUT3D lut;
    ASSERT_TRUE(lut.loadFromFile(path));
    EXPECT_TRUE(lut.isValid());
    EXPECT_EQ(lut.size(), 2);

    QImage input(2, 1, QImage::Format_RGBA8888);
    input.setPixelColor(0, 0, QColor(0, 0, 0, 19));
    input.setPixelColor(1, 0, QColor(255, 128, 64, 73));
    const QImage output = lut.apply(input);
    ASSERT_EQ(output.size(), input.size());
    EXPECT_EQ(output.format(), QImage::Format_RGBA8888);
    EXPECT_EQ(output.pixelColor(0, 0), QColor(0, 0, 0, 19));

    const QColor interpolated = output.pixelColor(1, 0);
    EXPECT_NEAR(interpolated.red(), 255, 1);
    EXPECT_NEAR(interpolated.green(), 128, 1);
    EXPECT_NEAR(interpolated.blue(), 64, 1);
    EXPECT_EQ(interpolated.alpha(), 73);
}

TEST(LUTLoaderContractTest, ReloadResetsDataCursorAndOptionalTitle)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString invertPath = directory.filePath(QStringLiteral("invert.cube"));
    const QString identityPath = directory.filePath(QStringLiteral("identity.cube"));
    ASSERT_TRUE(writeTextFile(invertPath, kInvertLut));
    ASSERT_TRUE(writeTextFile(identityPath, kIdentityLut));

    LUT3D lut;
    ASSERT_TRUE(lut.loadFromFile(invertPath));
    EXPECT_EQ(lut.name(), QStringLiteral("Invert"));
    ASSERT_TRUE(lut.loadFromFile(identityPath));
    EXPECT_TRUE(lut.isValid());
    EXPECT_EQ(lut.size(), 2);
    EXPECT_TRUE(lut.name().isEmpty());

    QImage input(1, 1, QImage::Format_RGBA8888);
    input.setPixelColor(0, 0, QColor(255, 0, 0, 128));
    EXPECT_EQ(lut.apply(input).pixelColor(0, 0), QColor(255, 0, 0, 128));
}

TEST(LUTLoaderContractTest, IncompleteCubeInvalidatesPartialData)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString identityPath = directory.filePath(QStringLiteral("identity.cube"));
    const QString path = directory.filePath(QStringLiteral("incomplete.cube"));
    ASSERT_TRUE(writeTextFile(identityPath, kIdentityLut));
    const QByteArray incomplete =
        "TITLE \"Partial\"\n"
        "LUT_3D_SIZE 2\n"
        "0 0 0\n"
        "0 0 1\n"
        "0 1 0\n"
        "0 1 1\n"
        "1 0 0\n"
        "1 0 1\n"
        "1 1 0\n";
    ASSERT_TRUE(writeTextFile(path, incomplete));

    LUT3D lut;
    ASSERT_TRUE(lut.loadFromFile(identityPath));
    EXPECT_TRUE(lut.isValid());
    EXPECT_FALSE(lut.loadFromFile(path));
    EXPECT_FALSE(lut.isValid());
    EXPECT_EQ(lut.size(), 0);
    EXPECT_TRUE(lut.name().isEmpty());

    QImage input(1, 1, QImage::Format_RGBA8888);
    input.setPixelColor(0, 0, QColor(24, 48, 96, 128));
    EXPECT_EQ(lut.apply(input).pixelColor(0, 0), QColor(24, 48, 96, 128));
}
