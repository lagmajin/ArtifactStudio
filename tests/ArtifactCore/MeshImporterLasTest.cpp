#include <gtest/gtest.h>

#include <cmath>

#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <QVector3D>
#include <QVector4D>

import MeshImporter;
import Mesh;
import Memory.SharedPtr;
import Utils.String.UniString;

using namespace ArtifactCore;

namespace {

// Minimal LAS 1.2 header (227 bytes) written to the stream.
void writeLasHeader(QDataStream& out, quint8 pointFormat, quint16 recordLength,
                    quint32 pointCount, double scale = 0.01) {
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("LASF", 4);
    out << quint16(0); // File Source ID
    out << quint16(0); // Global Encoding
    out.writeRawData("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16); // GUID
    out << quint8(1) << quint8(2); // Version 1.2
    QByteArray systemId(32, '\0');
    out.writeRawData(systemId.constData(), 32);
    QByteArray genSoftware(32, '\0');
    out.writeRawData(genSoftware.constData(), 32);
    out << quint16(1) << quint16(2026); // Creation day/year
    out << quint16(227); // Header size
    out << quint32(227); // Offset to point data
    out << quint32(0); // Number of VLRs
    out << pointFormat;
    out << recordLength;
    out << pointCount;
    for (int i = 0; i < 5; ++i) out << quint32(0); // Points by return
    out << scale << scale << scale; // Scale X/Y/Z
    out << 0.0 << 0.0 << 0.0; // Offset X/Y/Z
    out << 10.0 << -10.0 << 10.0 << -10.0 << 10.0 << -10.0; // Max/min bounds
}

SharedPtr<Mesh> importLas(const QString& path, MeshImporter& importer) {
    return importer.importMeshFromFile(UniString(path));
}

} // namespace

TEST(MeshImporterLasTest, Format2RgbPointsLoad) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("rgb.las"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    QDataStream out(&file);
    writeLasHeader(out, 2, 26, 2);
    // Point 1: (1, 2, 3) red
    out << qint32(100) << qint32(200) << qint32(300);
    out << quint16(1000) << quint8(0) << quint8(2) << qint8(0) << quint8(0) << quint16(0);
    out << quint16(65535) << quint16(0) << quint16(0);
    // Point 2: (4, 5, 6) green/blue mix
    out << qint32(400) << qint32(500) << qint32(600);
    out << quint16(2000) << quint8(0) << quint8(2) << qint8(0) << quint8(0) << quint16(0);
    out << quint16(0) << quint16(32768) << quint16(65535);
    file.close();

    MeshImporter importer;
    auto mesh = importLas(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    EXPECT_EQ(mesh->vertexCount(), 2);
    EXPECT_EQ(mesh->polygonCount(), 0);
    const auto positions = mesh->vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positions, nullptr);
    EXPECT_FLOAT_EQ((*positions)[0].x(), 1.0f);
    EXPECT_FLOAT_EQ((*positions)[0].y(), 2.0f);
    EXPECT_FLOAT_EQ((*positions)[1].z(), 6.0f);
    const auto colors = mesh->vertexAttributes().get<QVector4D>("color");
    ASSERT_NE(colors, nullptr);
    EXPECT_NEAR((*colors)[0].x(), 1.0f, 1e-5);
    EXPECT_NEAR((*colors)[0].y(), 0.0f, 1e-5);
    EXPECT_NEAR((*colors)[1].y(), 32768.0f / 65535.0f, 1e-5);
    EXPECT_NEAR((*colors)[1].z(), 1.0f, 1e-5);
}

TEST(MeshImporterLasTest,
     OversizedRgbPointCloudDecimatesDeterministicallyAndKeepsColorsAttached)
{
    constexpr quint32 kInputPoints = 262145;
    constexpr int kPointCloudBudget = 262144;
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("oversized_rgb.las"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    QDataStream out(&file);
    writeLasHeader(out, 2, 26, kInputPoints);
    for (quint32 index = 0; index < kInputPoints; ++index) {
        const qint32 x = static_cast<qint32>(index % 640u);
        const qint32 y = static_cast<qint32>((index / 640u) % 640u);
        const qint32 z = static_cast<qint32>(index / (640u * 640u));
        out << x << y << z;
        out << quint16(0) << quint8(0) << quint8(2) << qint8(0) << quint8(0)
            << quint16(0);
        out << static_cast<quint16>((index % 256u) * 257u)
            << static_cast<quint16>(((index / 256u) % 256u) * 257u)
            << static_cast<quint16>(((index / 65536u) % 256u) * 257u);
    }
    file.close();

    MeshImporter importer;
    auto mesh = importLas(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    EXPECT_EQ(mesh->polygonCount(), 0);
    EXPECT_EQ(mesh->vertexCount(), kPointCloudBudget);
    const auto positions = mesh->vertexAttributes().get<QVector3D>("position");
    const auto colors = mesh->vertexAttributes().get<QVector4D>("color");
    ASSERT_NE(positions, nullptr);
    ASSERT_NE(colors, nullptr);
    ASSERT_EQ(positions->data().size(), colors->data().size());
    for (qsizetype pointIndex = 0; pointIndex < positions->data().size();
         ++pointIndex) {
        const QVector3D position = (*positions)[pointIndex];
        const quint32 originalIndex =
            static_cast<quint32>(std::lround(position.z() / 0.01f)) * 640u * 640u +
            static_cast<quint32>(std::lround(position.y() / 0.01f)) * 640u +
            static_cast<quint32>(std::lround(position.x() / 0.01f));
        const QVector4D color = (*colors)[pointIndex];
        EXPECT_NEAR(color.x(), static_cast<float>(originalIndex % 256u) / 255.0f,
                    1e-5f);
        EXPECT_NEAR(color.y(),
                    static_cast<float>((originalIndex / 256u) % 256u) / 255.0f,
                    1e-5f);
        EXPECT_NEAR(color.z(),
                    static_cast<float>((originalIndex / 65536u) % 256u) / 255.0f,
                    1e-5f);
    }

    MeshImporter repeatedImporter;
    auto repeatedMesh = importLas(path, repeatedImporter);
    ASSERT_NE(repeatedMesh, nullptr) << repeatedImporter.lastError().toStdString();
    ASSERT_EQ(repeatedMesh->vertexCount(), mesh->vertexCount());
    const auto repeatedPositions =
        repeatedMesh->vertexAttributes().get<QVector3D>("position");
    const auto repeatedColors =
        repeatedMesh->vertexAttributes().get<QVector4D>("color");
    ASSERT_NE(repeatedPositions, nullptr);
    ASSERT_NE(repeatedColors, nullptr);
    for (qsizetype pointIndex = 0; pointIndex < positions->data().size();
         ++pointIndex) {
        EXPECT_EQ((*repeatedPositions)[pointIndex], (*positions)[pointIndex]);
        EXPECT_EQ((*repeatedColors)[pointIndex], (*colors)[pointIndex]);
    }
    EXPECT_TRUE(mesh->generateMeshletLODData().levels.isEmpty())
        << "decimated LAS point clouds must remain outside triangle mesh LOD";
}

TEST(MeshImporterLasTest,
     OversizedCoincidentPointCloudKeepsOnePointAndItsFirstColor)
{
    constexpr quint32 kInputPoints = 262145;
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("coincident_rgb.las"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    QDataStream out(&file);
    writeLasHeader(out, 2, 26, kInputPoints);
    for (quint32 index = 0; index < kInputPoints; ++index) {
        out << qint32(100) << qint32(-200) << qint32(300);
        out << quint16(0) << quint8(0) << quint8(2) << qint8(0) << quint8(0)
            << quint16(0);
        out << static_cast<quint16>((index % 256u) * 257u)
            << static_cast<quint16>(((index / 256u) % 256u) * 257u)
            << static_cast<quint16>(((index / 65536u) % 256u) * 257u);
    }
    file.close();

    MeshImporter importer;
    auto mesh = importLas(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    EXPECT_EQ(mesh->vertexCount(), 1)
        << "duplicate positions should not consume the point-cloud LOD budget";
    const auto positions = mesh->vertexAttributes().get<QVector3D>("position");
    const auto colors = mesh->vertexAttributes().get<QVector4D>("color");
    ASSERT_NE(positions, nullptr);
    ASSERT_NE(colors, nullptr);
    ASSERT_EQ(positions->data().size(), 1);
    ASSERT_EQ(colors->data().size(), 1);
    EXPECT_EQ((*positions)[0], QVector3D(1.0f, -2.0f, 3.0f));
    EXPECT_EQ((*colors)[0], QVector4D(0.0f, 0.0f, 0.0f, 1.0f))
        << "the retained first point must keep its matching RGB sample";
}

TEST(MeshImporterLasTest, PointCloudAtDecimationBudgetIsLeftUnchanged)
{
    constexpr quint32 kPointCount = 262144;
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("budget_boundary.las"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    QDataStream out(&file);
    writeLasHeader(out, 0, 20, kPointCount);
    for (quint32 index = 0; index < kPointCount; ++index) {
        out << qint32(100) << qint32(-200) << qint32(300);
        out << quint16(0) << quint8(0) << quint8(1) << qint8(0) << quint8(0)
            << quint16(0);
    }
    file.close();

    MeshImporter importer;
    auto mesh = importLas(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    EXPECT_EQ(mesh->vertexCount(), static_cast<int>(kPointCount))
        << "decimation should begin only when the point count exceeds budget";
    EXPECT_EQ(mesh->polygonCount(), 0);
}

TEST(MeshImporterLasTest, Format0IntensityBecomesGray) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("intensity.las"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    QDataStream out(&file);
    writeLasHeader(out, 0, 20, 1);
    out << qint32(1000) << qint32(0) << qint32(-500);
    out << quint16(32768) << quint8(0) << quint8(1) << qint8(0) << quint8(0) << quint16(7);
    file.close();

    MeshImporter importer;
    auto mesh = importLas(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    EXPECT_EQ(mesh->vertexCount(), 1);
    const auto positions = mesh->vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positions, nullptr);
    EXPECT_FLOAT_EQ((*positions)[0].x(), 10.0f);
    EXPECT_FLOAT_EQ((*positions)[0].z(), -5.0f);
    const auto colors = mesh->vertexAttributes().get<QVector4D>("color");
    ASSERT_NE(colors, nullptr);
    EXPECT_NEAR((*colors)[0].x(), 32768.0f / 65535.0f, 1e-5);
    EXPECT_NEAR((*colors)[0].y(), (*colors)[0].x(), 1e-6);
    EXPECT_NEAR((*colors)[0].z(), (*colors)[0].x(), 1e-6);
}

TEST(MeshImporterLasTest, WaveformFormatIsRejected) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("waveform.las"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    QDataStream out(&file);
    writeLasHeader(out, 9, 59, 1);
    file.close();

    MeshImporter importer;
    auto mesh = importLas(path, importer);
    EXPECT_EQ(mesh, nullptr);
    EXPECT_FALSE(importer.lastError().isEmpty());
}

TEST(MeshImporterLasTest, NonLasFileIsRejected) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("fake.las"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("this is not a LAS file");
    file.close();

    MeshImporter importer;
    auto mesh = importLas(path, importer);
    EXPECT_EQ(mesh, nullptr);
    EXPECT_FALSE(importer.lastError().isEmpty());
}
