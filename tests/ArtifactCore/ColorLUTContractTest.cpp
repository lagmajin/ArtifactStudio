#include <gtest/gtest.h>

#include <QColor>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QVector3D>
#include <QByteArray>

#include <limits>

import Color.LUT;

using namespace ArtifactCore;

namespace {

QString writeLutFile(QTemporaryDir& directory, const QString& name,
                     const QByteArray& contents)
{
    const QString path = directory.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    if (file.write(contents) != contents.size()) {
        return {};
    }
    return path;
}

class LutManagerReset {
public:
    LutManagerReset() : manager_(LUTManager::instance()) { manager_.clear(); }
    ~LutManagerReset() { manager_.clear(); }

    LUTManager& manager() { return manager_; }

private:
    LUTManager& manager_;
};

} // namespace

TEST(ColorLUTContractTest, IdentityLUTPreservesInteriorAndClampsInput)
{
    auto lut = ColorLUT::createIdentity(2);
    ASSERT_TRUE(lut.isValid());

    float red = 0.25f;
    float green = 0.5f;
    float blue = 0.75f;
    lut.apply(red, green, blue);

    EXPECT_FLOAT_EQ(red, 0.25f);
    EXPECT_FLOAT_EQ(green, 0.5f);
    EXPECT_FLOAT_EQ(blue, 0.75f);

    red = -0.2f;
    green = 1.2f;
    blue = 0.4f;
    lut.apply(red, green, blue);
    EXPECT_FLOAT_EQ(red, 0.0f);
    EXPECT_FLOAT_EQ(green, 1.0f);
    EXPECT_FLOAT_EQ(blue, 0.4f);
}

TEST(ColorLUTContractTest, TrilinearInterpolationMatchesAffineCornerValues)
{
    auto lut = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                lut.setValue(x, y, z, QVector3D(
                    0.2f + 0.4f * x + 0.1f * y,
                    0.1f + 0.5f * y,
                    0.3f + 0.5f * z));
            }
        }
    }

    float red = 0.5f;
    float green = 0.5f;
    float blue = 0.5f;
    lut.apply(red, green, blue);

    EXPECT_NEAR(red, 0.45f, 1e-6f);
    EXPECT_NEAR(green, 0.35f, 1e-6f);
    EXPECT_NEAR(blue, 0.55f, 1e-6f);
}

TEST(ColorLUTContractTest, IntensityBlendsMappedColorWithOriginal)
{
    auto lut = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                lut.setValue(x, y, z, QVector3D(1.0f - x, y, z));
            }
        }
    }

    const QColor original = QColor::fromRgbF(0.2, 0.4, 0.6, 0.7);
    const QColor result = lut.applyWithIntensity(original, 0.5f);

    EXPECT_NEAR(result.redF(), 0.5, 1e-3);
    EXPECT_NEAR(result.greenF(), 0.4, 1e-3);
    EXPECT_NEAR(result.blueF(), 0.6, 1e-3);
    EXPECT_NEAR(result.alphaF(), 0.7, 1e-3);
}

TEST(ColorLUTContractTest, CubeFileLoadsTitleSizeAndTrilinearSamples)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("fixture.cube"),
        "TITLE \"Test Grade\"\n"
        "# fixture\n"
        "LUT_3D_SIZE 2\n"
        "LUT_3D_INPUT_RANGE 0.0 1.0\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.format(), LUTFormat::Cube);
    EXPECT_EQ(lut.name(), QStringLiteral("Test Grade"));
    EXPECT_EQ(lut.size().dimX, 2);
    EXPECT_EQ(lut.size().dimY, 2);
    EXPECT_EQ(lut.size().dimZ, 2);
    const QVector3D center = lut.sample(0.5f, 0.5f, 0.5f);
    EXPECT_NEAR(center.x(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.y(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.z(), 0.5f, 1e-6f);
}

TEST(ColorLUTContractTest, CubeFileRejectsIncompleteSampleData)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("incomplete.cube"),
        "LUT_3D_SIZE 2\n0 0 0\n1 0 0\n0 1 0\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    EXPECT_FALSE(lut.isValid());
    EXPECT_FALSE(lut.errorMessage().isEmpty());
}

TEST(ColorLUTContractTest, CubeFileRejectsExtraSampleTriplets)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("extra.cube"),
        "LUT_3D_SIZE 2\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n"
        "0.5 0.5 0.5\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    EXPECT_FALSE(lut.isValid());
    EXPECT_FALSE(lut.errorMessage().isEmpty());
}

