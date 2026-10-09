#include <gtest/gtest.h>

#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QList>
#include <QTemporaryDir>
#include <QVector3D>
#include <QByteArray>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

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

TEST(ColorLUTContractTest, NonIdentityAffineLUTClampsEachInputAxisBeforeSampling)
{
    constexpr int gridSize = 3;
    auto lut = ColorLUT::createIdentity(gridSize);
    for (int z = 0; z < gridSize; ++z) {
        for (int y = 0; y < gridSize; ++y) {
            for (int x = 0; x < gridSize; ++x) {
                const float red = x / float(gridSize - 1);
                const float green = y / float(gridSize - 1);
                const float blue = z / float(gridSize - 1);
                lut.setValue(x, y, z, QVector3D(
                    0.1f + 0.7f * red,
                    0.2f + 0.6f * green,
                    0.05f + 0.8f * blue));
            }
        }
    }

    constexpr float coordinates[] = {-0.5f, -0.01f, 0.0f, 0.125f,
                                     0.5f, 0.875f, 1.0f, 1.01f, 1.5f};
    for (const float inputR : coordinates) {
        for (const float inputG : coordinates) {
            for (const float inputB : coordinates) {
                const float clampedR = std::clamp(inputR, 0.0f, 1.0f);
                const float clampedG = std::clamp(inputG, 0.0f, 1.0f);
                const float clampedB = std::clamp(inputB, 0.0f, 1.0f);
                float red = inputR;
                float green = inputG;
                float blue = inputB;
                lut.apply(red, green, blue);
                SCOPED_TRACE(::testing::Message()
                    << "input=" << inputR << ',' << inputG << ',' << inputB);
                EXPECT_NEAR(red, 0.1f + 0.7f * clampedR, 2e-6f);
                EXPECT_NEAR(green, 0.2f + 0.6f * clampedG, 2e-6f);
                EXPECT_NEAR(blue, 0.05f + 0.8f * clampedB, 2e-6f);
            }
        }
    }
}

TEST(ColorLUTContractTest, IdentityFactoryFallsBackToTwoPointGridForInvalidSmallSizes)
{
    for (const int requestedSize : {0, 1, -4}) {
        const ColorLUT lut = ColorLUT::createIdentity(requestedSize);
        SCOPED_TRACE(requestedSize);
        ASSERT_TRUE(lut.isValid());
        EXPECT_EQ(lut.size().dimX, 2);
        EXPECT_EQ(lut.size().dimY, 2);
        EXPECT_EQ(lut.size().dimZ, 2);
        EXPECT_EQ(lut.dataSize(), 2u * 2u * 2u * 3u * sizeof(float));
    }
}

TEST(ColorLUTContractTest, IdentityFactoryFillsEveryGridPointForSupportedSizes)
{
    for (const int gridSize : {2, 3, 17}) {
        const ColorLUT lut = ColorLUT::createIdentity(gridSize);
        SCOPED_TRACE(gridSize);
        ASSERT_TRUE(lut.isValid());
        EXPECT_EQ(lut.size().dimX, gridSize);
        EXPECT_EQ(lut.size().dimY, gridSize);
        EXPECT_EQ(lut.size().dimZ, gridSize);
        for (int z = 0; z < gridSize; ++z) {
            for (int y = 0; y < gridSize; ++y) {
                for (int x = 0; x < gridSize; ++x) {
                    const QVector3D sample = lut.getValue(x, y, z);
                    SCOPED_TRACE(::testing::Message()
                        << "x=" << x << " y=" << y << " z=" << z);
                    EXPECT_FLOAT_EQ(sample.x(), x / float(gridSize - 1));
                    EXPECT_FLOAT_EQ(sample.y(), y / float(gridSize - 1));
                    EXPECT_FLOAT_EQ(sample.z(), z / float(gridSize - 1));
                }
            }
        }
    }
}

TEST(ColorLUTContractTest, IdentityFactoryAcceptsMaximumSizeAndFallsBackAboveIt)
{
    {
        const ColorLUT maximum = ColorLUT::createIdentity(256);
        ASSERT_TRUE(maximum.isValid());
        EXPECT_EQ(maximum.size().dimX, 256);
        EXPECT_EQ(maximum.size().dimY, 256);
        EXPECT_EQ(maximum.size().dimZ, 256);
        EXPECT_EQ(maximum.dataSize(), 256u * 256u * 256u * 3u * sizeof(float));
        EXPECT_EQ(maximum.getValue(0, 0, 0), QVector3D(0.0f, 0.0f, 0.0f));
        EXPECT_EQ(maximum.getValue(255, 255, 255), QVector3D(1.0f, 1.0f, 1.0f));
        constexpr struct Coordinate { int x; int y; int z; } coordinates[] = {
            {1, 64, 200}, {127, 128, 129}, {254, 17, 3},
        };
        for (const auto& coordinate : coordinates) {
            const QVector3D sample = maximum.getValue(
                coordinate.x, coordinate.y, coordinate.z);
            SCOPED_TRACE(::testing::Message()
                << "x=" << coordinate.x << " y=" << coordinate.y
                << " z=" << coordinate.z);
            EXPECT_FLOAT_EQ(sample.x(), coordinate.x / 255.0f);
            EXPECT_FLOAT_EQ(sample.y(), coordinate.y / 255.0f);
            EXPECT_FLOAT_EQ(sample.z(), coordinate.z / 255.0f);
        }
    }

    const ColorLUT aboveMaximum = ColorLUT::createIdentity(257);
    ASSERT_TRUE(aboveMaximum.isValid());
    EXPECT_EQ(aboveMaximum.size().dimX, 2);
    EXPECT_EQ(aboveMaximum.size().dimY, 2);
    EXPECT_EQ(aboveMaximum.size().dimZ, 2);
    EXPECT_EQ(aboveMaximum.dataSize(), 2u * 2u * 2u * 3u * sizeof(float));
}

TEST(ColorLUTContractTest, FloatApplyMapsNonFiniteInputsToZeroBeforeSampling)
{
    const auto lut = ColorLUT::createIdentity(2);
    float red = std::numeric_limits<float>::quiet_NaN();
    float green = std::numeric_limits<float>::infinity();
    float blue = -std::numeric_limits<float>::infinity();

    lut.apply(red, green, blue);

    EXPECT_FLOAT_EQ(red, 0.0f);
    EXPECT_FLOAT_EQ(green, 0.0f);
    EXPECT_FLOAT_EQ(blue, 0.0f);
}

TEST(ColorLUTContractTest, RawDataCorruptionIsSanitizedBySampleAndApply)
{
    auto lut = ColorLUT::createIdentity(2);
    float* data = lut.rawData();
    ASSERT_NE(data, nullptr);
    data[0] = std::numeric_limits<float>::quiet_NaN();
    data[1] = std::numeric_limits<float>::infinity();
    data[2] = -std::numeric_limits<float>::infinity();
    const size_t lastSample = (2u * 2u * 2u - 1u) * 3u;
    data[lastSample] = -0.5f;
    data[lastSample + 1u] = 1.5f;
    data[lastSample + 2u] = 0.25f;

    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    lut.apply(red, green, blue);
    EXPECT_FLOAT_EQ(red, 0.0f);
    EXPECT_FLOAT_EQ(green, 0.0f);
    EXPECT_FLOAT_EQ(blue, 0.0f);

    red = 1.0f;
    green = 1.0f;
    blue = 1.0f;
    lut.apply(red, green, blue);
    EXPECT_FLOAT_EQ(red, 0.0f);
    EXPECT_FLOAT_EQ(green, 1.0f);
    EXPECT_FLOAT_EQ(blue, 0.25f);
}

TEST(ColorLUTContractTest, QColorApplyTransformsTransparentRgbAndPreservesAlpha)
{
    auto lut = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                lut.setValue(x, y, z, QVector3D(1.0f - x, y, z));
            }
        }
    }
    const QColor source = QColor::fromRgbF(0.2, 0.4, 0.6, 0.0);

    const QColor result = lut.apply(source);

    EXPECT_NEAR(result.redF(), 0.8, 1e-6);
    EXPECT_NEAR(result.greenF(), 0.4, 1e-6);
    EXPECT_NEAR(result.blueF(), 0.6, 1e-6);
    EXPECT_FLOAT_EQ(result.alphaF(), 0.0f);
    EXPECT_NEAR(source.redF(), 0.2, 1e-6);
    EXPECT_NEAR(source.greenF(), 0.4, 1e-6);
    EXPECT_NEAR(source.blueF(), 0.6, 1e-6);
    EXPECT_FLOAT_EQ(source.alphaF(), 0.0f);
}

TEST(ColorLUTContractTest, QColorApplyPreservesAlphaAcrossOpacityRange)
{
    auto lut = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                lut.setValue(x, y, z, QVector3D(1.0f - x, 0.25f + 0.5f * y,
                                                 0.75f - 0.5f * z));
            }
        }
    }

    constexpr float alphas[] = {0.0f, 0.125f, 0.5f, 0.875f, 1.0f};
    for (const float alpha : alphas) {
        const QColor source = QColor::fromRgbF(0.2, 0.4, 0.6, alpha);
        const QColor result = lut.apply(source);
        SCOPED_TRACE(alpha);
        EXPECT_TRUE(result.isValid());
        EXPECT_NEAR(result.redF(), 0.8, 1e-5);
        EXPECT_NEAR(result.greenF(), 0.45, 1e-5);
        EXPECT_NEAR(result.blueF(), 0.45, 1e-5);
        EXPECT_NEAR(result.alphaF(), alpha, 1e-5);
        EXPECT_NEAR(source.redF(), 0.2, 1e-5);
        EXPECT_NEAR(source.greenF(), 0.4, 1e-5);
        EXPECT_NEAR(source.blueF(), 0.6, 1e-5);
        EXPECT_NEAR(source.alphaF(), alpha, 1e-5);
    }
}

TEST(ColorLUTContractTest, InvalidFloatLutLeavesInputChannelsUntouched)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("invalid-float.cube"),
                                      "LUT_3D_SIZE 2\n0 0 0\n");
    ASSERT_FALSE(path.isEmpty());
    const ColorLUT invalid(path);
    ASSERT_FALSE(invalid.isValid());
    float red = -0.25f;
    float green = 0.5f;
    float blue = 1.25f;

    invalid.apply(red, green, blue);

    EXPECT_FLOAT_EQ(red, -0.25f);
    EXPECT_FLOAT_EQ(green, 0.5f);
    EXPECT_FLOAT_EQ(blue, 1.25f);
}

TEST(ColorLUTContractTest, InvalidLutQColorApplicationsPreserveSourceChannels)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("invalid-qcolor.cube"),
                                      "LUT_3D_SIZE 2\n0 0 0\n");
    ASSERT_FALSE(path.isEmpty());
    const ColorLUT invalid(path);
    ASSERT_FALSE(invalid.isValid());

    const QColor source = QColor::fromRgbF(0.2, 0.4, 0.6, 0.37);
    const QColor directResult = invalid.apply(source);
    EXPECT_NEAR(directResult.redF(), source.redF(), 1e-6);
    EXPECT_NEAR(directResult.greenF(), source.greenF(), 1e-6);
    EXPECT_NEAR(directResult.blueF(), source.blueF(), 1e-6);
    EXPECT_FLOAT_EQ(directResult.alphaF(), source.alphaF());

    constexpr float intensities[] = {0.0f, 0.35f, 1.0f};
    for (const float intensity : intensities) {
        const QColor result = invalid.applyWithIntensity(source, intensity);
        SCOPED_TRACE(intensity);
        EXPECT_NEAR(result.redF(), source.redF(), 1e-6);
        EXPECT_NEAR(result.greenF(), source.greenF(), 1e-6);
        EXPECT_NEAR(result.blueF(), source.blueF(), 1e-6);
        EXPECT_FLOAT_EQ(result.alphaF(), source.alphaF());
    }

    EXPECT_NEAR(source.redF(), 0.2, 1e-6);
    EXPECT_NEAR(source.greenF(), 0.4, 1e-6);
    EXPECT_NEAR(source.blueF(), 0.6, 1e-6);
    EXPECT_NEAR(source.alphaF(), 0.37, 1e-6);
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

TEST(ColorLUTContractTest, TrilinearInterpolationReproducesMultilinearFieldsAcrossCells)
{
    constexpr int gridSize = 3;
    auto lut = ColorLUT::createIdentity(gridSize);
    for (int z = 0; z < gridSize; ++z) {
        for (int y = 0; y < gridSize; ++y) {
            for (int x = 0; x < gridSize; ++x) {
                const float r = x / float(gridSize - 1);
                const float g = y / float(gridSize - 1);
                const float b = z / float(gridSize - 1);
                lut.setValue(x, y, z, QVector3D(
                    0.1f + 0.2f * r + 0.1f * g + 0.15f * b + 0.1f * r * g,
                    0.05f + 0.1f * r + 0.15f * g + 0.2f * b + 0.1f * r * g * b,
                    0.1f + 0.15f * r + 0.05f * g + 0.1f * b + 0.1f * r * b + 0.05f * g * b));
            }
        }
    }

    constexpr float coordinates[] = {
        0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f,
    };
    for (const float inputR : coordinates) {
        for (const float inputG : coordinates) {
            for (const float inputB : coordinates) {
                float red = inputR;
                float green = inputG;
                float blue = inputB;
                lut.apply(red, green, blue);
                SCOPED_TRACE(::testing::Message()
                    << "r=" << inputR << " g=" << inputG << " b=" << inputB);
                EXPECT_NEAR(red, 0.1f + 0.2f * inputR + 0.1f * inputG +
                                  0.15f * inputB + 0.1f * inputR * inputG, 2e-6f);
                EXPECT_NEAR(green, 0.05f + 0.1f * inputR + 0.15f * inputG +
                                    0.2f * inputB + 0.1f * inputR * inputG * inputB, 2e-6f);
                EXPECT_NEAR(blue, 0.1f + 0.15f * inputR + 0.05f * inputG +
                                   0.1f * inputB + 0.1f * inputR * inputB +
                                   0.05f * inputG * inputB, 2e-6f);
            }
        }
    }
}

