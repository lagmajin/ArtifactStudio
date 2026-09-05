#include <gtest/gtest.h>

#include <cmath>

#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVector3D>
#include <QVector4D>

import MeshImporter;
import Mesh;
import Memory.SharedPtr;
import Utils.String.UniString;

using namespace ArtifactCore;

namespace {

QString writeTempPly(QTemporaryDir& dir, const QString& name, const QString& content) {
    const QString path = dir.filePath(name);
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&file);
    out << content;
    file.close();
    return path;
}

SharedPtr<Mesh> importPly(const QString& path, MeshImporter& importer) {
    return importer.importMeshFromFile(UniString(path));
}

} // namespace

TEST(MeshImporterPlyTest, VertexOnlyWithoutFaceElementLoads) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeTempPly(dir, QStringLiteral("points.ply"),
        QStringLiteral("ply\n"
                       "format ascii 1.0\n"
                       "element vertex 4\n"
                       "property float x\n"
                       "property float y\n"
                       "property float z\n"
                       "end_header\n"
                       "0 0 0\n"
                       "1 0 0\n"
                       "0 2 0\n"
                       "0 0 3\n"));

    MeshImporter importer;
    auto mesh = importPly(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    EXPECT_EQ(mesh->vertexCount(), 4);
    EXPECT_EQ(mesh->polygonCount(), 0);

    const auto positions = mesh->vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positions, nullptr);
    EXPECT_FLOAT_EQ((*positions)[1].x(), 1.0f);
    EXPECT_FLOAT_EQ((*positions)[2].y(), 2.0f);
    EXPECT_FLOAT_EQ((*positions)[3].z(), 3.0f);

    // No color declared: defaults to opaque white.
    const auto colors = mesh->vertexAttributes().get<QVector4D>("color");
    ASSERT_NE(colors, nullptr);
    EXPECT_FLOAT_EQ((*colors)[0].x(), 1.0f);
    EXPECT_FLOAT_EQ((*colors)[0].w(), 1.0f);

    EXPECT_FLOAT_EQ(mesh->boundingBoxMin().x(), 0.0f);
    EXPECT_FLOAT_EQ(mesh->boundingBoxMax().y(), 2.0f);
}

TEST(MeshImporterPlyTest, ZeroFaceElementLoads) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeTempPly(dir, QStringLiteral("zero_face.ply"),
        QStringLiteral("ply\n"
                       "format ascii 1.0\n"
                       "element vertex 2\n"
                       "property float x\n"
                       "property float y\n"
                       "property float z\n"
                       "element face 0\n"
                       "property list uchar int vertex_indices\n"
                       "end_header\n"
                       "0 0 0\n"
                       "1 1 1\n"));

    MeshImporter importer;
    auto mesh = importPly(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    EXPECT_EQ(mesh->vertexCount(), 2);
    EXPECT_EQ(mesh->polygonCount(), 0);
}

TEST(MeshImporterPlyTest, UcharVertexColorsAreNormalized) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeTempPly(dir, QStringLiteral("colored.ply"),
        QStringLiteral("ply\n"
                       "format ascii 1.0\n"
                       "element vertex 2\n"
                       "property float x\n"
                       "property float y\n"
                       "property float z\n"
                       "property uchar red\n"
                       "property uchar green\n"
                       "property uchar blue\n"
                       "end_header\n"
                       "0 0 0 255 0 0\n"
                       "1 0 0 0 128 255\n"));

    MeshImporter importer;
    auto mesh = importPly(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    const auto colors = mesh->vertexAttributes().get<QVector4D>("color");
    ASSERT_NE(colors, nullptr);
    EXPECT_NEAR((*colors)[0].x(), 1.0f, 1e-5);
    EXPECT_NEAR((*colors)[0].y(), 0.0f, 1e-5);
    EXPECT_NEAR((*colors)[1].y(), 128.0f / 255.0f, 1e-5);
    EXPECT_NEAR((*colors)[1].z(), 1.0f, 1e-5);
}

TEST(MeshImporterPlyTest, PolygonMeshStillLoads) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeTempPly(dir, QStringLiteral("tri.ply"),
        QStringLiteral("ply\n"
                       "format ascii 1.0\n"
                       "element vertex 3\n"
                       "property float x\n"
                       "property float y\n"
                       "property float z\n"
                       "element face 1\n"
                       "property list uchar int vertex_indices\n"
                       "end_header\n"
                       "0 0 0\n"
                       "1 0 0\n"
                       "0 1 0\n"
                       "3 0 1 2\n"));

    MeshImporter importer;
    auto mesh = importPly(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    EXPECT_EQ(mesh->vertexCount(), 3);
    EXPECT_EQ(mesh->polygonCount(), 1);
}

TEST(MeshImporterPlyTest, PolygonMeshGetsValidNormals) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeTempPly(dir, QStringLiteral("tri_norm.ply"),
        QStringLiteral("ply\n"
                       "format ascii 1.0\n"
                       "element vertex 3\n"
                       "property float x\n"
                       "property float y\n"
                       "property float z\n"
                       "element face 1\n"
                       "property list uchar int vertex_indices\n"
                       "end_header\n"
                       "0 0 0\n"
                       "1 0 0\n"
                       "0 1 0\n"
                       "3 0 1 2\n"));

    MeshImporter importer;
    auto mesh = importPly(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    const auto normals = mesh->vertexAttributes().get<QVector3D>("normal");
    ASSERT_NE(normals, nullptr);
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(std::isfinite((*normals)[i].x()));
        EXPECT_FLOAT_EQ((*normals)[i].x(), 0.0f);
        EXPECT_FLOAT_EQ((*normals)[i].y(), 0.0f);
        EXPECT_FLOAT_EQ((*normals)[i].z(), 1.0f);
    }
}