TEST(ColorLUTContractTest, CubeFileRejectsNonFiniteSamples)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("non-finite.cube"),
        "LUT_3D_SIZE 2\n"
        "nan 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    EXPECT_FALSE(lut.isValid());
    EXPECT_FALSE(lut.errorMessage().isEmpty());
}

TEST(ColorLUTContractTest, CubeSaveAndReloadPreservesLUTValues)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("roundtrip.cube"));

    auto source = ColorLUT::createIdentity(2);
    source.setName(QStringLiteral("Round Trip"));
    source.setValue(1, 0, 0, QVector3D(0.8f, 0.1f, 0.2f));
    ASSERT_TRUE(source.saveToCube(path));

    const ColorLUT loaded(path);
    ASSERT_TRUE(loaded.isValid()) << loaded.errorMessage().toStdString();
    EXPECT_EQ(loaded.name(), QStringLiteral("Round Trip"));
    const QVector3D value = loaded.getValue(1, 0, 0);
    EXPECT_NEAR(value.x(), 0.8f, 1e-6f);
    EXPECT_NEAR(value.y(), 0.1f, 1e-6f);
    EXPECT_NEAR(value.z(), 0.2f, 1e-6f);
}

TEST(ColorLUTContractTest, CspDataBlockLoadsDeclaredSizeAndSamples)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("fixture.csp"),
        "CSPLUTV100\nLUT_3D_SIZE 2\nBEGIN DATA\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\nEND DATA\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.format(), LUTFormat::Csp);
    EXPECT_EQ(lut.size().totalPoints(), 8);
    const QVector3D center = lut.sample(0.5f, 0.5f, 0.5f);
    EXPECT_NEAR(center.x(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.y(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.z(), 0.5f, 1e-6f);
}

TEST(ColorLUTContractTest, CspRejectsNonFiniteSamples)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("non-finite.csp"),
        "LUT_3D_SIZE 2\nBEGIN DATA\n"
        "nan 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\nEND DATA\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    EXPECT_FALSE(lut.isValid());
    EXPECT_FALSE(lut.errorMessage().isEmpty());
}

TEST(ColorLUTContractTest, ThreeDlNormalizesIntegerRangeToUnitInterval)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("fixture.3dl"),
        "2\n"
        "0 0 0\n16 0 0\n0 16 0\n16 16 0\n"
        "0 0 16\n16 0 16\n0 16 16\n16 16 16\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.format(), LUTFormat::_3dl);
    EXPECT_EQ(lut.size().totalPoints(), 8);
    const QVector3D center = lut.sample(0.5f, 0.5f, 0.5f);
    EXPECT_NEAR(center.x(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.y(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.z(), 0.5f, 1e-6f);
}

TEST(ColorLUTContractTest, ThreeDlRejectsNegativeSamples)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("negative.3dl"),
        "2\n"
        "-1 0 0\n16 0 0\n0 16 0\n16 16 0\n"
        "0 0 16\n16 0 16\n0 16 16\n16 16 16\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    EXPECT_FALSE(lut.isValid());
    EXPECT_FALSE(lut.errorMessage().isEmpty());
}

TEST(ColorLUTContractTest, WithIntensityBlendsLUTGridTowardIdentity)
{
    auto source = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                source.setValue(x, y, z, QVector3D(1.0f - x, y, z));
            }
        }
    }

    const ColorLUT zero = source.withIntensity(0.0f);
    const ColorLUT half = source.withIntensity(0.5f);
    const ColorLUT full = source.withIntensity(1.0f);
    EXPECT_NEAR(zero.getValue(1, 0, 0).x(), 1.0f, 1e-6f);
    EXPECT_NEAR(half.getValue(1, 0, 0).x(), 0.5f, 1e-6f);
    EXPECT_NEAR(full.getValue(1, 0, 0).x(), 0.0f, 1e-6f);
}

TEST(ColorLUTContractTest, CombiningIndependentChannelInversionsComposesThem)
{
    auto invertRed = ColorLUT::createIdentity(2);
    auto invertGreen = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                invertRed.setValue(x, y, z, QVector3D(1.0f - x, y, z));
                invertGreen.setValue(x, y, z, QVector3D(x, 1.0f - y, z));
            }
        }
    }

    const ColorLUT combined = invertRed.combine(invertGreen);
    const QVector3D result = combined.sample(0.2f, 0.4f, 0.6f);
    EXPECT_NEAR(result.x(), 0.8f, 1e-6f);
    EXPECT_NEAR(result.y(), 0.6f, 1e-6f);
    EXPECT_NEAR(result.z(), 0.6f, 1e-6f);
}