TEST(ColorLUTContractTest, TrilinearInterpolationMatchesEightCornerWeightsInEveryCell)
{
    constexpr int gridSize = 4;
    auto lut = ColorLUT::createIdentity(gridSize);
    for (int z = 0; z < gridSize; ++z) {
        for (int y = 0; y < gridSize; ++y) {
            for (int x = 0; x < gridSize; ++x) {
                const float fx = float(x) / float(gridSize - 1);
                const float fy = float(y) / float(gridSize - 1);
                const float fz = float(z) / float(gridSize - 1);
                lut.setValue(x, y, z, QVector3D(
                    0.05f + 0.65f * fx + 0.1f * fy * fz,
                    0.1f + 0.6f * fy + 0.1f * fx * fz,
                    0.08f + 0.62f * fz + 0.1f * fx * fy));
            }
        }
    }

    constexpr float localCoordinates[] = {0.0f, 0.17f, 0.5f, 0.83f, 1.0f};
    const auto reference = [](const float value[2][2][2],
                              const float tx, const float ty, const float tz) {
        float result = 0.0f;
        for (int z = 0; z < 2; ++z) {
            for (int y = 0; y < 2; ++y) {
                for (int x = 0; x < 2; ++x) {
                    const float wx = x == 0 ? 1.0f - tx : tx;
                    const float wy = y == 0 ? 1.0f - ty : ty;
                    const float wz = z == 0 ? 1.0f - tz : tz;
                    result += value[z][y][x] * wx * wy * wz;
                }
            }
        }
        return result;
    };

    for (int cellZ = 0; cellZ < gridSize - 1; ++cellZ) {
        for (int cellY = 0; cellY < gridSize - 1; ++cellY) {
            for (int cellX = 0; cellX < gridSize - 1; ++cellX) {
                float corners[3][2][2][2] = {};
                for (int z = 0; z < 2; ++z) {
                    for (int y = 0; y < 2; ++y) {
                        for (int x = 0; x < 2; ++x) {
                            const QVector3D value = lut.getValue(cellX + x, cellY + y, cellZ + z);
                            corners[0][z][y][x] = value.x();
                            corners[1][z][y][x] = value.y();
                            corners[2][z][y][x] = value.z();
                        }
                    }
                }

                for (const float tz : localCoordinates) {
                    for (const float ty : localCoordinates) {
                        for (const float tx : localCoordinates) {
                            const float inputR = (cellX + tx) / float(gridSize - 1);
                            const float inputG = (cellY + ty) / float(gridSize - 1);
                            const float inputB = (cellZ + tz) / float(gridSize - 1);
                            float red = inputR;
                            float green = inputG;
                            float blue = inputB;
                            lut.apply(red, green, blue);
                            SCOPED_TRACE(::testing::Message()
                                << "cell=" << cellX << ',' << cellY << ',' << cellZ
                                << " local=" << tx << ',' << ty << ',' << tz);
                            EXPECT_NEAR(red, reference(corners[0], tx, ty, tz), 2e-6f);
                            EXPECT_NEAR(green, reference(corners[1], tx, ty, tz), 2e-6f);
                            EXPECT_NEAR(blue, reference(corners[2], tx, ty, tz), 2e-6f);
                        }
                    }
                }
            }
        }
    }
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

TEST(ColorLUTContractTest, ApplyWithIntensityInterpolatesAcrossEndpointsAndPreservesAlpha)
{
    auto lut = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                lut.setValue(x, y, z, QVector3D(1.0f - x, y, z));
            }
        }
    }

    const QColor source = QColor::fromRgbF(0.2, 0.4, 0.6, 0.37);
    constexpr float intensities[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (const float intensity : intensities) {
        const QColor result = lut.applyWithIntensity(source, intensity);
        SCOPED_TRACE(intensity);
        EXPECT_NEAR(result.redF(), 0.2 + 0.6 * intensity, 2.0e-4);
        EXPECT_NEAR(result.greenF(), 0.4, 2.0e-4);
        EXPECT_NEAR(result.blueF(), 0.6, 2.0e-4);
        EXPECT_NEAR(result.alphaF(), source.alphaF(), 1.0e-6);
    }
}

TEST(ColorLUTContractTest, ApplyWithIntensityMatchesIndependentAffineRgbReferenceGrid)
{
    constexpr int gridSize = 3;
    auto lut = ColorLUT::createIdentity(gridSize);
    for (int z = 0; z < gridSize; ++z) {
        for (int y = 0; y < gridSize; ++y) {
            for (int x = 0; x < gridSize; ++x) {
                const float red = x / float(gridSize - 1);
                const float green = y / float(gridSize - 1);
                const float blue = z / float(gridSize - 1);
                lut.setValue(x, y, z, QVector3D(
                    0.05f + 0.55f * red + 0.1f * green,
                    0.1f + 0.6f * green + 0.05f * blue,
                    0.08f + 0.5f * blue + 0.12f * red));
            }
        }
    }

    constexpr float coordinates[] = {0.0f, 0.125f, 0.35f, 0.5f, 0.875f, 1.0f};
    constexpr float intensities[] = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};
    constexpr float alphas[] = {0.0f, 0.17f, 0.5f, 0.93f, 1.0f};

    for (const float red : coordinates) {
        for (const float green : coordinates) {
            for (const float blue : coordinates) {
                const float mappedRed = 0.05f + 0.55f * red + 0.1f * green;
                const float mappedGreen = 0.1f + 0.6f * green + 0.05f * blue;
                const float mappedBlue = 0.08f + 0.5f * blue + 0.12f * red;
                for (const float alpha : alphas) {
                    const QColor source = QColor::fromRgbF(red, green, blue, alpha);
                    for (const float intensity : intensities) {
                        const QColor result = lut.applyWithIntensity(source, intensity);
                        SCOPED_TRACE(::testing::Message()
                            << "rgb=" << red << ',' << green << ',' << blue
                            << " alpha=" << alpha << " intensity=" << intensity);
                        EXPECT_NEAR(result.redF(),
                                    red * (1.0f - intensity) + mappedRed * intensity,
                                    3e-4);
                        EXPECT_NEAR(result.greenF(),
                                    green * (1.0f - intensity) + mappedGreen * intensity,
                                    3e-4);
                        EXPECT_NEAR(result.blueF(),
                                    blue * (1.0f - intensity) + mappedBlue * intensity,
                                    3e-4);
                        EXPECT_NEAR(result.alphaF(), source.alphaF(), 1e-6);
                    }
                }
            }
        }
    }
}

TEST(ColorLUTContractTest, ApplyWithOutOfRangeIntensityExtrapolatesRgbAndPreservesAlpha)
{
    auto lut = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                lut.setValue(x, y, z, QVector3D(1.0f - x, y, z));
            }
        }
    }

    const QColor source = QColor::fromRgbF(0.2, 0.4, 0.6, 0.37);
    const QColor aboveOne = lut.applyWithIntensity(source, 1.5f);
    EXPECT_TRUE(aboveOne.isValid());
    EXPECT_NEAR(aboveOne.redF(), 1.1, 5e-4);
    EXPECT_NEAR(aboveOne.greenF(), source.greenF(), 2e-4);
    EXPECT_NEAR(aboveOne.blueF(), source.blueF(), 2e-4);
    EXPECT_NEAR(aboveOne.alphaF(), source.alphaF(), 2e-4);

    const QColor belowZero = lut.applyWithIntensity(source, -0.5f);
    EXPECT_TRUE(belowZero.isValid());
    EXPECT_NEAR(belowZero.redF(), -0.1, 5e-4);
    EXPECT_NEAR(belowZero.greenF(), source.greenF(), 2e-4);
    EXPECT_NEAR(belowZero.blueF(), source.blueF(), 2e-4);
    EXPECT_NEAR(belowZero.alphaF(), source.alphaF(), 2e-4);
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
    float centerRed = 0.5f;
    float centerGreen = 0.5f;
    float centerBlue = 0.5f;
    lut.apply(centerRed, centerGreen, centerBlue);
    const QVector3D center(centerRed, centerGreen, centerBlue);
    EXPECT_NEAR(center.x(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.y(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.z(), 0.5f, 1e-6f);
}

TEST(ColorLUTContractTest, CubeFilePreservesFiniteOutOfRangeSamplesAndApplyClamps)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("extended-range.cube"),
        "LUT_3D_SIZE 2\n"
        "-0.5 0.25 1.5\n-0.5 0.25 1.5\n"
        "-0.5 0.25 1.5\n-0.5 0.25 1.5\n"
        "-0.5 0.25 1.5\n-0.5 0.25 1.5\n"
        "-0.5 0.25 1.5\n-0.5 0.25 1.5\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    const QVector3D stored = lut.getValue(1, 1, 1);
    EXPECT_FLOAT_EQ(stored.x(), -0.5f);
    EXPECT_FLOAT_EQ(stored.y(), 0.25f);
    EXPECT_FLOAT_EQ(stored.z(), 1.5f);

    float red = 0.4f;
    float green = 0.5f;
    float blue = 0.6f;
    lut.apply(red, green, blue);
    EXPECT_FLOAT_EQ(red, 0.0f);
    EXPECT_FLOAT_EQ(green, 0.25f);
    EXPECT_FLOAT_EQ(blue, 1.0f);
}

TEST(ColorLUTContractTest, CubeFileInfersCubicSizeWhenHeaderIsMissing)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(
        directory, QStringLiteral("headerless.cube"),
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.size().dimX, 2);
    EXPECT_EQ(lut.size().dimY, 2);
    EXPECT_EQ(lut.size().dimZ, 2);
    EXPECT_EQ(lut.getValue(1, 1, 1), QVector3D(1.0f, 1.0f, 1.0f));
}

TEST(ColorLUTContractTest, HeaderlessCubeRejectsNonCubicSampleCountsAndRemainderChannels)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    constexpr int pointCounts[] = {7, 9, 15};

    for (const int pointCount : pointCounts) {
        QByteArray contents;
        for (int index = 0; index < pointCount; ++index) {
            contents.append("0.1 0.2 0.3\n");
        }
        const QString path = writeLutFile(
            directory, QStringLiteral("headerless-%1-points.cube").arg(pointCount),
            contents);
        ASSERT_FALSE(path.isEmpty());
        const ColorLUT lut(path);
        SCOPED_TRACE(pointCount);
        EXPECT_FALSE(lut.isValid());
        EXPECT_FALSE(lut.errorMessage().isEmpty());
    }

    QByteArray remainderChannels;
    for (int index = 0; index < 8; ++index) {
        remainderChannels.append("0.1 0.2 0.3\n");
    }
    remainderChannels.append("0.4\n");
    const QString remainderPath = writeLutFile(
        directory, QStringLiteral("headerless-remainder-channel.cube"),
        remainderChannels);
    ASSERT_FALSE(remainderPath.isEmpty());
    const ColorLUT remainderLut(remainderPath);
    EXPECT_FALSE(remainderLut.isValid());
    EXPECT_FALSE(remainderLut.errorMessage().isEmpty());
}

TEST(ColorLUTContractTest, CubeFileRejectsDimensionsOutsideSupportedRange)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    constexpr int dimensions[] = {1, 257};

    for (const int dimension : dimensions) {
        const QString path = writeLutFile(
            directory, QStringLiteral("size-%1.cube").arg(dimension),
            QStringLiteral("LUT_3D_SIZE %1\n").arg(dimension).toUtf8());
        ASSERT_FALSE(path.isEmpty());
        const ColorLUT lut(path);
        SCOPED_TRACE(dimension);
        EXPECT_FALSE(lut.isValid());
        EXPECT_FALSE(lut.errorMessage().isEmpty());
    }
}

TEST(ColorLUTContractTest, LoadDispatchesCubeExtensionWithoutCaseSensitivity)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("fixture.CUBE"),
        "LUT_3D_SIZE 2\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n");
    ASSERT_FALSE(path.isEmpty());

    ColorLUT lut;
    ASSERT_TRUE(lut.load(path)) << lut.errorMessage().toStdString();
    EXPECT_TRUE(lut.isValid());
    EXPECT_EQ(lut.format(), LUTFormat::Cube);
    EXPECT_EQ(lut.filePath(), path);
}

TEST(ColorLUTContractTest, LoadRejectsUnsupportedExtensionWithDiagnostic)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("fixture.look"),
        "LUT_3D_SIZE 2\n");
    ASSERT_FALSE(path.isEmpty());

    ColorLUT lut;
    EXPECT_FALSE(lut.load(path));
    EXPECT_FALSE(lut.isValid());
    EXPECT_NE(lut.errorMessage().indexOf(QStringLiteral("Unknown LUT format")), -1);
}

TEST(ColorLUTContractTest, FailedLoadCanBeFollowedBySuccessfulLoad)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString invalidPath = writeLutFile(
        directory, QStringLiteral("invalid.txt"), "not a LUT\n");
    const QString validPath = writeLutFile(
        directory, QStringLiteral("valid.cube"),
        "LUT_3D_SIZE 2\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n");
    ASSERT_FALSE(invalidPath.isEmpty());
    ASSERT_FALSE(validPath.isEmpty());

    ColorLUT lut = ColorLUT::createIdentity(2);
    ASSERT_FALSE(lut.load(invalidPath));
    EXPECT_FALSE(lut.isValid());
    EXPECT_FALSE(lut.errorMessage().isEmpty());

    ASSERT_TRUE(lut.load(validPath)) << lut.errorMessage().toStdString();
    EXPECT_TRUE(lut.isValid());
    EXPECT_TRUE(lut.errorMessage().isEmpty());
    EXPECT_EQ(lut.format(), LUTFormat::Cube);
    EXPECT_EQ(lut.size().totalPoints(), 8);
    EXPECT_EQ(lut.name(), QStringLiteral("valid"));
}

