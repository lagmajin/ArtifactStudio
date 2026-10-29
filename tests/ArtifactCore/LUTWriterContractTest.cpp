#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>
#include <QString>

#include <array>

import Color.LUT;
import Color.LUTWriter;

using namespace ArtifactCore;

TEST(LUTWriterContractTest, CubeExportWritesHeaderAndAllRgbEntriesInGridOrder)
{
    constexpr std::array<float, 24> values = {
        0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
        0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 0.0f,
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f,
        0.7f, 0.8f, 0.9f, 1.0f, 0.25f, 0.5f,
    };

    const QByteArray output = exportLUTToMemory(values.data(), 2, LUTFormat::Cube);

    ASSERT_FALSE(output.isEmpty());
    EXPECT_TRUE(output.startsWith(
        "TITLE \"ArtifactStudio Exported LUT\"\n"
        "LUT_3D_SIZE 2\n"
        "DOMAIN_MIN 0.0 0.0 0.0\n"
        "DOMAIN_MAX 1.0 1.0 1.0\n"));
    EXPECT_TRUE(output.endsWith("0.700000 0.800000 0.900000\n"
                                "1.000000 0.250000 0.500000\n"));
    EXPECT_EQ(output.count('\n'), 12);
    EXPECT_EQ(output, exportLUTToMemory(values.data(), 2, LUTFormat::Cube));
}

TEST(LUTWriterContractTest, DiscreetExportNumbersEntriesAndPreservesInputValues)
{
    constexpr std::array<float, 24> values = {
        0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
        0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 0.0f,
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f,
        0.7f, 0.8f, 0.9f, 1.0f, 0.25f, 0.5f,
    };

    const QByteArray output = exportLUTToMemory(values.data(), 2, LUTFormat::_3dl);

    ASSERT_FALSE(output.isEmpty());
    EXPECT_TRUE(output.startsWith("3DMLUT\nLUT_3D_SIZE 2\n\n"));
    EXPECT_TRUE(output.contains("0000 0.000000 0.100000 0.200000\n"));
    EXPECT_TRUE(output.contains("0007 1.000000 0.250000 0.500000\n"));
    EXPECT_EQ(output.count('\n'), 11);
    EXPECT_EQ(output, exportLUTToMemory(values.data(), 2, LUTFormat::_3dl));
    EXPECT_FLOAT_EQ(values[0], 0.0f);
    EXPECT_FLOAT_EQ(values[23], 0.5f);
}

TEST(LUTWriterContractTest, RejectsNullUnsupportedAndOutOfRangeInputs)
{
    constexpr std::array<float, 24> values = {};
    EXPECT_TRUE(exportLUTToMemory(nullptr, 2, LUTFormat::Cube).isEmpty());
    EXPECT_TRUE(exportLUTToMemory(values.data(), 1, LUTFormat::Cube).isEmpty());
    EXPECT_TRUE(exportLUTToMemory(values.data(), 257, LUTFormat::Cube).isEmpty());
    EXPECT_TRUE(exportLUTToMemory(values.data(), 2, LUTFormat::Csp).isEmpty());
    EXPECT_TRUE(exportLUTToMemory(values.data(), 2, LUTFormat::Unknown).isEmpty());
}

TEST(LUTWriterContractTest, FileExportReportsInvalidInputAndWritesMemoryPayload)
{
    constexpr std::array<float, 24> values = {
        0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
        0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 0.0f,
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f,
        0.7f, 0.8f, 0.9f, 1.0f, 0.25f, 0.5f,
    };
    QString error;
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString filePath = directory.filePath(QStringLiteral("output.cube"));
    EXPECT_FALSE(writeLUTToFile(filePath, nullptr, 2,
                                LUTFormat::Cube, &error));
    EXPECT_EQ(error, QStringLiteral("Invalid LUT data or size"));

    EXPECT_FALSE(writeLUTToFile(filePath, values.data(), 2,
                                LUTFormat::Csp, &error));
    EXPECT_EQ(error, QStringLiteral("Failed to generate LUT content"));

    ASSERT_TRUE(writeLUTToFile(filePath, values.data(), 2, LUTFormat::Cube, &error))
        << error.toStdString();
    QFile file(filePath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(file.readAll(), exportLUTToMemory(values.data(), 2, LUTFormat::Cube));
}

TEST(LUTWriterContractTest, FileExportReportsOpenFailure)
{
    constexpr std::array<float, 24> values = {};
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString missingPath = directory.filePath(
        QStringLiteral("missing/output.cube"));
    QString error;

    EXPECT_FALSE(writeLUTToFile(missingPath, values.data(), 2,
                                LUTFormat::Cube, &error));
    EXPECT_TRUE(error.startsWith(QStringLiteral("Cannot open file:")));
}