TEST(ColorLUTContractTest, InvertedLUTMapsTransformedSampleBackToSource)
{
    auto source = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                source.setValue(x, y, z, QVector3D(1.0f - x, y, z));
            }
        }
    }

    const ColorLUT inverse = source.inverted();
    const QVector3D result = inverse.sample(0.8f, 0.4f, 0.6f);
    EXPECT_NEAR(result.x(), 0.2f, 1e-5f);
    EXPECT_NEAR(result.y(), 0.4f, 1e-5f);
    EXPECT_NEAR(result.z(), 0.6f, 1e-5f);
}

TEST(ColorLUTContractTest, ImageImportUsesHaldTileAndChannelOrdering)
{
    constexpr int lutSize = 2;
    QImage image(lutSize * lutSize, lutSize * lutSize, QImage::Format_RGB32);
    ASSERT_FALSE(image.isNull());
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                const int px = x + (z % 2) * lutSize;
                const int py = y + (z / 2) * lutSize;
                image.setPixel(px, py, qRgb(x * 255, y * 255, z * 255));
            }
        }
    }

    ColorLUT lut;
    ASSERT_TRUE(lut.loadFromImage(image, lutSize)) << lut.errorMessage().toStdString();
    EXPECT_TRUE(lut.isValid());
    EXPECT_EQ(lut.format(), LUTFormat::PNG);
    EXPECT_EQ(lut.size().totalPoints(), 8);
    const QVector3D center = lut.sample(0.5f, 0.5f, 0.5f);
    EXPECT_NEAR(center.x(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.y(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.z(), 0.5f, 1e-6f);
}

TEST(ColorLUTContractTest, ImageImportRejectsInsufficientTileArea)
{
    const QImage image(3, 3, QImage::Format_RGB32);
    ColorLUT lut;

    EXPECT_FALSE(lut.loadFromImage(image, 2));
    EXPECT_FALSE(lut.errorMessage().isEmpty());
}

TEST(ColorLUTContractTest, GetValueReturnsZeroVectorOutsideGrid)
{
    const auto lut = ColorLUT::createIdentity(2);

    EXPECT_EQ(lut.getValue(-1, 0, 0), QVector3D());
    EXPECT_EQ(lut.getValue(2, 0, 0), QVector3D());
    EXPECT_EQ(lut.getValue(0, 0, 2), QVector3D());
}

TEST(ColorLUTContractTest, SetValueClampsChannelsAndIgnoresInvalidWrites)
{
    auto lut = ColorLUT::createIdentity(2);
    lut.setValue(0, 0, 0, QVector3D(-0.5f, 0.4f, 1.5f));
    EXPECT_EQ(lut.getValue(0, 0, 0), QVector3D(0.0f, 0.4f, 1.0f));

    lut.setValue(1, 0, 0, QVector3D(
        std::numeric_limits<float>::quiet_NaN(), 0.2f, 0.3f));
    EXPECT_EQ(lut.getValue(1, 0, 0), QVector3D(1.0f, 0.0f, 0.0f));
    lut.setValue(2, 0, 0, QVector3D(0.1f, 0.2f, 0.3f));
    EXPECT_EQ(lut.getValue(1, 0, 0), QVector3D(1.0f, 0.0f, 0.0f));
}

TEST(ColorLUTContractTest, CopyOwnsIndependentLUTSamples)
{
    auto original = ColorLUT::createIdentity(2);
    auto copy = original;
    copy.setValue(1, 1, 1, QVector3D(0.2f, 0.3f, 0.4f));

    EXPECT_EQ(original.getValue(1, 1, 1), QVector3D(1.0f, 1.0f, 1.0f));
    EXPECT_EQ(copy.getValue(1, 1, 1), QVector3D(0.2f, 0.3f, 0.4f));
    EXPECT_EQ(copy.dataSize(), 2u * 2u * 2u * 3u * sizeof(float));
}

TEST(ColorLUTContractTest, ApplyToImageTransformsRGBAndPreservesAlphaAndSource)
{
    auto lut = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                lut.setValue(x, y, z, QVector3D(1.0f - x, y, z));
            }
        }
    }

    QImage source(2, 1, QImage::Format_ARGB32);
    source.setPixel(0, 0, qRgba(51, 102, 153, 77));
    source.setPixel(1, 0, qRgba(0, 255, 64, 0));
    const QImage result = lut.applyToImage(source);

    EXPECT_EQ(result.format(), QImage::Format_ARGB32);
    EXPECT_NEAR(qRed(result.pixel(0, 0)), 204, 1);
    EXPECT_NEAR(qGreen(result.pixel(0, 0)), 102, 1);
    EXPECT_NEAR(qBlue(result.pixel(0, 0)), 153, 1);
    EXPECT_EQ(qAlpha(result.pixel(0, 0)), 77);
    EXPECT_NEAR(qRed(result.pixel(1, 0)), 255, 1);
    EXPECT_NEAR(qGreen(result.pixel(1, 0)), 255, 1);
    EXPECT_NEAR(qBlue(result.pixel(1, 0)), 64, 1);
    EXPECT_EQ(qAlpha(result.pixel(1, 0)), 0);
    EXPECT_EQ(source.pixel(0, 0), qRgba(51, 102, 153, 77));
    EXPECT_EQ(source.pixel(1, 0), qRgba(0, 255, 64, 0));
}