TEST(ColorLUTContractTest, FailedReloadClearsPreviouslyLoadedSamplesAndMetadata)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString validPath = writeLutFile(
        directory, QStringLiteral("valid-before-failure.cube"),
        "TITLE \"Known Good\"\n"
        "LUT_3D_SIZE 2\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n");
    const QString invalidPath = writeLutFile(
        directory, QStringLiteral("invalid-after-success.cube"),
        "TITLE \"Broken\"\nLUT_3D_SIZE 2\n0 0 0\n");
    ASSERT_FALSE(validPath.isEmpty());
    ASSERT_FALSE(invalidPath.isEmpty());

    ColorLUT lut;
    ASSERT_TRUE(lut.load(validPath)) << lut.errorMessage().toStdString();
    ASSERT_TRUE(lut.isValid());
    EXPECT_EQ(lut.name(), QStringLiteral("Known Good"));
    EXPECT_EQ(lut.filePath(), validPath);
    EXPECT_EQ(lut.format(), LUTFormat::Cube);
    EXPECT_EQ(lut.size().totalPoints(), 8);
    ASSERT_GT(lut.dataSize(), 0u);

    EXPECT_FALSE(lut.load(invalidPath));

    EXPECT_FALSE(lut.isValid());
    EXPECT_EQ(lut.name(), QStringLiteral("Broken"));
    EXPECT_EQ(lut.filePath(), invalidPath);
    EXPECT_EQ(lut.format(), LUTFormat::Cube);
    EXPECT_EQ(lut.size().totalPoints(), 8);
    EXPECT_EQ(lut.dataSize(), 0u);
    EXPECT_FALSE(lut.errorMessage().isEmpty());

    float red = 0.2f;
    float green = 0.4f;
    float blue = 0.6f;
    lut.apply(red, green, blue);
    EXPECT_FLOAT_EQ(red, 0.2f);
    EXPECT_FLOAT_EQ(green, 0.4f);
    EXPECT_FLOAT_EQ(blue, 0.6f);
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

    constexpr int lutSize = 3;
    auto source = ColorLUT::createIdentity(lutSize);
    source.setName(QStringLiteral("Round Trip"));
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                source.setValue(x, y, z, QVector3D(
                    (x * 5 + y * 2 + z) / 16.0f,
                    (x + y * 6 + z * 2) / 18.0f,
                    (x * 3 + y + z * 7) / 22.0f));
            }
        }
    }
    ASSERT_TRUE(source.saveToCube(path));

    QFile serializedFile(path);
    ASSERT_TRUE(serializedFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QList<QByteArray> serializedLines = serializedFile.readAll().split('\n');
    ASSERT_EQ(serializedLines.size(), 4 + lutSize * lutSize * lutSize + 1);
    EXPECT_EQ(serializedLines[0], QByteArray("TITLE \"Round Trip\""));
    EXPECT_EQ(serializedLines[1], QByteArray("# Created by Artifact"));
    EXPECT_EQ(serializedLines[2], QByteArray("LUT_3D_SIZE 3"));
    EXPECT_TRUE(serializedLines[3].isEmpty());
    EXPECT_TRUE(serializedLines.last().isEmpty());
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                const int lineIndex = 4 + z * lutSize * lutSize + y * lutSize + x;
                const QList<QByteArray> channels = serializedLines[lineIndex].split(' ');
                ASSERT_EQ(channels.size(), 3);
                bool redOk = false;
                bool greenOk = false;
                bool blueOk = false;
                const float red = channels[0].toFloat(&redOk);
                const float green = channels[1].toFloat(&greenOk);
                const float blue = channels[2].toFloat(&blueOk);
                ASSERT_TRUE(redOk);
                ASSERT_TRUE(greenOk);
                ASSERT_TRUE(blueOk);
                const QVector3D expected = source.getValue(x, y, z);
                SCOPED_TRACE(::testing::Message()
                    << "serialized coordinate=(" << x << ", " << y << ", " << z << ")");
                EXPECT_NEAR(red, expected.x(), 1e-6f);
                EXPECT_NEAR(green, expected.y(), 1e-6f);
                EXPECT_NEAR(blue, expected.z(), 1e-6f);
            }
        }
    }

    const ColorLUT loaded(path);
    ASSERT_TRUE(loaded.isValid()) << loaded.errorMessage().toStdString();
    EXPECT_EQ(loaded.name(), QStringLiteral("Round Trip"));
    EXPECT_EQ(loaded.format(), LUTFormat::Cube);
    EXPECT_EQ(loaded.size().dimX, lutSize);
    EXPECT_EQ(loaded.size().dimY, lutSize);
    EXPECT_EQ(loaded.size().dimZ, lutSize);
    EXPECT_EQ(loaded.dataSize(), source.dataSize());
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                const QVector3D expected = source.getValue(x, y, z);
                const QVector3D actual = loaded.getValue(x, y, z);
                SCOPED_TRACE(::testing::Message()
                    << "coordinate=(" << x << ", " << y << ", " << z << ")");
                EXPECT_NEAR(actual.x(), expected.x(), 1e-6f);
                EXPECT_NEAR(actual.y(), expected.y(), 1e-6f);
                EXPECT_NEAR(actual.z(), expected.z(), 1e-6f);
            }
        }
    }
}

TEST(ColorLUTContractTest, CubeSaveFailureLeavesSourceLutUnchanged)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    auto source = ColorLUT::createIdentity(2);
    source.setName(QStringLiteral("Save Failure Source"));
    source.setValue(1, 0, 0, QVector3D(0.2f, 0.4f, 0.6f));
    const QVector3D before = source.getValue(1, 0, 0);
    const QString path = directory.filePath(
        QStringLiteral("missing-parent/output.cube"));

    EXPECT_FALSE(source.saveToCube(path));
    EXPECT_TRUE(source.isValid());
    EXPECT_EQ(source.name(), QStringLiteral("Save Failure Source"));
    EXPECT_EQ(source.getValue(1, 0, 0), before);
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
    float centerRed = 0.5f;
    float centerGreen = 0.5f;
    float centerBlue = 0.5f;
    lut.apply(centerRed, centerGreen, centerBlue);
    const QVector3D center(centerRed, centerGreen, centerBlue);
    EXPECT_NEAR(center.x(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.y(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.z(), 0.5f, 1e-6f);
}

TEST(ColorLUTContractTest, CspPreservesFiniteOutOfRangeSamplesAndApplyClamps)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("extended-range.csp"),
        "CSPLUTV100\nLUT_3D_SIZE 2\nBEGIN DATA\n"
        "-0.5 0.25 1.5\n-0.5 0.25 1.5\n"
        "-0.5 0.25 1.5\n-0.5 0.25 1.5\n"
        "-0.5 0.25 1.5\n-0.5 0.25 1.5\n"
        "-0.5 0.25 1.5\n-0.5 0.25 1.5\nEND DATA\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    const QVector3D stored = lut.getValue(1, 1, 1);
    EXPECT_FLOAT_EQ(stored.x(), -0.5f);
    EXPECT_FLOAT_EQ(stored.y(), 0.25f);
    EXPECT_FLOAT_EQ(stored.z(), 1.5f);

    float red = 0.4f;
    float green = 0.5f;
    float blue = 0.6f;
    lut.apply(red, green, blue);
    EXPECT_FLOAT_EQ(red, 0.0f);
    EXPECT_FLOAT_EQ(green, 0.25f);
    EXPECT_FLOAT_EQ(blue, 1.0f);
}

TEST(ColorLUTContractTest, CspInfersCubicSizeWhenHeaderIsMissing)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(
        directory, QStringLiteral("headerless.csp"),
        "CSPLUTV100\nBEGIN DATA\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\nEND DATA\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.size().dimX, 2);
    EXPECT_EQ(lut.size().dimY, 2);
    EXPECT_EQ(lut.size().dimZ, 2);
    EXPECT_EQ(lut.getValue(1, 1, 1), QVector3D(1.0f, 1.0f, 1.0f));
}

TEST(ColorLUTContractTest, CspIgnoresSamplesAfterEndDataMarker)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(
        directory, QStringLiteral("bounded-data.csp"),
        "CSPLUTV100\nLUT_3D_SIZE 2\nBEGIN DATA\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\nEND DATA\n"
        "0.75 0.75 0.75\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.getValue(0, 0, 0), QVector3D(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(lut.getValue(1, 1, 1), QVector3D(1.0f, 1.0f, 1.0f));
}

TEST(ColorLUTContractTest, CspSkipsRowsThatDoNotContainExactlyThreeValues)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(
        directory, QStringLiteral("extra-fields.csp"),
        "LUT_3D_SIZE 2\nBEGIN DATA\n"
        "header note\nDOMAIN_MIN 0 0 0\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\nEND DATA\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.getValue(0, 0, 0), QVector3D(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(lut.getValue(1, 1, 1), QVector3D(1.0f, 1.0f, 1.0f));
}

TEST(ColorLUTContractTest, CspRejectsNonNumericThreeValueSample)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(
        directory, QStringLiteral("invalid-sample.csp"),
        "LUT_3D_SIZE 2\nBEGIN DATA\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\ninvalid 1 1\nEND DATA\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    EXPECT_FALSE(lut.isValid());
    EXPECT_NE(lut.errorMessage().indexOf(QStringLiteral("Invalid CSP LUT sample")), -1);
}

TEST(ColorLUTContractTest, ReusingLutAcrossFormatsRefreshesMetadataAndSamples)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString cubePath = writeLutFile(
        directory, QStringLiteral("first.cube"),
        "TITLE \"First Grade\"\nLUT_3D_SIZE 2\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n");
    const QString cspPath = writeLutFile(
        directory, QStringLiteral("second.csp"),
        "LUT_3D_SIZE 2\nBEGIN DATA\n"
        "0 0 0\n0.5 0 0\n0 0.5 0\n0.5 0.5 0\n"
        "0 0 0.5\n0.5 0 0.5\n0 0.5 0.5\n0.5 0.5 0.5\n"
        "END DATA\n");
    ASSERT_FALSE(cubePath.isEmpty());
    ASSERT_FALSE(cspPath.isEmpty());

    ColorLUT lut;
    ASSERT_TRUE(lut.load(cubePath)) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.name(), QStringLiteral("First Grade"));
    ASSERT_TRUE(lut.load(cspPath)) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.format(), LUTFormat::Csp);
    EXPECT_EQ(lut.name(), QStringLiteral("second"));
    EXPECT_EQ(lut.filePath(), cspPath);
    EXPECT_TRUE(lut.errorMessage().isEmpty());
    EXPECT_EQ(lut.getValue(1, 1, 1), QVector3D(0.5f, 0.5f, 0.5f));
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

TEST(ColorLUTContractTest, CspRejectsInvalidSizesAndSampleCounts)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    constexpr struct Case { const char* name; const char* contents; } cases[] = {
        {"undersized", "LUT_3D_SIZE 1\nBEGIN DATA\nEND DATA\n"},
        {"oversized", "LUT_3D_SIZE 257\nBEGIN DATA\nEND DATA\n"},
        {"malformed-size", "LUT_3D_SIZE nope\nBEGIN DATA\nEND DATA\n"},
        {"incomplete", "LUT_3D_SIZE 2\nBEGIN DATA\n"
                        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
                        "0 0 1\n1 0 1\n0 1 1\nEND DATA\n"},
        {"extra", "LUT_3D_SIZE 2\nBEGIN DATA\n"
                   "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
                   "0 0 1\n1 0 1\n0 1 1\n1 1 1\n0.5 0.5 0.5\nEND DATA\n"},
    };

    for (const auto& testCase : cases) {
        const QString path = writeLutFile(
            directory, QString::fromLatin1(testCase.name) + QStringLiteral(".csp"),
            QByteArray(testCase.contents));
        ASSERT_FALSE(path.isEmpty());
        const ColorLUT lut(path);
        SCOPED_TRACE(testCase.name);
        EXPECT_FALSE(lut.isValid());
        EXPECT_FALSE(lut.errorMessage().isEmpty());
    }
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
    float centerRed = 0.5f;
    float centerGreen = 0.5f;
    float centerBlue = 0.5f;
    lut.apply(centerRed, centerGreen, centerBlue);
    const QVector3D center(centerRed, centerGreen, centerBlue);
    EXPECT_NEAR(center.x(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.y(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.z(), 0.5f, 1e-6f);
}

TEST(ColorLUTContractTest, ThreeDlNormalizesSamplesByTheObservedMaximum)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(
        directory, QStringLiteral("scaled.3dl"),
        "2\n"
        "0 0 0\n8 0 0\n0 4 0\n8 4 0\n"
        "0 0 2\n8 0 2\n0 4 2\n8 4 8\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.getValue(1, 0, 0), QVector3D(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(lut.getValue(0, 1, 0), QVector3D(0.0f, 0.5f, 0.0f));
    EXPECT_EQ(lut.getValue(0, 0, 1), QVector3D(0.0f, 0.0f, 0.25f));
    EXPECT_EQ(lut.getValue(1, 1, 1), QVector3D(1.0f, 0.5f, 1.0f));
}

TEST(ColorLUTContractTest, ThreeDlLeavesUnitRangeSamplesUnscaled)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("unit-range.3dl"),
        "2\n"
        "0 0 0\n0.5 0 0\n0 0.25 0\n0.5 0.25 0\n"
        "0 0 0.75\n0.5 0 0.75\n0 0.25 0.75\n0.5 0.25 1\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.format(), LUTFormat::_3dl);
    EXPECT_EQ(lut.getValue(1, 0, 0), QVector3D(0.5f, 0.0f, 0.0f));
    EXPECT_EQ(lut.getValue(0, 1, 0), QVector3D(0.0f, 0.25f, 0.0f));
    EXPECT_EQ(lut.getValue(0, 0, 1), QVector3D(0.0f, 0.0f, 0.75f));
    EXPECT_EQ(lut.getValue(1, 1, 1), QVector3D(0.5f, 0.25f, 1.0f));
}