TEST(MeshImporterPlyTest, LargePointCloudIsDecimatedDeterministically) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("large.ply"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    constexpr int kPoints = 300000;
    file.write("ply\n"
               "format binary_little_endian 1.0\n"
               "element vertex 300000\n"
               "property float x\n"
               "property float y\n"
               "property float z\n"
               "property uchar red\n"
               "property uchar green\n"
               "property uchar blue\n"
               "end_header\n");
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);
    for (int i = 0; i < kPoints; ++i) {
        out << static_cast<float>(i % 100) << static_cast<float>((i / 100) % 100)
            << static_cast<float>(i / 10000);
        out.writeRawData("\x80\x80\x80", 3);
    }
    file.close();

    MeshImporter importer;
    auto mesh = importPly(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    EXPECT_GT(mesh->vertexCount(), 200000);
    EXPECT_LE(mesh->vertexCount(), 262144);
    EXPECT_EQ(mesh->polygonCount(), 0);
    const auto positions = mesh->vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positions, nullptr);
    // The first input point always survives decimation.
    EXPECT_FLOAT_EQ((*positions)[0].x(), 0.0f);
    EXPECT_FLOAT_EQ((*positions)[0].y(), 0.0f);
    EXPECT_FLOAT_EQ((*positions)[0].z(), 0.0f);
    const auto colors = mesh->vertexAttributes().get<QVector4D>("color");
    ASSERT_NE(colors, nullptr);
    EXPECT_EQ(colors->data().size(), positions->data().size());

    MeshImporter secondImporter;
    auto secondMesh = importPly(path, secondImporter);
    ASSERT_NE(secondMesh, nullptr);
    EXPECT_EQ(secondMesh->vertexCount(), mesh->vertexCount());
}
TEST(MeshImporterPlyTest, MissingFileReturnsNull) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    MeshImporter importer;
    auto mesh = importPly(dir.filePath(QStringLiteral("does_not_exist.ply")), importer);
    EXPECT_EQ(mesh, nullptr);
    EXPECT_FALSE(importer.lastError().isEmpty());
}

TEST(MeshImporterPlyTest, BinaryLittleEndianPointsLoad) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("points_le.ply"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    const QByteArray header =
        "ply\n"
        "format binary_little_endian 1.0\n"
        "element vertex 2\n"
        "property float x\n"
        "property float y\n"
        "property float z\n"
        "property uchar red\n"
        "property uchar green\n"
        "property uchar blue\n"
        "end_header\n";
    file.write(header);
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);
    out << 0.0f << 0.0f << 0.0f;
    out.writeRawData("\xff\x00\x00", 3);
    out << 1.0f << 2.0f << 3.0f;
    out.writeRawData("\x00\x80\xff", 3);
    file.close();

    MeshImporter importer;
    auto mesh = importPly(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    EXPECT_EQ(mesh->vertexCount(), 2);
    EXPECT_EQ(mesh->polygonCount(), 0);
    const auto positions = mesh->vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positions, nullptr);
    EXPECT_FLOAT_EQ((*positions)[1].x(), 1.0f);
    EXPECT_FLOAT_EQ((*positions)[1].y(), 2.0f);
    EXPECT_FLOAT_EQ((*positions)[1].z(), 3.0f);
    const auto colors = mesh->vertexAttributes().get<QVector4D>("color");
    ASSERT_NE(colors, nullptr);
    EXPECT_NEAR((*colors)[0].x(), 1.0f, 1e-5);
    EXPECT_NEAR((*colors)[1].y(), 128.0f / 255.0f, 1e-5);
    EXPECT_NEAR((*colors)[1].z(), 1.0f, 1e-5);
}

TEST(MeshImporterPlyTest, BinaryBigEndianPointsLoad) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("points_be.ply"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("ply\n"
               "format binary_big_endian 1.0\n"
               "element vertex 1\n"
               "property float x\n"
               "property float y\n"
               "property float z\n"
               "end_header\n");
    QDataStream out(&file);
    out.setByteOrder(QDataStream::BigEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);
    out << 4.0f << 5.0f << 6.0f;
    file.close();

    MeshImporter importer;
    auto mesh = importPly(path, importer);
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    EXPECT_EQ(mesh->vertexCount(), 1);
    const auto positions = mesh->vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positions, nullptr);
    EXPECT_FLOAT_EQ((*positions)[0].x(), 4.0f);
    EXPECT_FLOAT_EQ((*positions)[0].y(), 5.0f);
    EXPECT_FLOAT_EQ((*positions)[0].z(), 6.0f);
}

TEST(MeshImporterPlyTest, TruncatedBinaryReturnsNull) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("truncated.ply"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("ply\n"
               "format binary_little_endian 1.0\n"
               "element vertex 2\n"
               "property float x\n"
               "property float y\n"
               "property float z\n"
               "end_header\n");
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);
    out << 1.0f << 2.0f << 3.0f;
    file.close();

    MeshImporter importer;
    auto mesh = importPly(path, importer);
    EXPECT_EQ(mesh, nullptr);
    EXPECT_FALSE(importer.lastError().isEmpty());
}