TEST(ColorLUTContractTest, ApplyToImageWithInvalidLUTPreservesSourcePixels)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("invalid.cube"),
        "LUT_3D_SIZE 2\n0 0 0\n");
    ASSERT_FALSE(path.isEmpty());
    const ColorLUT invalid(path);
    ASSERT_FALSE(invalid.isValid());

    QImage source(1, 1, QImage::Format_ARGB32);
    source.setPixel(0, 0, qRgba(17, 34, 51, 68));
    const QImage result = invalid.applyToImage(source);
    EXPECT_EQ(result, source);
    EXPECT_EQ(result.pixel(0, 0), qRgba(17, 34, 51, 68));
}

TEST(ColorLUTManagerContractTest, RegistrationTrimsNamesRejectsInvalidAndOverwrites)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    auto first = ColorLUT::createIdentity(2);
    auto replacement = ColorLUT::createIdentity(2);
    replacement.setValue(1, 1, 1, QVector3D(0.25f, 0.5f, 0.75f));

    manager.registerLUT(QStringLiteral("  grade  "), first);
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("grade")));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("  grade  ")));
    manager.registerLUT(QStringLiteral("grade"), replacement);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("grade")});
    EXPECT_EQ(manager.getLUT(QStringLiteral("grade")).getValue(1, 1, 1),
              QVector3D(0.25f, 0.5f, 0.75f));

    manager.registerLUT(QStringLiteral("  "), first);
    EXPECT_EQ(manager.lutNames().size(), 1);
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString invalidPath = writeLutFile(directory,
        QStringLiteral("invalid.cube"), "LUT_3D_SIZE 2\n0 0 0\n");
    ASSERT_FALSE(invalidPath.isEmpty());
    const ColorLUT invalid(invalidPath);
    manager.registerLUT(QStringLiteral("invalid"), invalid);
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("invalid")));

    manager.registerLUT(QStringLiteral("grade"), invalid);
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("grade")));
    EXPECT_EQ(manager.getLUT(QStringLiteral("grade")).getValue(1, 1, 1),
              QVector3D(0.25f, 0.5f, 0.75f));
}

TEST(ColorLUTManagerContractTest, RemoveAndClearUpdateRegistry)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    const auto identity = ColorLUT::createIdentity(2);
    manager.registerLUT(QStringLiteral("first"), identity);
    manager.registerLUT(QStringLiteral("second"), identity);

    manager.removeLUT(QStringLiteral("first"));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("first")));
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("second")));
    manager.clear();
    EXPECT_TRUE(manager.lutNames().isEmpty());
}

TEST(ColorLUTManagerContractTest, DirectoryLoadRegistersOnlyValidSupportedFiles)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QByteArray validCube =
        "LUT_3D_SIZE 2\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n";
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("usable.cube"), validCube).isEmpty());
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("broken.cube"),
                              "LUT_3D_SIZE 2\n0 0 0\n").isEmpty());
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("ignored.txt"), validCube).isEmpty());

    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 1);
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("usable")));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("broken")));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("ignored")));
    EXPECT_EQ(manager.loadFromDirectory(directory.filePath(QStringLiteral("missing"))), 0);
}