TEST(ColorLUTContractTest, ThreeDlAcceptsAllZeroBlackLut)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("black.3dl"),
        "2\n"
        "0 0 0\n0 0 0\n0 0 0\n0 0 0\n"
        "0 0 0\n0 0 0\n0 0 0\n0 0 0\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.format(), LUTFormat::_3dl);
    EXPECT_EQ(lut.size().totalPoints(), 8);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                EXPECT_EQ(lut.getValue(x, y, z), QVector3D(0.0f, 0.0f, 0.0f));
            }
        }
    }
    float red = 0.4f;
    float green = 0.5f;
    float blue = 0.6f;
    lut.apply(red, green, blue);
    EXPECT_FLOAT_EQ(red, 0.0f);
    EXPECT_FLOAT_EQ(green, 0.0f);
    EXPECT_FLOAT_EQ(blue, 0.0f);
}

TEST(ColorLUTContractTest, ThreeDlInfersCubicSizeWhenHeaderIsMissing)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(directory, QStringLiteral("headerless.3dl"),
        "0 0 0\n16 0 0\n0 16 0\n16 16 0\n"
        "0 0 16\n16 0 16\n0 16 16\n16 16 16\n");
    ASSERT_FALSE(path.isEmpty());

    const ColorLUT lut(path);
    ASSERT_TRUE(lut.isValid()) << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.size().dimX, 2);
    EXPECT_EQ(lut.size().dimY, 2);
    EXPECT_EQ(lut.size().dimZ, 2);
}

TEST(ColorLUTContractTest, ThreeDlRejectsDimensionsOutsideSupportedRange)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    constexpr int dimensions[] = {1, 257};

    for (const int dimension : dimensions) {
        const QString path = writeLutFile(
            directory, QStringLiteral("size-%1.3dl").arg(dimension),
            QStringLiteral("%1\n").arg(dimension).toUtf8());
        ASSERT_FALSE(path.isEmpty());
        const ColorLUT lut(path);
        SCOPED_TRACE(dimension);
        EXPECT_FALSE(lut.isValid());
        EXPECT_FALSE(lut.errorMessage().isEmpty());
    }
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

TEST(ColorLUTContractTest, ThreeDlRejectsNonFiniteSamples)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeLutFile(
        directory, QStringLiteral("non-finite.3dl"),
        "2\n"
        "0 0 0\n8 0 0\n0 4 0\n8 4 0\n"
        "0 0 2\n8 0 2\n0 4 2\n8 4 inf\n");
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

TEST(ColorLUTContractTest, WithIntensityReturnsIndependentCopyAndPreservesMetadata)
{
    constexpr int lutSize = 3;
    auto source = ColorLUT::createIdentity(lutSize);
    source.setName(QStringLiteral("Intensity Source"));
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                source.setValue(x, y, z, QVector3D(
                    (x + 2 * y + z) / 8.0f,
                    (2 * x + y + z) / 8.0f,
                    (x + y + 2 * z) / 8.0f));
            }
        }
    }
    const ColorLUT original = source;
    constexpr float intensity = 0.4f;
    ColorLUT adjusted = source.withIntensity(intensity);

    ASSERT_TRUE(adjusted.isValid());
    EXPECT_EQ(adjusted.name(), source.name());
    EXPECT_EQ(adjusted.filePath(), source.filePath());
    EXPECT_EQ(adjusted.format(), source.format());
    EXPECT_EQ(adjusted.size().dimX, lutSize);
    EXPECT_EQ(adjusted.size().dimY, lutSize);
    EXPECT_EQ(adjusted.size().dimZ, lutSize);
    EXPECT_EQ(adjusted.dataSize(), source.dataSize());
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                const QVector3D sourceValue = original.getValue(x, y, z);
                const QVector3D identityValue(x / 2.0f, y / 2.0f, z / 2.0f);
                const QVector3D expected = identityValue * (1.0f - intensity)
                    + sourceValue * intensity;
                const QVector3D actual = adjusted.getValue(x, y, z);
                EXPECT_NEAR(actual.x(), expected.x(), 1e-6f);
                EXPECT_NEAR(actual.y(), expected.y(), 1e-6f);
                EXPECT_NEAR(actual.z(), expected.z(), 1e-6f);
            }
        }
    }

    source.setValue(1, 1, 1, QVector3D(0.9f, 0.8f, 0.7f));
    const QVector3D stillAdjusted = adjusted.getValue(1, 1, 1);
    const QVector3D originalCenter = original.getValue(1, 1, 1);
    EXPECT_NEAR(stillAdjusted.x(), originalCenter.x() * intensity + 0.5f * (1.0f - intensity), 1e-6f);
    EXPECT_NEAR(stillAdjusted.y(), originalCenter.y() * intensity + 0.5f * (1.0f - intensity), 1e-6f);
    EXPECT_NEAR(stillAdjusted.z(), originalCenter.z() * intensity + 0.5f * (1.0f - intensity), 1e-6f);

    const QVector3D adjustedBeforeEdit = adjusted.getValue(0, 2, 1);
    adjusted.setValue(0, 2, 1, QVector3D(0.11f, 0.22f, 0.33f));
    EXPECT_EQ(source.getValue(0, 2, 1), original.getValue(0, 2, 1));
    EXPECT_EQ(adjusted.getValue(0, 2, 1), QVector3D(0.11f, 0.22f, 0.33f));
    EXPECT_NE(adjustedBeforeEdit, adjusted.getValue(0, 2, 1));
}

TEST(ColorLUTContractTest, WithIntensityMapsEveryNonFiniteValueToFullLut)
{
    auto source = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                source.setValue(x, y, z, QVector3D(1.0f - x, y, z));
            }
        }
    }

    const float infinity = std::numeric_limits<float>::infinity();
    const ColorLUT nanIntensity = source.withIntensity(
        std::numeric_limits<float>::quiet_NaN());
    const ColorLUT positiveInfinity = source.withIntensity(infinity);
    const ColorLUT negativeInfinity = source.withIntensity(-infinity);

    EXPECT_EQ(nanIntensity.getValue(1, 0, 0), QVector3D(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(positiveInfinity.getValue(1, 0, 0), QVector3D(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(negativeInfinity.getValue(1, 0, 0), QVector3D(0.0f, 0.0f, 0.0f));
}

TEST(ColorLUTContractTest, WithIntensityClampsFiniteValuesOutsideUnitInterval)
{
    constexpr int gridSize = 3;
    auto source = ColorLUT::createIdentity(gridSize);
    for (int z = 0; z < gridSize; ++z) {
        for (int y = 0; y < gridSize; ++y) {
            for (int x = 0; x < gridSize; ++x) {
                source.setValue(x, y, z, QVector3D(
                    1.0f - x / float(gridSize - 1),
                    y / float(gridSize - 1) * 0.75f,
                    z / float(gridSize - 1) * 0.5f));
            }
        }
    }

    const ColorLUT negative = source.withIntensity(-0.25f);
    const ColorLUT aboveOne = source.withIntensity(1.25f);
    for (int z = 0; z < gridSize; ++z) {
        for (int y = 0; y < gridSize; ++y) {
            for (int x = 0; x < gridSize; ++x) {
                const QVector3D identityValue(
                    x / float(gridSize - 1),
                    y / float(gridSize - 1),
                    z / float(gridSize - 1));
                EXPECT_EQ(negative.getValue(x, y, z), identityValue);
                EXPECT_EQ(aboveOne.getValue(x, y, z), source.getValue(x, y, z));
            }
        }
    }
    EXPECT_EQ(source.getValue(2, 0, 2), QVector3D(0.0f, 0.0f, 0.5f));
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
    float resultRed = 0.2f;
    float resultGreen = 0.4f;
    float resultBlue = 0.6f;
    combined.apply(resultRed, resultGreen, resultBlue);
    const QVector3D result(resultRed, resultGreen, resultBlue);
    EXPECT_NEAR(result.x(), 0.8f, 1e-6f);
    EXPECT_NEAR(result.y(), 0.6f, 1e-6f);
    EXPECT_NEAR(result.z(), 0.6f, 1e-6f);
}

TEST(ColorLUTContractTest, CombineAppliesTheReceiverBeforeTheArgument)
{
    auto first = ColorLUT::createIdentity(3);
    auto second = ColorLUT::createIdentity(3);
    for (int z = 0; z < 3; ++z) {
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                const float red = x / 2.0f;
                first.setValue(x, y, z, QVector3D(0.5f * red + 0.1f,
                                                   y / 2.0f,
                                                   z / 2.0f));
                second.setValue(x, y, z, QVector3D(0.5f * red + 0.2f,
                                                    y / 2.0f,
                                                    z / 2.0f));
            }
        }
    }

    const ColorLUT combined = first.combine(second);
    float red = 0.4f;
    float green = 0.3f;
    float blue = 0.7f;
    combined.apply(red, green, blue);

    // second(first(0.4)) = 0.5 * (0.5 * 0.4 + 0.1) + 0.2.
    EXPECT_NEAR(red, 0.35f, 1e-6f);
    EXPECT_NEAR(green, 0.3f, 1e-6f);
    EXPECT_NEAR(blue, 0.7f, 1e-6f);
}

TEST(ColorLUTContractTest, IdentityLUTIsNeutralOnEitherSideOfCombine)
{
    constexpr int lutSize = 3;
    auto source = ColorLUT::createIdentity(lutSize);
    source.setName(QStringLiteral("Nonlinear Source"));
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                source.setValue(x, y, z, QVector3D(
                    (x * x + y + z) / 8.0f,
                    (x + y * y + z) / 8.0f,
                    (x + y + z * z) / 8.0f));
            }
        }
    }
    const auto identity = ColorLUT::createIdentity(lutSize);
    const ColorLUT identityThenSource = identity.combine(source);
    const ColorLUT sourceThenIdentity = source.combine(identity);

    ASSERT_TRUE(identityThenSource.isValid());
    ASSERT_TRUE(sourceThenIdentity.isValid());
    EXPECT_EQ(identityThenSource.size().totalPoints(), source.size().totalPoints());
    EXPECT_EQ(sourceThenIdentity.size().totalPoints(), source.size().totalPoints());
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                const QVector3D expected = source.getValue(x, y, z);
                const QVector3D leftIdentity = identityThenSource.getValue(x, y, z);
                const QVector3D rightIdentity = sourceThenIdentity.getValue(x, y, z);
                SCOPED_TRACE(::testing::Message()
                    << "coordinate=(" << x << ", " << y << ", " << z << ")");
                EXPECT_NEAR(leftIdentity.x(), expected.x(), 1e-6f);
                EXPECT_NEAR(leftIdentity.y(), expected.y(), 1e-6f);
                EXPECT_NEAR(leftIdentity.z(), expected.z(), 1e-6f);
                EXPECT_NEAR(rightIdentity.x(), expected.x(), 1e-6f);
                EXPECT_NEAR(rightIdentity.y(), expected.y(), 1e-6f);
                EXPECT_NEAR(rightIdentity.z(), expected.z(), 1e-6f);
            }
        }
    }
    EXPECT_EQ(source.name(), QStringLiteral("Nonlinear Source"));
    EXPECT_NEAR(source.getValue(2, 1, 0).x(), 0.625f, 1e-6f);
}

TEST(ColorLUTContractTest, CombineUsesReceiverResolutionForDifferentGridSizes)
{
    auto fine = ColorLUT::createIdentity(3);
    for (int z = 0; z < 3; ++z) {
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                fine.setValue(x, y, z, QVector3D(
                    (x * 3 + y + z) / 10.0f,
                    (x + y * 3 + z) / 10.0f,
                    (x + y + z * 3) / 10.0f));
            }
        }
    }
    auto coarse = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                coarse.setValue(x, y, z, QVector3D(
                    0.1f + 0.6f * y,
                    0.2f + 0.5f * z,
                    0.15f + 0.7f * x));
            }
        }
    }

    const auto expectCompositionOnReceiverGrid = [](const ColorLUT& receiver,
                                                     const ColorLUT& argument,
                                                     const ColorLUT& combined,
                                                     int receiverSize) {
        ASSERT_TRUE(combined.isValid());
        EXPECT_EQ(combined.size().dimX, receiverSize);
        EXPECT_EQ(combined.size().dimY, receiverSize);
        EXPECT_EQ(combined.size().dimZ, receiverSize);
        for (int z = 0; z < receiverSize; ++z) {
            for (int y = 0; y < receiverSize; ++y) {
                for (int x = 0; x < receiverSize; ++x) {
                    const QVector3D input = receiver.getValue(x, y, z);
                    float red = input.x();
                    float green = input.y();
                    float blue = input.z();
                    argument.apply(red, green, blue);
                    const QVector3D actual = combined.getValue(x, y, z);
                    SCOPED_TRACE(::testing::Message()
                        << "coordinate=(" << x << ", " << y << ", " << z
                        << "), receiverSize=" << receiverSize);
                    EXPECT_NEAR(actual.x(), red, 1e-6f);
                    EXPECT_NEAR(actual.y(), green, 1e-6f);
                    EXPECT_NEAR(actual.z(), blue, 1e-6f);
                }
            }
        }
    };

    const ColorLUT fineThenCoarse = fine.combine(coarse);
    expectCompositionOnReceiverGrid(fine, coarse, fineThenCoarse, 3);

    const ColorLUT coarseThenFine = coarse.combine(fine);
    expectCompositionOnReceiverGrid(coarse, fine, coarseThenFine, 2);
}

