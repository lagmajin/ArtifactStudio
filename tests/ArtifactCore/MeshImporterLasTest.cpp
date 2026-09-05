#include <gtest/gtest.h>

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