TEST(ColorLUTContractTest, CombinePreservesReceiverWhenEitherOperandIsInvalid)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString invalidPath = writeLutFile(directory,
        QStringLiteral("invalid-combine.cube"), "LUT_3D_SIZE 2\n0 0 0\n");
    ASSERT_FALSE(invalidPath.isEmpty());
    const ColorLUT invalid(invalidPath);
    ASSERT_FALSE(invalid.isValid());

    auto valid = ColorLUT::createIdentity(2);
    valid.setName(QStringLiteral("Valid Receiver"));
    valid.setValue(1, 0, 1, QVector3D(0.2f, 0.4f, 0.6f));
    const ColorLUT validBefore = valid;

    const ColorLUT validWithInvalidArgument = valid.combine(invalid);
    EXPECT_TRUE(validWithInvalidArgument.isValid());
    EXPECT_EQ(validWithInvalidArgument.name(), validBefore.name());
    EXPECT_EQ(validWithInvalidArgument.getValue(1, 0, 1),
              validBefore.getValue(1, 0, 1));

    const ColorLUT invalidWithValidArgument = invalid.combine(valid);
    EXPECT_FALSE(invalidWithValidArgument.isValid());
    EXPECT_EQ(invalidWithValidArgument.dataSize(), 0u);
    EXPECT_EQ(invalidWithValidArgument.filePath(), invalid.filePath());
    EXPECT_EQ(valid.getValue(1, 0, 1), QVector3D(0.2f, 0.4f, 0.6f));
}

TEST(ColorLUTContractTest, InvertedLUTProvidesAValidApproximationForConstantMapping)
{
    auto source = ColorLUT::createIdentity(3);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                source.setValue(x, y, z, QVector3D(0.25f, 0.5f, 0.75f));
            }
        }
    }

    const ColorLUT inverse = source.inverted();
    ASSERT_TRUE(inverse.isValid());
    float resultRed = 0.5f;
    float resultGreen = 0.5f;
    float resultBlue = 0.5f;
    inverse.apply(resultRed, resultGreen, resultBlue);
    const QVector3D result(resultRed, resultGreen, resultBlue);
    EXPECT_TRUE(std::isfinite(result.x()));
    EXPECT_TRUE(std::isfinite(result.y()));
    EXPECT_TRUE(std::isfinite(result.z()));
    EXPECT_GE(result.x(), 0.0f);
    EXPECT_LE(result.x(), 1.0f);
    EXPECT_GE(result.y(), 0.0f);
    EXPECT_LE(result.y(), 1.0f);
    EXPECT_GE(result.z(), 0.0f);
    EXPECT_LE(result.z(), 1.0f);
}

TEST(ColorLUTContractTest, InvertedMonotonicLinearCurveApproximatelyRoundTripsInteriorValues)
{
    constexpr int lutSize = 5;
    auto source = ColorLUT::createIdentity(lutSize);
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                const float red = x / float(lutSize - 1);
                const float green = y / float(lutSize - 1);
                const float blue = z / float(lutSize - 1);
                source.setValue(x, y, z, QVector3D(
                    0.75f * red + 0.1f,
                    0.8f * green + 0.1f,
                    0.7f * blue + 0.15f));
            }
        }
    }

    const ColorLUT inverse = source.inverted();
    constexpr QVector3D samples[] = {
        QVector3D(0.2f, 0.3f, 0.4f),
        QVector3D(0.4f, 0.5f, 0.6f),
        QVector3D(0.7f, 0.2f, 0.8f),
    };
    for (const QVector3D& sample : samples) {
        float mappedRed = sample.x();
        float mappedGreen = sample.y();
        float mappedBlue = sample.z();
        source.apply(mappedRed, mappedGreen, mappedBlue);
        inverse.apply(mappedRed, mappedGreen, mappedBlue);

        EXPECT_NEAR(mappedRed, sample.x(), 0.01f);
        EXPECT_NEAR(mappedGreen, sample.y(), 0.01f);
        EXPECT_NEAR(mappedBlue, sample.z(), 0.01f);
    }

}

TEST(ColorLUTContractTest, InvertedMonotonicFullRangeCurveRoundTripsBoundariesAndInterior)
{
    constexpr int lutSize = 17;
    auto source = ColorLUT::createIdentity(lutSize);
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                const float red = x / float(lutSize - 1);
                const float green = y / float(lutSize - 1);
                const float blue = z / float(lutSize - 1);
                source.setValue(x, y, z, QVector3D(
                    0.8f * red + 0.2f * red * red,
                    0.9f * green + 0.1f * green * green,
                    0.7f * blue + 0.3f * blue * blue));
            }
        }
    }

    const ColorLUT inverse = source.inverted();
    ASSERT_TRUE(inverse.isValid());
    constexpr int sampleCount = 9;
    for (int z = 0; z < sampleCount; ++z) {
        for (int y = 0; y < sampleCount; ++y) {
            for (int x = 0; x < sampleCount; ++x) {
                const QVector3D sample(
                    x / float(sampleCount - 1),
                    y / float(sampleCount - 1),
                    z / float(sampleCount - 1));
                float mappedRed = sample.x();
                float mappedGreen = sample.y();
                float mappedBlue = sample.z();
                source.apply(mappedRed, mappedGreen, mappedBlue);
                inverse.apply(mappedRed, mappedGreen, mappedBlue);
                SCOPED_TRACE(::testing::Message()
                    << "sourceSample=(" << x << ", " << y << ", " << z << ")");
                EXPECT_NEAR(mappedRed, sample.x(), 0.01f);
                EXPECT_NEAR(mappedGreen, sample.y(), 0.01f);
                EXPECT_NEAR(mappedBlue, sample.z(), 0.01f);
            }
        }
    }
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
    float centerRed = 0.5f;
    float centerGreen = 0.5f;
    float centerBlue = 0.5f;
    lut.apply(centerRed, centerGreen, centerBlue);
    const QVector3D center(centerRed, centerGreen, centerBlue);
    EXPECT_NEAR(center.x(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.y(), 0.5f, 1e-6f);
    EXPECT_NEAR(center.z(), 0.5f, 1e-6f);
}

TEST(ColorLUTContractTest, ImageImportReadsVerticalTilesAndLeavesArgbSourceUntouched)
{
    constexpr int lutSize = 2;
    QImage image(lutSize, lutSize * lutSize, QImage::Format_ARGB32);
    ASSERT_FALSE(image.isNull());
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                const int px = x;
                const int py = y + z * lutSize;
                image.setPixel(px, py,
                    qRgba(x * 255, y * 255, z * 255, 47 + 83 * z + 29 * y + x));
            }
        }
    }
    const QImage original = image.copy();

    ColorLUT lut;
    ASSERT_TRUE(lut.loadFromImage(image, lutSize))
        << lut.errorMessage().toStdString();
    EXPECT_EQ(lut.format(), LUTFormat::PNG);
    EXPECT_EQ(lut.getValue(1, 0, 0), QVector3D(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(lut.getValue(0, 1, 0), QVector3D(0.0f, 1.0f, 0.0f));
    EXPECT_EQ(lut.getValue(0, 0, 1), QVector3D(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(lut.getValue(1, 1, 1), QVector3D(1.0f, 1.0f, 1.0f));
    EXPECT_EQ(image, original);
}

TEST(ColorLUTContractTest, ImageImportReadsAllSlicesFromRectangularPartialTileAtlas)
{
    constexpr int lutSize = 3;
    constexpr int tilesX = 2;
    QImage image(lutSize * tilesX, lutSize * 2, QImage::Format_ARGB32);
    ASSERT_FALSE(image.isNull());
    image.fill(qRgba(255, 0, 255, 17));

    const auto expectedPixel = [](const int x, const int y, const int z) {
        return std::array<int, 3>{
            (x * 71 + z * 29 + y * 11) % 256,
            (y * 83 + z * 37 + x * 7) % 256,
            (z * 101 + y * 13 + x * 5) % 256,
        };
    };
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                const int px = x + (z % tilesX) * lutSize;
                const int py = y + (z / tilesX) * lutSize;
                const auto rgb = expectedPixel(x, y, z);
                image.setPixel(px, py,
                    qRgba(rgb[0], rgb[1], rgb[2], 31 + 53 * z + 19 * y + x));
            }
        }
    }
    const QImage original = image.copy();

    ColorLUT lut;
    ASSERT_TRUE(lut.loadFromImage(image, lutSize))
        << lut.errorMessage().toStdString();
    ASSERT_TRUE(lut.isValid());
    EXPECT_EQ(lut.size().dimX, lutSize);
    EXPECT_EQ(lut.size().dimY, lutSize);
    EXPECT_EQ(lut.size().dimZ, lutSize);
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                const auto rgb = expectedPixel(x, y, z);
                const QVector3D actual = lut.getValue(x, y, z);
                SCOPED_TRACE(::testing::Message()
                    << "grid=" << x << ',' << y << ',' << z);
                EXPECT_FLOAT_EQ(actual.x(), rgb[0] / 255.0f);
                EXPECT_FLOAT_EQ(actual.y(), rgb[1] / 255.0f);
                EXPECT_FLOAT_EQ(actual.z(), rgb[2] / 255.0f);
            }
        }
    }
    EXPECT_EQ(image, original);
}

TEST(ColorLUTContractTest, ImageImportRejectsInsufficientTileArea)
{
    const QImage image(3, 3, QImage::Format_RGB32);
    ColorLUT lut;

    EXPECT_FALSE(lut.loadFromImage(image, 2));
    EXPECT_FALSE(lut.errorMessage().isEmpty());
}

TEST(ColorLUTContractTest, ImageImportInvalidArgumentsPreserveExistingLutData)
{
    auto lut = ColorLUT::createIdentity(2);
    lut.setName(QStringLiteral("Keep This LUT"));
    const QImage validImage(4, 4, QImage::Format_RGB32);
    ASSERT_FALSE(validImage.isNull());
    ASSERT_TRUE(lut.loadFromImage(validImage, 2))
        << lut.errorMessage().toStdString();
    lut.setName(QStringLiteral("Keep This LUT"));
    const QVector3D savedSample = lut.getValue(1, 0, 0);
    const LUTSize savedSize = lut.size();
    ASSERT_TRUE(lut.isValid());
    ASSERT_GT(lut.dataSize(), 0u);

    const QImage nullImage;
    EXPECT_FALSE(lut.loadFromImage(nullImage, 2));
    EXPECT_TRUE(lut.isValid());
    EXPECT_EQ(lut.name(), QStringLiteral("Keep This LUT"));
    EXPECT_EQ(lut.size().dimX, savedSize.dimX);
    EXPECT_EQ(lut.size().dimY, savedSize.dimY);
    EXPECT_EQ(lut.size().dimZ, savedSize.dimZ);
    EXPECT_EQ(lut.dataSize(), static_cast<size_t>(savedSize.totalPoints()) * 3u * sizeof(float));
    EXPECT_EQ(lut.getValue(1, 0, 0), savedSample);
    EXPECT_FALSE(lut.errorMessage().isEmpty());

    const QImage nonNullImage(4, 4, QImage::Format_RGB32);
    for (const int invalidSize : {1, 257}) {
        SCOPED_TRACE(invalidSize);
        EXPECT_FALSE(lut.loadFromImage(nonNullImage, invalidSize));
        EXPECT_TRUE(lut.isValid());
        EXPECT_EQ(lut.name(), QStringLiteral("Keep This LUT"));
        EXPECT_EQ(lut.getValue(1, 0, 0), savedSample);
        EXPECT_EQ(lut.size().dimX, savedSize.dimX);
        EXPECT_EQ(lut.size().dimY, savedSize.dimY);
        EXPECT_EQ(lut.size().dimZ, savedSize.dimZ);
    }
}

TEST(ColorLUTContractTest, InsufficientImageTilesInvalidateExistingLutAndRecordRequestedSize)
{
    auto lut = ColorLUT::createIdentity(2);
    lut.setName(QStringLiteral("Before Small Image"));
    const QVector3D previousSample = lut.getValue(1, 0, 0);
    const size_t previousDataSize = lut.dataSize();
    ASSERT_TRUE(lut.isValid());

    QImage insufficientImage(5, 4, QImage::Format_ARGB32);
    ASSERT_FALSE(insufficientImage.isNull());
    insufficientImage.fill(qRgba(23, 91, 177, 61));
    const QImage originalImage = insufficientImage.copy();

    EXPECT_FALSE(lut.loadFromImage(insufficientImage, 3));

    EXPECT_FALSE(lut.isValid());
    EXPECT_EQ(lut.name(), QStringLiteral("Before Small Image"));
    EXPECT_EQ(lut.format(), LUTFormat::PNG);
    EXPECT_EQ(lut.size().dimX, 3);
    EXPECT_EQ(lut.size().dimY, 3);
    EXPECT_EQ(lut.size().dimZ, 3);
    EXPECT_EQ(lut.dataSize(), 0u);
    EXPECT_FALSE(lut.errorMessage().isEmpty());
    EXPECT_NE(previousDataSize, 0u);
    EXPECT_EQ(lut.getValue(1, 0, 0), QVector3D());
    EXPECT_NE(previousSample, QVector3D());
    EXPECT_EQ(insufficientImage, originalImage);
}

TEST(ColorLUTContractTest, FailedHaldFileLoadPreservesExistingLutData)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString invalidImagePath = directory.filePath(QStringLiteral("missing-hald.png"));

    auto lut = ColorLUT::createIdentity(2);
    lut.setName(QStringLiteral("Existing Identity"));
    const QVector3D savedSample = lut.getValue(1, 0, 0);
    const LUTSize savedSize = lut.size();
    const LUTFormat savedFormat = lut.format();
    const size_t savedDataSize = lut.dataSize();
    ASSERT_TRUE(lut.isValid());

    EXPECT_FALSE(lut.loadFromHaldCLUT(invalidImagePath));

    EXPECT_TRUE(lut.isValid());
    EXPECT_EQ(lut.name(), QStringLiteral("Existing Identity"));
    EXPECT_EQ(lut.size().dimX, savedSize.dimX);
    EXPECT_EQ(lut.size().dimY, savedSize.dimY);
    EXPECT_EQ(lut.size().dimZ, savedSize.dimZ);
    EXPECT_EQ(lut.format(), savedFormat);
    EXPECT_EQ(lut.dataSize(), savedDataSize);
    EXPECT_EQ(lut.getValue(1, 0, 0), savedSample);
    EXPECT_FALSE(lut.errorMessage().isEmpty());
}

TEST(ColorLUTContractTest, HaldFileLoadMatchesInMemoryImageImport)
{
    constexpr int lutSize = 33;
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("hald-reference.png"));

    QImage image(lutSize * lutSize, lutSize * lutSize, QImage::Format_ARGB32);
    ASSERT_FALSE(image.isNull());
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                const int px = x + (z % lutSize) * lutSize;
                const int py = y + (z / lutSize) * lutSize;
                const int red = (x * 17 + y * 3 + z * 5) % 256;
                const int green = (x * 7 + y * 19 + z * 11) % 256;
                const int blue = (x * 13 + y * 2 + z * 23) % 256;
                const int alpha = (x * 5 + y * 9 + z * 15) % 256;
                image.setPixel(px, py, qRgba(red, green, blue, alpha));
            }
        }
    }
    ASSERT_TRUE(image.save(imagePath, "PNG"));

    ColorLUT fromImage;
    ASSERT_TRUE(fromImage.loadFromImage(image, lutSize))
        << fromImage.errorMessage().toStdString();
    ColorLUT fromFile;
    ASSERT_TRUE(fromFile.loadFromHaldCLUT(imagePath))
        << fromFile.errorMessage().toStdString();

    EXPECT_TRUE(fromFile.isValid());
    EXPECT_EQ(fromFile.format(), LUTFormat::PNG);
    EXPECT_EQ(fromFile.size().dimX, lutSize);
    EXPECT_EQ(fromFile.size().dimY, lutSize);
    EXPECT_EQ(fromFile.size().dimZ, lutSize);
    EXPECT_EQ(fromFile.dataSize(), fromImage.dataSize());
    EXPECT_EQ(fromFile.name(), QStringLiteral("Identity"));
    EXPECT_EQ(fromFile.filePath(), QString());
    EXPECT_TRUE(fromFile.errorMessage().isEmpty());

    constexpr int coordinates[][3] = {
        {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
        {16, 16, 16}, {32, 7, 21}, {11, 32, 4}, {32, 32, 32},
    };
    for (const auto& coordinate : coordinates) {
        const QVector3D imageValue = fromImage.getValue(
            coordinate[0], coordinate[1], coordinate[2]);
        const QVector3D fileValue = fromFile.getValue(
            coordinate[0], coordinate[1], coordinate[2]);
        SCOPED_TRACE(::testing::Message()
            << "coordinate=(" << coordinate[0] << ", "
            << coordinate[1] << ", " << coordinate[2] << ")");
        EXPECT_EQ(fileValue, imageValue);
    }

    EXPECT_EQ(image.pixelColor(17, 29),
              QColor((17 * 17 + 29 * 3) % 256,
                     (17 * 7 + 29 * 19) % 256,
                     (17 * 13 + 29 * 2) % 256,
                     (17 * 5 + 29 * 9) % 256));
}

TEST(ColorLUTContractTest, GetValueReturnsZeroVectorOutsideGrid)
{
    constexpr int gridSize = 3;
    const auto lut = ColorLUT::createIdentity(gridSize);
    constexpr int invalidCoordinates[][3] = {
        {-1, 1, 1}, {gridSize, 1, 1}, {-32, 1, 1}, {32, 1, 1},
        {std::numeric_limits<int>::min(), 1, 1},
        {std::numeric_limits<int>::max(), 1, 1},
        {1, -1, 1}, {1, gridSize, 1}, {1, -32, 1}, {1, 32, 1},
        {1, std::numeric_limits<int>::min(), 1},
        {1, std::numeric_limits<int>::max(), 1},
        {1, 1, -1}, {1, 1, gridSize}, {1, 1, -32}, {1, 1, 32},
        {1, 1, std::numeric_limits<int>::min()},
        {1, 1, std::numeric_limits<int>::max()},
    };
    for (const auto& coordinate : invalidCoordinates) {
        SCOPED_TRACE(::testing::Message()
            << "coordinate=(" << coordinate[0] << ", "
            << coordinate[1] << ", " << coordinate[2] << ")");
        EXPECT_EQ(lut.getValue(coordinate[0], coordinate[1], coordinate[2]),
                  QVector3D());
    }
    EXPECT_EQ(lut.getValue(0, 0, 0), QVector3D(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(lut.getValue(2, 2, 2), QVector3D(1.0f, 1.0f, 1.0f));
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

TEST(ColorLUTContractTest, SetValueRejectsIntegerExtremeCoordinatesWithoutMutation)
{
    constexpr int gridSize = 3;
    auto lut = ColorLUT::createIdentity(gridSize);
    const QVector3D preserved(0.2f, 0.4f, 0.6f);
    lut.setValue(1, 1, 1, preserved);
    constexpr int extremeCoordinates[][3] = {
        {std::numeric_limits<int>::min(), 1, 1},
        {std::numeric_limits<int>::max(), 1, 1},
        {1, std::numeric_limits<int>::min(), 1},
        {1, std::numeric_limits<int>::max(), 1},
        {1, 1, std::numeric_limits<int>::min()},
        {1, 1, std::numeric_limits<int>::max()},
    };
    for (const auto& coordinate : extremeCoordinates) {
        SCOPED_TRACE(::testing::Message()
            << "coordinate=(" << coordinate[0] << ", "
            << coordinate[1] << ", " << coordinate[2] << ")");
        lut.setValue(coordinate[0], coordinate[1], coordinate[2],
                     QVector3D(0.9f, 0.8f, 0.7f));
        EXPECT_EQ(lut.getValue(1, 1, 1), preserved);
        EXPECT_TRUE(lut.isValid());
    }
    EXPECT_EQ(lut.getValue(0, 0, 0), QVector3D(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(lut.getValue(2, 2, 2), QVector3D(1.0f, 1.0f, 1.0f));
}

TEST(ColorLUTContractTest, SetValueRejectsNonFiniteValuesInEveryChannelWithoutMutation)
{
    auto lut = ColorLUT::createIdentity(2);
    const QVector3D original(0.25f, 0.5f, 0.75f);
    lut.setValue(1, 0, 1, original);
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const QVector3D invalidValues[] = {
        QVector3D(nan, 0.1f, 0.2f),
        QVector3D(0.1f, nan, 0.2f),
        QVector3D(0.1f, 0.2f, nan),
        QVector3D(infinity, 0.1f, 0.2f),
        QVector3D(0.1f, -infinity, 0.2f),
        QVector3D(0.1f, 0.2f, infinity),
        QVector3D(-infinity, 0.1f, 0.2f),
        QVector3D(0.1f, infinity, 0.2f),
        QVector3D(0.1f, 0.2f, -infinity),
    };

    for (const QVector3D& invalidValue : invalidValues) {
        lut.setValue(1, 0, 1, invalidValue);
        EXPECT_EQ(lut.getValue(1, 0, 1), original);
        EXPECT_TRUE(lut.isValid());
    }
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

TEST(ColorLUTContractTest, CopyAssignmentOwnsIndependentLutSamples)
{
    auto original = ColorLUT::createIdentity(2);
    auto assigned = ColorLUT::createIdentity(3);
    assigned = original;
    assigned.setValue(1, 1, 1, QVector3D(0.2f, 0.3f, 0.4f));

    EXPECT_EQ(assigned.size().totalPoints(), 8);
    EXPECT_EQ(original.getValue(1, 1, 1), QVector3D(1.0f, 1.0f, 1.0f));
    EXPECT_EQ(assigned.getValue(1, 1, 1), QVector3D(0.2f, 0.3f, 0.4f));
}

TEST(ColorLUTContractTest, MoveConstructionTransfersLutState)
{
    auto source = ColorLUT::createIdentity(2);
    source.setName(QStringLiteral("Move Constructor"));
    source.setValue(1, 0, 0, QVector3D(0.2f, 0.4f, 0.6f));

    ColorLUT moved(std::move(source));

    EXPECT_TRUE(moved.isValid());
    EXPECT_EQ(moved.name(), QStringLiteral("Move Constructor"));
    EXPECT_EQ(moved.size().totalPoints(), 8);
    EXPECT_EQ(moved.getValue(1, 0, 0), QVector3D(0.2f, 0.4f, 0.6f));
}

TEST(ColorLUTContractTest, MoveAssignmentReleasesDestinationAndTransfersLutState)
{
    auto source = ColorLUT::createIdentity(2);
    source.setName(QStringLiteral("Move Assignment"));
    source.setValue(1, 0, 0, QVector3D(0.2f, 0.4f, 0.6f));
    ColorLUT destination = ColorLUT::createIdentity(3);

    destination = std::move(source);

    EXPECT_TRUE(destination.isValid());
    EXPECT_EQ(destination.name(), QStringLiteral("Move Assignment"));
    EXPECT_EQ(destination.size().totalPoints(), 8);
    EXPECT_EQ(destination.getValue(1, 0, 0), QVector3D(0.2f, 0.4f, 0.6f));
}

TEST(ColorLUTContractTest, MoveResultsRemainValidAfterSourcesAreDestroyed)
{
    ColorLUT moveConstructed = [] {
        auto source = ColorLUT::createIdentity(2);
        source.setName(QStringLiteral("Move Constructor Lifetime"));
        source.setValue(1, 0, 1, QVector3D(0.2f, 0.4f, 0.6f));
        return ColorLUT(std::move(source));
    }();

    ASSERT_TRUE(moveConstructed.isValid());
    EXPECT_EQ(moveConstructed.name(), QStringLiteral("Move Constructor Lifetime"));
    EXPECT_EQ(moveConstructed.getValue(1, 0, 1), QVector3D(0.2f, 0.4f, 0.6f));

    ColorLUT moveAssigned = ColorLUT::createIdentity(3);
    {
        auto source = ColorLUT::createIdentity(2);
        source.setName(QStringLiteral("Move Assignment Lifetime"));
        source.setValue(0, 1, 1, QVector3D(0.6f, 0.4f, 0.2f));
        moveAssigned = std::move(source);
    }

    ASSERT_TRUE(moveAssigned.isValid());
    EXPECT_EQ(moveAssigned.name(), QStringLiteral("Move Assignment Lifetime"));
    EXPECT_EQ(moveAssigned.size().totalPoints(), 8);
    EXPECT_EQ(moveAssigned.getValue(0, 1, 1), QVector3D(0.6f, 0.4f, 0.2f));
}

TEST(ColorLUTContractTest, SelfCopyAndSelfMoveAssignmentPreserveLutState)
{
    auto lut = ColorLUT::createIdentity(2);
    lut.setName(QStringLiteral("Self Assignment"));
    lut.setValue(0, 1, 0, QVector3D(0.15f, 0.35f, 0.55f));
    const QVector3D expected = lut.getValue(0, 1, 0);

    lut = lut;
    EXPECT_TRUE(lut.isValid());
    EXPECT_EQ(lut.name(), QStringLiteral("Self Assignment"));
    EXPECT_EQ(lut.getValue(0, 1, 0), expected);

    lut = std::move(lut);
    EXPECT_TRUE(lut.isValid());
    EXPECT_EQ(lut.name(), QStringLiteral("Self Assignment"));
    EXPECT_EQ(lut.getValue(0, 1, 0), expected);
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

TEST(ColorLUTContractTest, ApplyToRgb888ImageHandlesPaddedRowsAndAddsOpaqueAlpha)
{
    auto lut = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                lut.setValue(x, y, z, QVector3D(1.0f - x, y, z));
            }
        }
    }

    QImage source(3, 2, QImage::Format_RGB888);
    ASSERT_FALSE(source.isNull());
    source.setPixelColor(0, 0, QColor(51, 102, 153));
    source.setPixelColor(1, 0, QColor(25, 50, 75));
    source.setPixelColor(2, 0, QColor(0, 128, 255));
    source.setPixelColor(0, 1, QColor(255, 10, 20));
    source.setPixelColor(1, 1, QColor(64, 128, 192));
    source.setPixelColor(2, 1, QColor(200, 100, 50));
    const QImage original = source.copy();

    const QImage result = lut.applyToImage(source);

    ASSERT_EQ(result.size(), source.size());
    EXPECT_EQ(result.format(), QImage::Format_ARGB32);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const QRgb input = original.pixel(x, y);
            const QRgb output = result.pixel(x, y);
            SCOPED_TRACE(::testing::Message() << "pixel=(" << x << ',' << y << ')');
            EXPECT_NEAR(qRed(output), 255 - qRed(input), 1);
            EXPECT_EQ(qGreen(output), qGreen(input));
            EXPECT_EQ(qBlue(output), qBlue(input));
            EXPECT_EQ(qAlpha(output), 255);
        }
    }
    EXPECT_EQ(source, original);
}

TEST(ColorLUTContractTest, ApplyToImageCoversMultipleTilesAndPreservesEveryAlpha)
{
    auto lut = ColorLUT::createIdentity(2);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                lut.setValue(x, y, z, QVector3D(1.0f - x, y, z));
            }
        }
    }

    constexpr int width = 37;
    constexpr int height = 35;
    QImage source(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            source.setPixel(x, y, qRgba((x * 37 + y * 11) % 256,
                                        (x * 13 + y * 29) % 256,
                                        (x * 7 + y * 43) % 256,
                                        (x * 19 + y * 23) % 256));
        }
    }
    const QImage original = source.copy();
    const QImage result = lut.applyToImage(source);

    ASSERT_EQ(result.size(), source.size());
    EXPECT_EQ(result.format(), QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const QRgb input = original.pixel(x, y);
            const QRgb output = result.pixel(x, y);
            SCOPED_TRACE(::testing::Message() << "pixel=(" << x << ',' << y << ')');
            EXPECT_NEAR(qRed(output), 255 - qRed(input), 1);
            EXPECT_NEAR(qGreen(output), qGreen(input), 1);
            EXPECT_NEAR(qBlue(output), qBlue(input), 1);
            EXPECT_EQ(qAlpha(output), qAlpha(input));
        }
    }
    EXPECT_EQ(source, original);
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

TEST(ColorLUTManagerContractTest, RegistrationTrimsUnicodeWhitespaceAroundNames)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    auto lut = ColorLUT::createIdentity(2);
    lut.setValue(1, 1, 1, QVector3D(0.3f, 0.6f, 0.9f));

    manager.registerLUT(QString::fromUtf8("\t\n  grade  \t\n"), lut);

    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("grade")});
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("grade")));
    EXPECT_FALSE(manager.hasLUT(QString::fromUtf8("\t\n  grade  \t\n")));
    EXPECT_EQ(manager.getLUT(QStringLiteral("grade")).getValue(1, 1, 1),
              QVector3D(0.3f, 0.6f, 0.9f));
}

TEST(ColorLUTManagerContractTest, RegisteredAndRetrievedLutsOwnIndependentSamples)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    auto source = ColorLUT::createIdentity(2);
    source.setValue(1, 1, 1, QVector3D(0.2f, 0.4f, 0.6f));

    manager.registerLUT(QStringLiteral("grade"), source);
    source.setValue(1, 1, 1, QVector3D(0.8f, 0.6f, 0.4f));
    EXPECT_EQ(manager.getLUT(QStringLiteral("grade")).getValue(1, 1, 1),
              QVector3D(0.2f, 0.4f, 0.6f));

    auto retrieved = manager.getLUT(QStringLiteral("grade"));
    retrieved.setValue(1, 1, 1, QVector3D(0.9f, 0.1f, 0.3f));
    EXPECT_EQ(retrieved.getValue(1, 1, 1), QVector3D(0.9f, 0.1f, 0.3f));
    EXPECT_EQ(manager.getLUT(QStringLiteral("grade")).getValue(1, 1, 1),
              QVector3D(0.2f, 0.4f, 0.6f));
}

TEST(ColorLUTManagerContractTest, MissingNameReturnsIndependentDefaultLut)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    manager.registerLUT(QStringLiteral("present"), ColorLUT::createIdentity(2));

    EXPECT_FALSE(manager.hasLUT(QStringLiteral("absent")));
    auto missing = manager.getLUT(QStringLiteral("absent"));
    ASSERT_TRUE(missing.isValid());
    EXPECT_EQ(missing.name(), QStringLiteral("Identity"));
    EXPECT_EQ(missing.format(), LUTFormat::Cube);
    EXPECT_EQ(missing.size().dimX, 33);
    EXPECT_EQ(missing.size().dimY, 33);
    EXPECT_EQ(missing.size().dimZ, 33);
    EXPECT_EQ(missing.getValue(0, 0, 0), QVector3D(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(missing.getValue(32, 32, 32), QVector3D(1.0f, 1.0f, 1.0f));

    missing.setValue(32, 32, 32, QVector3D(0.25f, 0.5f, 0.75f));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("absent")));
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("present")});
    EXPECT_EQ(manager.getLUT(QStringLiteral("absent")).getValue(32, 32, 32),
              QVector3D(1.0f, 1.0f, 1.0f));
}

TEST(ColorLUTManagerContractTest, NamesRemainCaseSensitiveAndAreListedInKeyOrder)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    const auto lut = ColorLUT::createIdentity(2);

    manager.registerLUT(QStringLiteral("beta"), lut);
    manager.registerLUT(QStringLiteral("Alpha"), lut);
    manager.registerLUT(QStringLiteral("alpha"), lut);
    manager.registerLUT(QStringLiteral("Beta"), lut);

    EXPECT_TRUE(manager.hasLUT(QStringLiteral("Alpha")));
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("alpha")));
    EXPECT_EQ(manager.lutNames(), QStringList({
        QStringLiteral("Alpha"), QStringLiteral("Beta"),
        QStringLiteral("alpha"), QStringLiteral("beta")}));

    manager.removeLUT(QStringLiteral("ALPHA"));
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("Alpha")));
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("alpha")));
}

TEST(ColorLUTManagerContractTest, LookupAndRemovalRequireNormalizedRegistrationName)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    auto lut = ColorLUT::createIdentity(2);
    lut.setValue(1, 1, 1, QVector3D(0.3f, 0.6f, 0.9f));
    manager.registerLUT(QStringLiteral("  grade  "), lut);

    EXPECT_TRUE(manager.hasLUT(QStringLiteral("grade")));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("  grade  ")));
    EXPECT_EQ(manager.getLUT(QStringLiteral("  grade  ")).name(),
              QStringLiteral("Identity"));
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("grade")});

    manager.removeLUT(QStringLiteral("  grade  "));
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("grade")));
    manager.removeLUT(QStringLiteral("grade"));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("grade")));
    EXPECT_TRUE(manager.lutNames().isEmpty());
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

TEST(ColorLUTManagerContractTest, RepeatedRemovalIsIdempotentAndPreservesOtherEntries)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    auto target = ColorLUT::createIdentity(2);
    target.setValue(1, 1, 1, QVector3D(0.2f, 0.4f, 0.6f));
    auto other = ColorLUT::createIdentity(2);
    other.setValue(1, 1, 1, QVector3D(0.7f, 0.5f, 0.3f));
    manager.registerLUT(QStringLiteral("target"), target);
    manager.registerLUT(QStringLiteral("other"), other);
    const auto retained = manager.getLUT(QStringLiteral("target"));

    manager.removeLUT(QStringLiteral("target"));
    manager.removeLUT(QStringLiteral("target"));
    manager.removeLUT(QStringLiteral("missing"));

    EXPECT_FALSE(manager.hasLUT(QStringLiteral("target")));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("missing")));
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("other")));
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("other")});
    EXPECT_EQ(manager.getLUT(QStringLiteral("other")).getValue(1, 1, 1),
              QVector3D(0.7f, 0.5f, 0.3f));
    EXPECT_TRUE(retained.isValid());
    EXPECT_EQ(retained.getValue(1, 1, 1), QVector3D(0.2f, 0.4f, 0.6f));
}

TEST(ColorLUTManagerContractTest, RepeatedClearIsIdempotentAndRegistryCanBeReused)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    manager.registerLUT(QStringLiteral("first"), ColorLUT::createIdentity(2));
    auto retained = ColorLUT::createIdentity(2);
    retained.setValue(1, 1, 1, QVector3D(0.2f, 0.4f, 0.6f));
    manager.registerLUT(QStringLiteral("second"), retained);
    const auto retainedCopy = manager.getLUT(QStringLiteral("second"));

    manager.clear();
    EXPECT_TRUE(manager.lutNames().isEmpty());
    manager.clear();
    EXPECT_TRUE(manager.lutNames().isEmpty());
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("first")));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("second")));
    EXPECT_TRUE(retainedCopy.isValid());
    EXPECT_EQ(retainedCopy.getValue(1, 1, 1), QVector3D(0.2f, 0.4f, 0.6f));

    const auto recovered = ColorLUT::createIdentity(2);
    manager.registerLUT(QStringLiteral("recovered"), recovered);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("recovered")});
    EXPECT_TRUE(manager.getLUT(QStringLiteral("recovered")).isValid());
}

TEST(ColorLUTManagerContractTest, RetrievedLutSurvivesRemovalAndReRegistration)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    auto original = ColorLUT::createIdentity(2);
    original.setValue(1, 1, 1, QVector3D(0.2f, 0.4f, 0.6f));
    manager.registerLUT(QStringLiteral("grade"), original);
    auto retained = manager.getLUT(QStringLiteral("grade"));

    manager.removeLUT(QStringLiteral("grade"));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("grade")));
    EXPECT_TRUE(retained.isValid());
    EXPECT_EQ(retained.getValue(1, 1, 1), QVector3D(0.2f, 0.4f, 0.6f));

    auto replacement = ColorLUT::createIdentity(2);
    replacement.setValue(1, 1, 1, QVector3D(0.8f, 0.6f, 0.4f));
    manager.registerLUT(QStringLiteral("grade"), replacement);
    EXPECT_EQ(manager.getLUT(QStringLiteral("grade")).getValue(1, 1, 1),
              QVector3D(0.8f, 0.6f, 0.4f));
    EXPECT_EQ(retained.getValue(1, 1, 1), QVector3D(0.2f, 0.4f, 0.6f));

    manager.clear();
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("grade")));
    EXPECT_TRUE(retained.isValid());
    EXPECT_EQ(retained.getValue(1, 1, 1), QVector3D(0.2f, 0.4f, 0.6f));
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

TEST(ColorLUTManagerContractTest, DirectoryLoadDoesNotRecurseIntoSubdirectories)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString nestedPath = directory.filePath(QStringLiteral("nested"));
    ASSERT_TRUE(QDir().mkpath(nestedPath));
    const QByteArray cube =
        "LUT_3D_SIZE 2\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n";
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("root-grade.cube"), cube).isEmpty());
    QFile nestedFile(QDir(nestedPath).filePath(QStringLiteral("nested-grade.cube")));
    ASSERT_TRUE(nestedFile.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_EQ(nestedFile.write(cube), cube.size());
    nestedFile.close();

    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 1);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("root-grade")});
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("root-grade")));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("nested-grade")));
}

TEST(ColorLUTManagerContractTest, DirectoryLoadRegistersValidHaldPngAndSkipsInvalidImages)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    constexpr int lutSize = 33;
    QImage haldImage(lutSize * lutSize, lutSize, QImage::Format_ARGB32);
    ASSERT_FALSE(haldImage.isNull());
    for (int z = 0; z < lutSize; ++z) {
        for (int y = 0; y < lutSize; ++y) {
            for (int x = 0; x < lutSize; ++x) {
                haldImage.setPixel(x + z * lutSize, y,
                    qRgba(x * 255 / (lutSize - 1),
                          y * 255 / (lutSize - 1),
                          z * 255 / (lutSize - 1),
                          (x * 3 + y * 5 + z * 7) % 256));
            }
        }
    }
    const QString validPath = directory.filePath(QStringLiteral("film-grade.png"));
    ASSERT_TRUE(haldImage.save(validPath, "PNG"));
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("broken.png"),
                              "not an image").isEmpty());
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("ignored.bmp"),
                              "not scanned").isEmpty());

    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 1);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("film-grade")});
    ASSERT_TRUE(manager.hasLUT(QStringLiteral("film-grade")));
    const ColorLUT loaded = manager.getLUT(QStringLiteral("film-grade"));
    ASSERT_TRUE(loaded.isValid());
    EXPECT_EQ(loaded.format(), LUTFormat::PNG);
    EXPECT_EQ(loaded.size().dimX, lutSize);
    EXPECT_EQ(loaded.size().dimY, lutSize);
    EXPECT_EQ(loaded.size().dimZ, lutSize);
    const QVector3D loadedSample = loaded.getValue(32, 16, 7);
    EXPECT_NEAR(loadedSample.x(), 1.0f, 1e-6f);
    EXPECT_NEAR(loadedSample.y(), (16 * 255 / 32) / 255.0f, 1e-6f);
    EXPECT_NEAR(loadedSample.z(), (7 * 255 / 32) / 255.0f, 1e-6f);
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("broken")));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("ignored")));
}

TEST(ColorLUTManagerContractTest, DirectoryScanCandidatesCanBeRejectedByLoadDispatch)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    constexpr int lutSize = 33;
    QImage haldImage(lutSize * lutSize, lutSize, QImage::Format_RGB32);
    ASSERT_FALSE(haldImage.isNull());
    haldImage.fill(qRgb(31, 97, 181));
    ASSERT_TRUE(haldImage.save(directory.filePath(QStringLiteral("source.png")), "PNG"));
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("dispatch-rejects-jpeg.jpeg"),
                              "not an image").isEmpty());
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("dispatch-rejects-tif.tif"),
                              "not an image").isEmpty());

    const ColorLUT jpegCandidate(directory.filePath(QStringLiteral("dispatch-rejects-jpeg.jpeg")));
    const ColorLUT tifCandidate(directory.filePath(QStringLiteral("dispatch-rejects-tif.tif")));
    EXPECT_FALSE(jpegCandidate.isValid());
    EXPECT_NE(jpegCandidate.errorMessage().indexOf(QStringLiteral("Unknown LUT format")), -1);
    EXPECT_FALSE(tifCandidate.isValid());
    EXPECT_NE(tifCandidate.errorMessage().indexOf(QStringLiteral("Unknown LUT format")), -1);

    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 1);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("source")});
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("source")));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("dispatch-rejects-jpeg")));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("dispatch-rejects-tif")));
    EXPECT_EQ(manager.getLUT(QStringLiteral("source")).size().dimX, lutSize);
}

TEST(ColorLUTManagerContractTest, DirectoryReloadReplacesExistingLutByFilename)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("grade.cube"));
    const QByteArray firstCube =
        "LUT_3D_SIZE 2\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n";
    const QByteArray replacementCube =
        "LUT_3D_SIZE 2\n"
        "0.25 0.5 0.75\n0.25 0.5 0.75\n"
        "0.25 0.5 0.75\n0.25 0.5 0.75\n"
        "0.25 0.5 0.75\n0.25 0.5 0.75\n"
        "0.25 0.5 0.75\n0.25 0.5 0.75\n";
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("grade.cube"), firstCube).isEmpty());

    ASSERT_EQ(manager.loadFromDirectory(directory.path()), 1);
    EXPECT_EQ(manager.getLUT(QStringLiteral("grade")).getValue(1, 1, 1),
              QVector3D(1.0f, 1.0f, 1.0f));
    const ColorLUT retainedFirst = manager.getLUT(QStringLiteral("grade"));

    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("grade.cube"), replacementCube).isEmpty());
    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 1);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("grade")});
    EXPECT_EQ(manager.getLUT(QStringLiteral("grade")).getValue(1, 1, 1),
              QVector3D(0.25f, 0.5f, 0.75f));
    const ColorLUT retainedReplacement = manager.getLUT(QStringLiteral("grade"));

    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("grade.cube"), firstCube).isEmpty());
    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 1);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("grade")});
    EXPECT_EQ(manager.getLUT(QStringLiteral("grade")).getValue(1, 1, 1),
              QVector3D(1.0f, 1.0f, 1.0f));
    EXPECT_EQ(retainedFirst.getValue(1, 1, 1), QVector3D(1.0f, 1.0f, 1.0f));
    EXPECT_EQ(retainedReplacement.getValue(1, 1, 1), QVector3D(0.25f, 0.5f, 0.75f));
}

TEST(ColorLUTManagerContractTest, DirectoryLoadPreservesUnrelatedRegistryEntries)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    auto unrelated = ColorLUT::createIdentity(2);
    unrelated.setValue(1, 1, 1, QVector3D(0.1f, 0.3f, 0.5f));
    auto previousLoaded = ColorLUT::createIdentity(2);
    previousLoaded.setValue(1, 1, 1, QVector3D(0.2f, 0.4f, 0.6f));
    manager.registerLUT(QStringLiteral("keep"), unrelated);
    manager.registerLUT(QStringLiteral("replace-me"), previousLoaded);

    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QByteArray replacementCube =
        "LUT_3D_SIZE 2\n"
        "0.7 0.6 0.5\n0.7 0.6 0.5\n"
        "0.7 0.6 0.5\n0.7 0.6 0.5\n"
        "0.7 0.6 0.5\n0.7 0.6 0.5\n"
        "0.7 0.6 0.5\n0.7 0.6 0.5\n";
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("replace-me.cube"),
                              replacementCube).isEmpty());

    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 1);
    EXPECT_EQ(manager.lutNames(), QStringList({
        QStringLiteral("keep"), QStringLiteral("replace-me")}));
    EXPECT_EQ(manager.getLUT(QStringLiteral("keep")).getValue(1, 1, 1),
              QVector3D(0.1f, 0.3f, 0.5f));
    EXPECT_EQ(manager.getLUT(QStringLiteral("replace-me")).getValue(1, 1, 1),
              QVector3D(0.7f, 0.6f, 0.5f));
}

TEST(ColorLUTManagerContractTest, DirectoryLoadCountsFilesWithCollidingBasenamesSeparately)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QByteArray cube =
        "LUT_3D_SIZE 2\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n";
    const QByteArray csp =
        "CSPLUTV100\nLUT_3D_SIZE 2\nBEGIN DATA\n"
        "0.25 0.5 0.75\n0.25 0.5 0.75\n"
        "0.25 0.5 0.75\n0.25 0.5 0.75\n"
        "0.25 0.5 0.75\n0.25 0.5 0.75\n"
        "0.25 0.5 0.75\n0.25 0.5 0.75\nEND DATA\n";
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("shared.cube"), cube).isEmpty());
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("shared.csp"), csp).isEmpty());

    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 2);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("shared")});
    const ColorLUT registered = manager.getLUT(QStringLiteral("shared"));
    ASSERT_TRUE(registered.isValid());
    const QVector3D winner = registered.getValue(1, 1, 1);
    const bool cubeWon = winner == QVector3D(1.0f, 1.0f, 1.0f);
    const bool cspWon = winner == QVector3D(0.25f, 0.5f, 0.75f);
    EXPECT_TRUE(cubeWon || cspWon);
    EXPECT_EQ(registered.format(), cubeWon ? LUTFormat::Cube : LUTFormat::Csp);
}

TEST(ColorLUTManagerContractTest, InvalidCollidingFileDoesNotReplaceValidLut)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QByteArray validCube =
        "LUT_3D_SIZE 2\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n";
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("shared.cube"),
                              validCube).isEmpty());
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("shared.csp"),
                              "CSPLUTV100\nLUT_3D_SIZE 2\nBEGIN DATA\n0.1 0.2 0.3\nEND DATA\n").isEmpty());

    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 1);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("shared")});
    const ColorLUT registered = manager.getLUT(QStringLiteral("shared"));
    ASSERT_TRUE(registered.isValid());
    EXPECT_EQ(registered.format(), LUTFormat::Cube);
    EXPECT_EQ(registered.getValue(1, 1, 1), QVector3D(1.0f, 1.0f, 1.0f));
}

TEST(ColorLUTManagerContractTest, LexicallyLaterValidCollisionReplacesEarlierLut)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QByteArray identityCube =
        "LUT_3D_SIZE 2\n"
        "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
        "0 0 1\n1 0 1\n0 1 1\n1 1 1\n";
    const QByteArray constantCsp =
        "CSPLUTV100\nLUT_3D_SIZE 2\nBEGIN DATA\n"
        "0.25 0.5 0.75\n0.25 0.5 0.75\n"
        "0.25 0.5 0.75\n0.25 0.5 0.75\n"
        "0.25 0.5 0.75\n0.25 0.5 0.75\n"
        "0.25 0.5 0.75\n0.25 0.5 0.75\nEND DATA\n";
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("shared.cube"),
                              identityCube).isEmpty());
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("shared.csp"),
                              constantCsp).isEmpty());

    const ColorLUT cubeCheck(directory.filePath(QStringLiteral("shared.cube")));
    const ColorLUT cspCheck(directory.filePath(QStringLiteral("shared.csp")));
    ASSERT_TRUE(cubeCheck.isValid()) << cubeCheck.errorMessage().toStdString();
    ASSERT_TRUE(cspCheck.isValid()) << cspCheck.errorMessage().toStdString();
    EXPECT_EQ(cubeCheck.format(), LUTFormat::Cube);
    EXPECT_EQ(cspCheck.format(), LUTFormat::Csp);

    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 2);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("shared")});
    const ColorLUT registered = manager.getLUT(QStringLiteral("shared"));
    ASSERT_TRUE(registered.isValid());
    // QDir::Name visits shared.csp before shared.cube, so the cube is registered last.
    EXPECT_EQ(registered.format(), LUTFormat::Cube);
    EXPECT_EQ(registered.getValue(1, 1, 1), QVector3D(1.0f, 1.0f, 1.0f));
}

TEST(ColorLUTManagerContractTest, EmptyDirectoryReturnsZeroAndKeepsRegistry)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    manager.registerLUT(QStringLiteral("existing"), ColorLUT::createIdentity(2));
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 0);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("existing")});
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("existing")));
}

TEST(ColorLUTManagerContractTest, MissingDirectoryReturnsZeroAndPreservesExistingEntry)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    auto existing = ColorLUT::createIdentity(2);
    existing.setValue(1, 1, 1, QVector3D(0.2f, 0.4f, 0.6f));
    manager.registerLUT(QStringLiteral("existing"), existing);
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString missingPath = directory.filePath(QStringLiteral("not-created"));

    EXPECT_EQ(manager.loadFromDirectory(missingPath), 0);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("existing")});
    ASSERT_TRUE(manager.hasLUT(QStringLiteral("existing")));
    EXPECT_EQ(manager.getLUT(QStringLiteral("existing")).getValue(1, 1, 1),
              QVector3D(0.2f, 0.4f, 0.6f));
}

TEST(ColorLUTManagerContractTest, FilePathInsteadOfDirectoryPreservesExistingEntry)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    auto existing = ColorLUT::createIdentity(2);
    existing.setValue(1, 1, 1, QVector3D(0.2f, 0.4f, 0.6f));
    manager.registerLUT(QStringLiteral("existing"), existing);
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString filePath = writeLutFile(
        directory, QStringLiteral("not-a-directory"), "placeholder");
    ASSERT_FALSE(filePath.isEmpty());
    ASSERT_TRUE(QFileInfo(filePath).isFile());

    EXPECT_EQ(manager.loadFromDirectory(filePath), 0);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("existing")});
    EXPECT_EQ(manager.getLUT(QStringLiteral("existing")).getValue(1, 1, 1),
              QVector3D(0.2f, 0.4f, 0.6f));
}

TEST(ColorLUTManagerContractTest, DirectoryWithOnlyInvalidLutsPreservesExistingEntries)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    auto existing = ColorLUT::createIdentity(2);
    existing.setValue(1, 1, 1, QVector3D(0.2f, 0.4f, 0.6f));
    manager.registerLUT(QStringLiteral("existing"), existing);
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("broken.cube"),
                              "LUT_3D_SIZE 2\n0 0 0\n").isEmpty());
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("unsupported.jpeg"),
                              "not an image").isEmpty());

    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 0);
    EXPECT_EQ(manager.lutNames(), QStringList{QStringLiteral("existing")});
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("broken")));
    EXPECT_FALSE(manager.hasLUT(QStringLiteral("unsupported")));
    EXPECT_EQ(manager.getLUT(QStringLiteral("existing")).getValue(1, 1, 1),
              QVector3D(0.2f, 0.4f, 0.6f));
}

TEST(ColorLUTManagerContractTest, InvalidDirectoryFilePreservesExistingSameNameEntry)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    auto existing = ColorLUT::createIdentity(2);
    existing.setValue(1, 1, 1, QVector3D(0.2f, 0.4f, 0.6f));
    manager.registerLUT(QStringLiteral("grade"), existing);
    manager.registerLUT(QStringLiteral("unrelated"), ColorLUT::createIdentity(2));
    const ColorLUT retained = manager.getLUT(QStringLiteral("grade"));

    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_FALSE(writeLutFile(directory, QStringLiteral("grade.cube"),
                              "LUT_3D_SIZE 2\n0 0 0\n").isEmpty());

    EXPECT_EQ(manager.loadFromDirectory(directory.path()), 0);
    EXPECT_EQ(manager.lutNames(), QStringList({
        QStringLiteral("grade"), QStringLiteral("unrelated")}));
    EXPECT_TRUE(manager.hasLUT(QStringLiteral("grade")));
    EXPECT_EQ(manager.getLUT(QStringLiteral("grade")).getValue(1, 1, 1),
              QVector3D(0.2f, 0.4f, 0.6f));
    EXPECT_EQ(manager.getLUT(QStringLiteral("unrelated")).getValue(1, 1, 1),
              QVector3D(1.0f, 1.0f, 1.0f));
    EXPECT_EQ(retained.getValue(1, 1, 1), QVector3D(0.2f, 0.4f, 0.6f));
}

TEST(ColorLUTManagerContractTest, BuiltinNamesRegisterValidBoundedSeventeenPointLuts)
{
    LutManagerReset reset;
    auto& manager = reset.manager();
    const QStringList names = BuiltinLUTs::builtinLUTNames();
    ASSERT_EQ(names.size(), 9);
    for (qsizetype index = 0; index < names.size(); ++index) {
        for (qsizetype other = index + 1; other < names.size(); ++other) {
            EXPECT_NE(names[index], names[other]);
        }
    }

    BuiltinLUTs::registerBuiltins(manager);
    EXPECT_EQ(manager.lutNames().size(), names.size());
    for (const QString& name : names) {
        ASSERT_TRUE(manager.hasLUT(name)) << name.toStdString();
        const ColorLUT lut = manager.getLUT(name);
        ASSERT_TRUE(lut.isValid()) << name.toStdString();
        EXPECT_EQ(lut.size().dimX, 17) << name.toStdString();
        EXPECT_EQ(lut.size().dimY, 17) << name.toStdString();
        EXPECT_EQ(lut.size().dimZ, 17) << name.toStdString();
        EXPECT_EQ(lut.dataSize(), 17u * 17u * 17u * 3u * sizeof(float))
            << name.toStdString();

        const float* values = lut.rawData();
        ASSERT_NE(values, nullptr) << name.toStdString();
        const size_t valueCount = lut.dataSize() / sizeof(float);
        for (size_t index = 0; index < valueCount; ++index) {
            SCOPED_TRACE(::testing::Message()
                << "name=" << name.toStdString() << " sample=" << index);
            EXPECT_TRUE(std::isfinite(values[index]));
            EXPECT_GE(values[index], 0.0f);
            EXPECT_LE(values[index], 1.0f);
        }
    }
}
