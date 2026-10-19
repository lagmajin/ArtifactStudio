#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include "ufbx.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QMatrix4x4>
#include <QString>
#include <QVector>
#include <QVector2D>
#include <QVector3D>

import MeshImporter;
import Mesh;
import Memory.SharedPtr;
import Utils.String.UniString;
import Image.Raw;
import IO.ImageImporter;

using namespace ArtifactCore;

namespace {

QString fixturePath(const char* specificEnvironmentVariable)
{
    QString path = qEnvironmentVariable(specificEnvironmentVariable).trimmed();
    if (path.isEmpty()) {
        path = qEnvironmentVariable("ARTIFACT_FBX_TEST_FILE").trimmed();
    }
    if (path.isEmpty()) return {};

    return QFileInfo(path).absoluteFilePath();
}

bool matricesDiffer(const QVector<QMatrix4x4>& lhs,
                    const QVector<QMatrix4x4>& rhs)
{
    if (lhs.size() != rhs.size()) return true;
    for (qsizetype matrixIndex = 0; matrixIndex < lhs.size(); ++matrixIndex) {
        const float* left = lhs[matrixIndex].constData();
        const float* right = rhs[matrixIndex].constData();
        for (int component = 0; component < 16; ++component) {
            if (std::abs(left[component] - right[component]) > 1.0e-4f) {
                return true;
            }
        }
    }
    return false;
}

bool hasFiniteMatrices(const QVector<QMatrix4x4>& matrices)
{
    for (const QMatrix4x4& matrix : matrices) {
        const float* values = matrix.constData();
        for (int component = 0; component < 16; ++component) {
            if (!std::isfinite(values[component])) return false;
        }
    }
    return true;
}

bool positionsDiffer(const QVector<QVector3D>& lhs,
                     const QVector<QVector3D>& rhs)
{
    if (lhs.size() != rhs.size()) return true;
    for (qsizetype index = 0; index < lhs.size(); ++index) {
        if ((lhs[index] - rhs[index]).lengthSquared() > 1.0e-8f) {
            return true;
        }
    }
    return false;
}

bool hasFinitePositions(const QVector<QVector3D>& positions)
{
    for (const QVector3D& position : positions) {
        if (!std::isfinite(position.x()) || !std::isfinite(position.y()) ||
            !std::isfinite(position.z())) {
            return false;
        }
    }
    return true;
}

bool isEmbeddedTextureCachePath(const QString& path)
{
    const auto normalizedPath = [](QString value) {
        value = QDir::cleanPath(value);
        value.replace(QLatin1Char('\\'), QLatin1Char('/'));
        return value;
    };
    const QString tempPrefix = normalizedPath(QDir::tempPath()) +
        QLatin1Char('/');
    const QString absolutePath =
        normalizedPath(QFileInfo(path).absoluteFilePath());
    return absolutePath.startsWith(tempPrefix, Qt::CaseInsensitive) &&
        absolutePath.contains(QStringLiteral("ArtifactFbxTextures-"),
                              Qt::CaseInsensitive);
}

bool hasSupportedImageSignature(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    static constexpr char pngSignature[] = "\x89PNG\r\n\x1a\n";
    static constexpr char jpegSignature[] = "\xFF\xD8\xFF";
    const QByteArray signature = file.read(sizeof(pngSignature) - 1);
    return signature.startsWith(
               QByteArray(pngSignature, sizeof(pngSignature) - 1)) ||
        signature.startsWith(
            QByteArray(jpegSignature, sizeof(jpegSignature) - 1));
}

bool isFullyDecodedByRendererImporter(const QString& path)
{
    ImageImporter importer;
    if (!importer.open(path)) return false;

    const RawImage image = importer.readImage();
    if (!image.isValid() || image.width <= 0 || image.height <= 0 ||
        image.channels < 3) {
        return false;
    }
    const int bytesPerChannel = image.getPixelTypeSizeInBytes();
    if (bytesPerChannel <= 0) return false;
    const qint64 expectedByteCount = static_cast<qint64>(image.width) *
        image.height * image.channels * bytesPerChannel;
    return expectedByteCount > 0 &&
        expectedByteCount == image.data.size();
}

bool hasUsableUvSet(Mesh& mesh, const int uvSet)
{
    const auto attribute = mesh.vertexAttributes().get<QVector2D>(
        uvSet == 1 ? "uv1" : "uv");
    if (!attribute || attribute->size() != mesh.vertexCount()) return false;
    float minU = std::numeric_limits<float>::max();
    float minV = std::numeric_limits<float>::max();
    float maxU = std::numeric_limits<float>::lowest();
    float maxV = std::numeric_limits<float>::lowest();
    for (int index = 0; index < attribute->size(); ++index) {
        const QVector2D& uv = (*attribute)[index];
        if (!std::isfinite(uv.x()) || !std::isfinite(uv.y())) return false;
        minU = std::min(minU, uv.x());
        minV = std::min(minV, uv.y());
        maxU = std::max(maxU, uv.x());
        maxV = std::max(maxV, uv.y());
    }
    constexpr float minimumUvRange = 1.0e-5f;
    return maxU - minU > minimumUvRange && maxV - minV > minimumUvRange;
}

bool hasFiniteTextureTransform(
    const MeshImporter::ImportedPbrTextureTransform& transform)
{
    return std::isfinite(transform.scaleU) &&
        std::isfinite(transform.scaleV) &&
        std::isfinite(transform.rotationDegrees) &&
        std::isfinite(transform.offsetU) &&
        std::isfinite(transform.offsetV) &&
        transform.uvSet >= 0 && transform.uvSet <= 1;
}

bool isMaterialReferencedByMesh(const Mesh& mesh, const int materialIndex)
{
    const QVector<Mesh::MaterialSlot>& materialSlots = mesh.materialSlots();
    if (materialSlots.isEmpty()) return materialIndex == 0;
    for (const Mesh::MaterialSlot& slot : materialSlots) {
        if (slot.materialIndex == materialIndex) return true;
    }
    return false;
}

} // namespace

TEST(MeshImporterFbxTest, DoesNotClassifyDiffuseFactorAsPackedMetallicRoughness)
{
    const QString path = QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath(
        QStringLiteral("../fixtures/fbx-materials/diffuse-factor.fbx"));
    ASSERT_TRUE(QFileInfo(path).isFile());
    ufbx_error error{};
    ufbx_scene* source = ufbx_load_file(path.toUtf8().constData(), nullptr, &error);
    ASSERT_NE(source, nullptr) << error.description.data;
    ASSERT_EQ(source->materials.count, 1u);
    const bool hasFactorTexture = source->materials[0]->pbr.base_factor.texture != nullptr;
    ufbx_free_scene(source);
    ASSERT_TRUE(hasFactorTexture) << "The regression fixture must bind DiffuseFactor.";

    MeshImporter importer;
    auto mesh = importer.importMeshFromFile(UniString(path));
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    ASSERT_EQ(importer.importedPbrMaterials().size(), 1u);
    const auto& material = importer.importedPbrMaterials().front();
    EXPECT_TRUE(material.metallicRoughnessTexture.toQString().isEmpty());
    EXPECT_TRUE(material.metallicTexture.toQString().isEmpty());
    EXPECT_TRUE(material.roughnessTexture.toQString().isEmpty());
    EXPECT_TRUE(importer.lastMetallicRoughnessTexture().isEmpty());
    // A DiffuseFactor of 1 leaves the encoded diffuse color untouched.
    EXPECT_NEAR(material.baseColor[0], 0.79771f, 1.0e-4f);
}

TEST(MeshImporterFbxTest, AppliesDiffuseFactorAsBaseColorWeight)
{
    const QString path = QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath(
        QStringLiteral("../fixtures/fbx-materials/diffuse-factor-weight.fbx"));
    ASSERT_TRUE(QFileInfo(path).isFile());
    ufbx_error error{};
    ufbx_scene* source = ufbx_load_file(path.toUtf8().constData(), nullptr, &error);
    ASSERT_NE(source, nullptr) << error.description.data;
    ASSERT_EQ(source->materials.count, 1u);
    const ufbx_material* sourceMaterial = source->materials[0];
    const bool hasWeight = sourceMaterial->pbr.base_factor.has_value;
    const double weight = sourceMaterial->pbr.base_factor.value_real;
    ufbx_free_scene(source);
    ASSERT_TRUE(hasWeight) << "The fixture must declare DiffuseFactor.";

    MeshImporter importer;
    auto mesh = importer.importMeshFromFile(UniString(path));
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    ASSERT_EQ(importer.importedPbrMaterials().size(), 1u);
    const auto& material = importer.importedPbrMaterials().front();
    EXPECT_TRUE(material.hasBaseColorFactor);
    EXPECT_FLOAT_EQ(material.baseColorFactor, static_cast<float>(weight));
    // DiffuseColor 0.6/0.2 encoded to sRGB after being halved in linear space.
    EXPECT_NEAR(material.baseColor[0], 0.39886f, 1.0e-4f);
    EXPECT_NEAR(material.baseColor[1], 0.24228f, 1.0e-4f);
}

TEST(MeshImporterFbxTest, ExtractsCamerasAndLightsFromNodes)
{
    const QString path = QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath(
        QStringLiteral("../fixtures/fbx-materials/camera-light.fbx"));
    ASSERT_TRUE(QFileInfo(path).isFile());

    MeshImporter importer;
    auto mesh = importer.importMeshFromFile(UniString(path));
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();

    ASSERT_EQ(importer.importedCameras().size(), 1u);
    const MeshImporter::ImportedCamera& camera =
        importer.importedCameras().front();
    // The name comes from the node, not from its NodeAttribute.
    EXPECT_EQ(camera.name.toQString(), QStringLiteral("TestCamera"));
    EXPECT_EQ(camera.projection,
              MeshImporter::ImportedCameraProjection::Perspective);
    EXPECT_NEAR(camera.horizontalFovDegrees, 45.0f, 1.0e-3f);
    EXPECT_NEAR(camera.nearPlane, 1.0f, 1.0e-4f);
    EXPECT_NEAR(camera.farPlane, 1000.0f, 1.0e-3f);
    // Only the translation set is pinned. The fixture declares no axis
    // settings, so which axis each component lands on is not asserted.
    float translation[3] = {camera.transform[3], camera.transform[7],
                            camera.transform[11]};
    std::sort(translation, translation + 3);
    EXPECT_NEAR(translation[0], 10.0f, 1.0e-3f);
    EXPECT_NEAR(translation[1], 20.0f, 1.0e-3f);
    EXPECT_NEAR(translation[2], 30.0f, 1.0e-3f);

    ASSERT_EQ(importer.importedLights().size(), 1u);
    const MeshImporter::ImportedLight& light =
        importer.importedLights().front();
    EXPECT_EQ(light.name.toQString(), QStringLiteral("TestLight"));
    EXPECT_EQ(light.type, MeshImporter::ImportedLightType::Spot);
    EXPECT_NEAR(light.color[0], 1.0f, 1.0e-4f);
    EXPECT_NEAR(light.color[1], 0.8f, 1.0e-4f);
    EXPECT_NEAR(light.color[2], 0.6f, 1.0e-4f);
    // ufbx reports the FBX Intensity property divided by 100.
    EXPECT_NEAR(light.intensity, 1.5f, 1.0e-4f);
    EXPECT_NEAR(light.outerConeDegrees, 40.0f, 1.0e-3f);
    EXPECT_TRUE(light.castsShadows);
}

TEST(MeshImporterFbxTest, PreservesSeparateMetallicAndRoughnessMaps)
{
    const QString path = QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath(
        QStringLiteral("../fixtures/fbx-materials/separate-pbr.fbx"));
    ASSERT_TRUE(QFileInfo(path).isFile());
    ufbx_error error{};
    ufbx_scene* source = ufbx_load_file(path.toUtf8().constData(), nullptr, &error);
    ASSERT_NE(source, nullptr) << error.description.data;
    ASSERT_EQ(source->materials.count, 1u);
    const auto* sourceMaterial = source->materials[0];
    const bool hasMaps = sourceMaterial->pbr.metalness.texture &&
        sourceMaterial->pbr.roughness.texture;
    const auto shaderType = sourceMaterial->shader_type;
    ufbx_free_scene(source);
    ASSERT_EQ(shaderType, UFBX_SHADER_3DS_MAX_PBR_METAL_ROUGH);
    ASSERT_TRUE(hasMaps);

    MeshImporter importer;
    auto mesh = importer.importMeshFromFile(UniString(path));
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    ASSERT_EQ(importer.importedPbrMaterials().size(), 1u);
    const auto& material = importer.importedPbrMaterials().front();
    const QString metallicPath = material.metallicTexture.toQString();
    const QString roughnessPath = material.roughnessTexture.toQString();
    ASSERT_EQ(QFileInfo(metallicPath).fileName(), QStringLiteral("metallic.png"));
    ASSERT_EQ(QFileInfo(roughnessPath).fileName(), QStringLiteral("roughness.png"));
    EXPECT_TRUE(material.metallicRoughnessTexture.toQString().isEmpty());
    EXPECT_FLOAT_EQ(material.metallic, 1.0f);
    EXPECT_FLOAT_EQ(material.roughness, 1.0f);
    EXPECT_TRUE(hasFiniteTextureTransform(material.metallicUv));
    EXPECT_TRUE(hasFiniteTextureTransform(material.roughnessUv));
    ImageImporter imageImporter;
    ASSERT_TRUE(imageImporter.open(metallicPath));
    RawImage image = imageImporter.readImage();
    ASSERT_TRUE(image.isValid());
    EXPECT_EQ(image.data[0], 51);
    imageImporter.close();
    ASSERT_TRUE(imageImporter.open(roughnessPath));
    image = imageImporter.readImage();
    ASSERT_TRUE(image.isValid());
    EXPECT_EQ(image.data[0], 204);
    const QString referencePath = QFileInfo(path).dir().filePath(
        QStringLiteral("scalar-reference.fbx"));
    mesh = importer.importMeshFromFile(UniString(referencePath));
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(importer.importedPbrMaterials().size(), 1u);
    const auto& reference = importer.importedPbrMaterials().front();
    EXPECT_TRUE(reference.hasMetallic);
    EXPECT_TRUE(reference.hasRoughness);
    EXPECT_FLOAT_EQ(reference.metallic, 0.2f);
    EXPECT_FLOAT_EQ(reference.roughness, 0.8f);
    EXPECT_TRUE(reference.metallicTexture.toQString().isEmpty());
    EXPECT_TRUE(reference.roughnessTexture.toQString().isEmpty());
}

TEST(MeshImporterFbxTest, FindsExternalTextureBesideFbxTextureFolder)
{
    const QString path = qEnvironmentVariable(
        "ARTIFACT_FBX_EXTERNAL_TEXTURE_TEST_FILE").trimmed();
    ASSERT_TRUE(QFileInfo(path).isFile())
        << "External-texture fixture does not exist: " << path.toStdString();

    MeshImporter importer;
    const auto mesh = importer.importMeshFromFile(UniString(path));
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    ASSERT_EQ(importer.lastBackend(), MeshImporter::Backend::Ufbx);
    ASSERT_EQ(importer.importedPbrMaterials().size(), 1u);

    const MeshImporter::ImportedPbrMaterial& material =
        importer.importedPbrMaterials().front();
    QDir expectedDirectory(QFileInfo(path).absolutePath());
    ASSERT_TRUE(expectedDirectory.cdUp());
    const QString expectedTexture = expectedDirectory.filePath(
        QStringLiteral("Textures/external-base.png"));
    const QString importedTexture = material.baseColorTexture.toQString();
    EXPECT_EQ(QDir::cleanPath(importedTexture),
              QDir::cleanPath(expectedTexture));
    EXPECT_TRUE(QFileInfo(importedTexture).isFile());
    EXPECT_TRUE(hasSupportedImageSignature(importedTexture));
    EXPECT_TRUE(isFullyDecodedByRendererImporter(importedTexture));
    EXPECT_TRUE(hasUsableUvSet(*mesh, material.baseColorUv.uvSet));
    EXPECT_TRUE(isMaterialReferencedByMesh(*mesh, 0));
}

TEST(MeshImporterFbxTest, NormalizesFbxUvOriginForTopLeftImageRows)
{
    const QString path = fixturePath("ARTIFACT_FBX_TEXTURE_TEST_FILE");
    ASSERT_TRUE(QFileInfo(path).isFile());
    ufbx_error error{};
    ufbx_scene* source = ufbx_load_file(path.toUtf8().constData(), nullptr, &error);
    ASSERT_NE(source, nullptr) << error.description.data;
    struct SourceGuard {
        ufbx_scene* scene;
        ~SourceGuard() { ufbx_free_scene(scene); }
    } sourceGuard{source};

    MeshImporter importer;
    auto mesh = importer.importMeshFromFile(UniString(path));
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    const auto verifyOrigin = [&](Mesh& imported) {
        const auto uv = imported.vertexAttributes().get<QVector2D>("uv");
        const auto uv1 = imported.vertexAttributes().get<QVector2D>("uv1");
        ASSERT_NE(uv, nullptr);
        int destination = 0;
        int checked = 0;
        for (size_t meshIndex = 0; meshIndex < source->meshes.count; ++meshIndex) {
            const ufbx_mesh* sourceMesh = source->meshes[meshIndex];
            if (!sourceMesh) continue;
            for (size_t index = 0; index < sourceMesh->num_indices;
                 ++index, ++destination) {
                ASSERT_LT(destination, uv->size());
                if (!sourceMesh->vertex_uv.exists) continue;
                const auto sourceUv = ufbx_get_vertex_vec2(
                    &sourceMesh->vertex_uv, index);
                EXPECT_NEAR((*uv)[destination].x(), sourceUv.x, 1.0e-6);
                EXPECT_NEAR((*uv)[destination].y(), 1.0 - sourceUv.y, 1.0e-6);
                if (uv1 && sourceMesh->uv_sets.count > 1 &&
                    sourceMesh->uv_sets[1].vertex_uv.exists) {
                    const auto sourceUv1 = ufbx_get_vertex_vec2(
                        &sourceMesh->uv_sets[1].vertex_uv, index);
                    EXPECT_NEAR((*uv1)[destination].x(), sourceUv1.x, 1.0e-6);
                    EXPECT_NEAR((*uv1)[destination].y(), 1.0 - sourceUv1.y, 1.0e-6);
                }
                ++checked;
            }
        }
        EXPECT_EQ(destination, imported.vertexCount());
        EXPECT_GT(checked, 0);
    };
    verifyOrigin(*mesh);
    if (!mesh->skinAnimationClips().isEmpty()) {
        const auto& clip = mesh->skinAnimationClips().front();
        mesh = importer.importMeshFromFileAtTime(
            UniString(path), (clip.timeBegin + clip.timeEnd) * 0.5, 0);
        ASSERT_NE(mesh, nullptr);
        verifyOrigin(*mesh);
    }
}

TEST(MeshImporterFbxTest, EvaluatesAnimatedSkinPoseAcrossClipTimes)
{
    const QString path = fixturePath("ARTIFACT_FBX_ANIMATION_TEST_FILE");
    ASSERT_FALSE(path.isEmpty())
        << "Set ARTIFACT_FBX_ANIMATION_TEST_FILE to a skeletal FBX with at "
           "least one non-static animation clip, or set ARTIFACT_FBX_TEST_FILE "
           "for a shared fixture.";
    ASSERT_TRUE(QFileInfo(path).isFile())
        << "Animation FBX fixture does not exist: " << path.toStdString();

    MeshImporter importer;
    auto mesh = importer.importMeshFromFile(UniString(path));
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    ASSERT_EQ(importer.lastBackend(), MeshImporter::Backend::Ufbx);
    ASSERT_GT(mesh->vertexCount(), 0);
    ASSERT_GT(mesh->skinBones().size(), 0);
    ASSERT_FALSE(mesh->skinAnimationClips().isEmpty());
    const auto positionAttribute =
        mesh->vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positionAttribute, nullptr);
    ASSERT_EQ(positionAttribute->size(), mesh->vertexCount());

    bool observedPoseChange = false;
    bool observedVertexDeformation = false;
    for (qsizetype clipIndex = 0;
         clipIndex < mesh->skinAnimationClips().size(); ++clipIndex) {
        const Mesh::SkinAnimationClip& clip =
            mesh->skinAnimationClips()[clipIndex];
        ASSERT_TRUE(std::isfinite(clip.timeBegin));
        ASSERT_TRUE(std::isfinite(clip.timeEnd));
        const double duration = clip.timeEnd - clip.timeBegin;
        ASSERT_TRUE(std::isfinite(duration));
        if (!(duration > 1.0e-4)) continue;

        ASSERT_TRUE(importer.updateSkinPose(
            UniString(path), clip.timeBegin, static_cast<int>(clipIndex), *mesh))
            << "Could not evaluate clip " << clip.name.toStdString() << ": "
            << importer.lastError().toStdString();
        const QVector<QMatrix4x4> startPose = mesh->skinPoseMatrices();
        ASSERT_EQ(startPose.size(), mesh->skinBones().size());
        ASSERT_TRUE(hasFiniteMatrices(startPose));
        const QVector<QVector3D> startPositions = positionAttribute->data();
        ASSERT_TRUE(hasFinitePositions(startPositions));

        ASSERT_TRUE(importer.updateSkinPose(
            UniString(path), clip.timeBegin - duration,
            static_cast<int>(clipIndex), *mesh))
            << "Could not evaluate before the start of clip "
            << clip.name.toStdString() << ": "
            << importer.lastError().toStdString();
        const QVector<QMatrix4x4> beforeStartPose = mesh->skinPoseMatrices();
        const QVector<QVector3D> beforeStartPositions = positionAttribute->data();
        ASSERT_EQ(beforeStartPose.size(), mesh->skinBones().size());
        ASSERT_EQ(beforeStartPositions.size(), mesh->vertexCount());
        ASSERT_TRUE(hasFiniteMatrices(beforeStartPose));
        ASSERT_TRUE(hasFinitePositions(beforeStartPositions));
        EXPECT_FALSE(matricesDiffer(startPose, beforeStartPose))
            << "Clip evaluation before its start was not clamped.";
        EXPECT_FALSE(positionsDiffer(startPositions, beforeStartPositions))
            << "Vertex deformation before clip start was not clamped.";

        const double sampleTime = clip.timeBegin + duration * 0.5;
        ASSERT_TRUE(importer.updateSkinPose(
            UniString(path), sampleTime, static_cast<int>(clipIndex), *mesh))
            << "Could not evaluate clip " << clip.name.toStdString() << " at "
            << sampleTime << ": " << importer.lastError().toStdString();
        const QVector<QMatrix4x4> middlePose = mesh->skinPoseMatrices();
        const QVector<QVector3D> middlePositions = positionAttribute->data();
        ASSERT_EQ(middlePose.size(), mesh->skinBones().size());
        ASSERT_EQ(middlePositions.size(), mesh->vertexCount());
        ASSERT_TRUE(hasFiniteMatrices(middlePose));
        ASSERT_TRUE(hasFinitePositions(middlePositions));
        observedPoseChange = observedPoseChange ||
            matricesDiffer(startPose, middlePose);
        observedVertexDeformation = observedVertexDeformation ||
            positionsDiffer(startPositions, middlePositions);

        ASSERT_TRUE(importer.updateSkinPose(
            UniString(path), clip.timeEnd, static_cast<int>(clipIndex), *mesh))
            << "Could not evaluate clip " << clip.name.toStdString()
            << " at its end time: " << importer.lastError().toStdString();
        const QVector<QMatrix4x4> endPose = mesh->skinPoseMatrices();
        const QVector<QVector3D> endPositions = positionAttribute->data();
        ASSERT_EQ(endPose.size(), mesh->skinBones().size());
        ASSERT_EQ(endPositions.size(), mesh->vertexCount());
        ASSERT_TRUE(hasFiniteMatrices(endPose));
        ASSERT_TRUE(hasFinitePositions(endPositions));
        observedPoseChange = observedPoseChange ||
            matricesDiffer(startPose, endPose);
        observedVertexDeformation = observedVertexDeformation ||
            positionsDiffer(startPositions, endPositions);

        ASSERT_TRUE(importer.updateSkinPose(
            UniString(path), clip.timeEnd + duration,
            static_cast<int>(clipIndex), *mesh))
            << "Could not evaluate after the end of clip "
            << clip.name.toStdString() << ": "
            << importer.lastError().toStdString();
        const QVector<QMatrix4x4> afterEndPose = mesh->skinPoseMatrices();
        const QVector<QVector3D> afterEndPositions = positionAttribute->data();
        ASSERT_EQ(afterEndPose.size(), mesh->skinBones().size());
        ASSERT_EQ(afterEndPositions.size(), mesh->vertexCount());
        ASSERT_TRUE(hasFiniteMatrices(afterEndPose));
        ASSERT_TRUE(hasFinitePositions(afterEndPositions));
        EXPECT_FALSE(matricesDiffer(endPose, afterEndPose))
            << "Clip evaluation after its end was not clamped.";
        EXPECT_FALSE(positionsDiffer(endPositions, afterEndPositions))
            << "Vertex deformation after clip end was not clamped.";

        ASSERT_TRUE(importer.updateSkinPose(
            UniString(path), clip.timeBegin, static_cast<int>(clipIndex), *mesh))
            << "Could not re-evaluate the start of clip "
            << clip.name.toStdString() << " after sampling its full range: "
            << importer.lastError().toStdString();
        EXPECT_FALSE(matricesDiffer(startPose, mesh->skinPoseMatrices()))
            << "Re-evaluating the clip start changed the pose after later samples.";
        EXPECT_FALSE(positionsDiffer(startPositions, positionAttribute->data()))
            << "Re-evaluating the clip start accumulated vertex deformation.";
    }

    EXPECT_TRUE(observedPoseChange)
        << "No animation clip changed the evaluated skin pose. Use a fixture "
           "with a non-static skeletal animation.";
    EXPECT_TRUE(observedVertexDeformation)
        << "Animation changed the pose palette but not the mesh positions. "
           "Use a fixture with vertices weighted to the animated skeleton.";
}

TEST(MeshImporterFbxTest,
     GeneratesPoseConsistentMeshletLODWithoutMutatingAnimatedFbxPose)
{
    const QString path = fixturePath("ARTIFACT_FBX_ANIMATION_TEST_FILE");
    ASSERT_FALSE(path.isEmpty())
        << "Set ARTIFACT_FBX_ANIMATION_TEST_FILE to a skeletal FBX with at "
           "least one non-static animation clip, or set ARTIFACT_FBX_TEST_FILE "
           "for a shared fixture.";
    ASSERT_TRUE(QFileInfo(path).isFile())
        << "Animation FBX fixture does not exist: " << path.toStdString();

    MeshImporter importer;
    auto mesh = importer.importMeshFromFile(UniString(path));
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    ASSERT_EQ(importer.lastBackend(), MeshImporter::Backend::Ufbx);
    ASSERT_GT(mesh->vertexCount(), 0);
    ASSERT_GT(mesh->polygonCount(), 0);
    ASSERT_GT(mesh->skinBones().size(), 0);
    ASSERT_FALSE(mesh->skinAnimationClips().isEmpty());

    const auto positionAttribute =
        mesh->vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positionAttribute, nullptr);
    ASSERT_EQ(positionAttribute->size(), mesh->vertexCount());

    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 4;
    config.lodSwitchPixels = 96.0f;
    const auto verifyLODAtTime = [&](const double time,
                                     const int clipIndex,
                                     QVector<QVector3D>* lodPositions,
                                     QVector<unsigned int>* lodIndices,
                                     QVector<Mesh::MeshletLODLevel>* lodLevels,
                                     QVector<QVector2D>* lodUvs,
                                     QVector<QVector2D>* lodUvs1) {
        ASSERT_TRUE(importer.updateSkinPose(
            UniString(path), time, clipIndex, *mesh))
            << "Could not evaluate FBX animation for LOD generation: "
            << importer.lastError().toStdString();
        const QVector<QVector3D> positionsBefore = positionAttribute->data();
        const Mesh::MeshletLODData lod = mesh->generateMeshletLODData(config);
        ASSERT_TRUE(lod.isValid());
        ASSERT_FALSE(lod.levels.isEmpty());
        ASSERT_GT(lod.renderData.positions.size(), 0);
        if (lodPositions) *lodPositions = lod.renderData.positions;
        if (lodIndices) *lodIndices = lod.lodIndices;
        if (lodLevels) *lodLevels = lod.levels;
        if (lodUvs) *lodUvs = lod.renderData.uvs;
        if (lodUvs1) *lodUvs1 = lod.renderData.uvs1;

        float previousSwitchDistance =
            std::numeric_limits<float>::infinity();
        unsigned int previousIndexCount =
            std::numeric_limits<unsigned int>::max();
        int previousTriangleStride = 0;
        for (qsizetype levelIndex = 0; levelIndex < lod.levels.size();
             ++levelIndex) {
            const Mesh::MeshletLODLevel& level = lod.levels[levelIndex];
            ASSERT_GT(level.indexCount, 0u);
            EXPECT_EQ(level.indexCount % 3u, 0u);
            EXPECT_LE(level.indexCount, previousIndexCount)
                << "coarser animated-mesh LODs must not add triangles";
            EXPECT_GE(level.triangleStride, previousTriangleStride)
                << "coarser animated-mesh LODs must not reduce triangle stride";
            EXPECT_TRUE(std::isfinite(level.switchDistancePixels));
            EXPECT_LE(level.firstIndex + level.indexCount,
                      static_cast<unsigned int>(lod.lodIndices.size()));
            EXPECT_GE(level.meshletOffset, 0);
            EXPECT_LE(level.meshletOffset + level.meshletCount,
                      lod.meshlets.size());
            EXPECT_GE(level.switchDistancePixels, 0.0f);
            EXPECT_LE(level.switchDistancePixels, previousSwitchDistance);
            previousSwitchDistance = level.switchDistancePixels;
            previousIndexCount = level.indexCount;
            previousTriangleStride = level.triangleStride;
            EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                          lod, level.switchDistancePixels), level.level);
            if (levelIndex > 0) {
                const float boundary = level.switchDistancePixels;
                EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                              lod, std::nextafter(
                                       boundary,
                                       std::numeric_limits<float>::infinity())),
                          lod.levels[levelIndex - 1].level)
                    << "a radius immediately above this animated-mesh LOD "
                       "boundary should select the adjacent finer level";
                EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                              lod, std::nextafter(
                                       boundary,
                                       -std::numeric_limits<float>::infinity())),
                          level.level)
                    << "a radius immediately below this animated-mesh LOD "
                       "boundary should remain at this level";
            }

            for (unsigned int offset = level.firstIndex;
                 offset < level.firstIndex + level.indexCount; ++offset) {
                EXPECT_LT(lod.lodIndices[offset],
                          static_cast<unsigned int>(lod.renderData.positions.size()));
            }

            for (int meshletIndex = level.meshletOffset;
                 meshletIndex < level.meshletOffset + level.meshletCount;
                 ++meshletIndex) {
                const Mesh::Meshlet& meshlet = lod.meshlets[meshletIndex];
                ASSERT_GT(meshlet.indexCount, 0u);
                ASSERT_EQ(meshlet.indexCount % 3u, 0u);
                ASSERT_GE(meshlet.firstIndex, level.firstIndex);
                ASSERT_LE(meshlet.firstIndex + meshlet.indexCount,
                          level.firstIndex + level.indexCount);
                EXPECT_TRUE(std::isfinite(meshlet.boundsRadius));
                EXPECT_GE(meshlet.boundsRadius, 0.0f);

                for (unsigned int offset = meshlet.firstIndex;
                     offset < meshlet.firstIndex + meshlet.indexCount;
                     ++offset) {
                    const unsigned int vertexIndex = lod.lodIndices[offset];
                    ASSERT_LT(vertexIndex,
                              static_cast<unsigned int>(
                                  lod.renderData.positions.size()));
                    const QVector3D& position =
                        lod.renderData.positions[static_cast<qsizetype>(
                            vertexIndex)];
                    EXPECT_GE(position.x(), meshlet.boundsMin.x() - 1.0e-6f);
                    EXPECT_GE(position.y(), meshlet.boundsMin.y() - 1.0e-6f);
                    EXPECT_GE(position.z(), meshlet.boundsMin.z() - 1.0e-6f);
                    EXPECT_LE(position.x(), meshlet.boundsMax.x() + 1.0e-6f);
                    EXPECT_LE(position.y(), meshlet.boundsMax.y() + 1.0e-6f);
                    EXPECT_LE(position.z(), meshlet.boundsMax.z() + 1.0e-6f);
                    EXPECT_LE((position - meshlet.boundsCenter).length(),
                              meshlet.boundsRadius + 1.0e-5f);
                }
            }
        }

        EXPECT_FALSE(positionsDiffer(positionsBefore,
                                     positionAttribute->data()))
            << "Generating meshlet LOD must not mutate the animated FBX pose";
    };

    bool foundAnimatedClip = false;
    int firstVisitedClipIndex = -1;
    double firstVisitedClipStartTime = 0.0;
    QVector<QVector3D> firstVisitedClipStartLodPositions;
    QVector<unsigned int> firstVisitedClipStartLodIndices;
    QVector<QVector2D> firstVisitedClipStartLodUvs;
    QVector<QVector2D> firstVisitedClipStartLodUvs1;
    for (qsizetype clipIndex = 0;
         clipIndex < mesh->skinAnimationClips().size(); ++clipIndex) {
        const Mesh::SkinAnimationClip& clip =
            mesh->skinAnimationClips()[clipIndex];
        const double duration = clip.timeEnd - clip.timeBegin;
        if (!std::isfinite(duration) || duration <= 1.0e-4) continue;

        const QVector<QVector3D> startPositions = [&] {
            const bool updated = importer.updateSkinPose(
                UniString(path), clip.timeBegin,
                static_cast<int>(clipIndex), *mesh);
            EXPECT_TRUE(updated) << importer.lastError().toStdString();
            return positionAttribute->data();
        }();
        if (startPositions.isEmpty()) continue;
        QVector<QVector3D> startLodPositions;
        QVector<unsigned int> startLodIndices;
        QVector<Mesh::MeshletLODLevel> startLodLevels;
        QVector<QVector2D> startLodUvs;
        QVector<QVector2D> startLodUvs1;
        verifyLODAtTime(clip.timeBegin, static_cast<int>(clipIndex),
                        &startLodPositions, &startLodIndices,
                        &startLodLevels, &startLodUvs, &startLodUvs1);
        if (firstVisitedClipIndex < 0) {
            firstVisitedClipIndex = static_cast<int>(clipIndex);
            firstVisitedClipStartTime = clip.timeBegin;
            firstVisitedClipStartLodPositions = startLodPositions;
            firstVisitedClipStartLodIndices = startLodIndices;
            firstVisitedClipStartLodUvs = startLodUvs;
            firstVisitedClipStartLodUvs1 = startLodUvs1;
        }

        const double middleTime = clip.timeBegin + duration * 0.5;
        const bool middleUpdated = importer.updateSkinPose(
            UniString(path), middleTime, static_cast<int>(clipIndex), *mesh);
        ASSERT_TRUE(middleUpdated) << importer.lastError().toStdString();
        if (!positionsDiffer(startPositions, positionAttribute->data())) continue;

        QVector<QVector3D> middleLodPositions;
        QVector<unsigned int> middleLodIndices;
        QVector<Mesh::MeshletLODLevel> middleLodLevels;
        QVector<QVector2D> middleLodUvs;
        QVector<QVector2D> middleLodUvs1;
        verifyLODAtTime(middleTime, static_cast<int>(clipIndex),
                        &middleLodPositions, &middleLodIndices,
                        &middleLodLevels, &middleLodUvs, &middleLodUvs1);
        EXPECT_TRUE(positionsDiffer(startLodPositions, middleLodPositions))
            << "Generated meshlet LOD positions should follow the animated skin pose";
        EXPECT_EQ(middleLodIndices, startLodIndices)
            << "Skin animation should not change the meshlet LOD topology";
        EXPECT_EQ(middleLodUvs, startLodUvs)
            << "skin animation should not change the generated LOD's UV0 mapping";
        EXPECT_EQ(middleLodUvs1, startLodUvs1)
            << "skin animation should not change the generated LOD's UV1 mapping";
        ASSERT_EQ(middleLodLevels.size(), startLodLevels.size());
        for (qsizetype levelIndex = 0;
             levelIndex < startLodLevels.size(); ++levelIndex) {
            const Mesh::MeshletLODLevel& startLevel =
                startLodLevels[levelIndex];
            const Mesh::MeshletLODLevel& middleLevel =
                middleLodLevels[levelIndex];
            EXPECT_EQ(middleLevel.level, startLevel.level);
            EXPECT_EQ(middleLevel.meshletOffset, startLevel.meshletOffset);
            EXPECT_EQ(middleLevel.meshletCount, startLevel.meshletCount);
            EXPECT_EQ(middleLevel.firstIndex, startLevel.firstIndex);
            EXPECT_EQ(middleLevel.indexCount, startLevel.indexCount);
            EXPECT_EQ(middleLevel.triangleStride, startLevel.triangleStride);
            EXPECT_FLOAT_EQ(middleLevel.switchDistancePixels,
                            startLevel.switchDistancePixels)
                << "animation pose should not change the LOD switch boundary";
        }

        QVector<QVector3D> endLodPositions;
        QVector<unsigned int> endLodIndices;
        QVector<Mesh::MeshletLODLevel> endLodLevels;
        QVector<QVector2D> endLodUvs;
        QVector<QVector2D> endLodUvs1;
        verifyLODAtTime(clip.timeEnd, static_cast<int>(clipIndex),
                        &endLodPositions, &endLodIndices, &endLodLevels,
                        &endLodUvs, &endLodUvs1);
        EXPECT_EQ(endLodIndices, startLodIndices)
            << "the clip endpoint should preserve the generated LOD topology";
        EXPECT_EQ(endLodUvs, startLodUvs)
            << "the clip endpoint should preserve generated LOD UV0 mapping";
        EXPECT_EQ(endLodUvs1, startLodUvs1)
            << "the clip endpoint should preserve generated LOD UV1 mapping";
        ASSERT_EQ(endLodLevels.size(), startLodLevels.size());
        for (qsizetype levelIndex = 0;
             levelIndex < startLodLevels.size(); ++levelIndex) {
            const Mesh::MeshletLODLevel& startLevel =
                startLodLevels[levelIndex];
            const Mesh::MeshletLODLevel& endLevel =
                endLodLevels[levelIndex];
            EXPECT_EQ(endLevel.level, startLevel.level);
            EXPECT_EQ(endLevel.firstIndex, startLevel.firstIndex);
            EXPECT_EQ(endLevel.indexCount, startLevel.indexCount);
            EXPECT_EQ(endLevel.triangleStride, startLevel.triangleStride);
            EXPECT_FLOAT_EQ(endLevel.switchDistancePixels,
                            startLevel.switchDistancePixels)
                << "clip-end pose should keep the same LOD switch boundaries";
        }

        QVector<QVector3D> restoredLodPositions;
        QVector<unsigned int> restoredLodIndices;
        QVector<Mesh::MeshletLODLevel> restoredLodLevels;
        QVector<QVector2D> restoredLodUvs;
        QVector<QVector2D> restoredLodUvs1;
        verifyLODAtTime(clip.timeBegin, static_cast<int>(clipIndex),
                        &restoredLodPositions, &restoredLodIndices,
                        &restoredLodLevels, &restoredLodUvs,
                        &restoredLodUvs1);
        EXPECT_FALSE(positionsDiffer(startLodPositions, restoredLodPositions))
            << "Returning to the clip start should reproduce its meshlet LOD geometry";
        EXPECT_EQ(restoredLodIndices, startLodIndices)
            << "Regenerating the same pose should reproduce the LOD index table";
        EXPECT_EQ(restoredLodUvs, startLodUvs);
        EXPECT_EQ(restoredLodUvs1, startLodUvs1);
        ASSERT_EQ(restoredLodLevels.size(), startLodLevels.size());
        for (qsizetype levelIndex = 0;
             levelIndex < startLodLevels.size(); ++levelIndex) {
            const Mesh::MeshletLODLevel& startLevel =
                startLodLevels[levelIndex];
            const Mesh::MeshletLODLevel& restoredLevel =
                restoredLodLevels[levelIndex];
            EXPECT_EQ(restoredLevel.level, startLevel.level);
            EXPECT_EQ(restoredLevel.meshletOffset, startLevel.meshletOffset);
            EXPECT_EQ(restoredLevel.meshletCount, startLevel.meshletCount);
            EXPECT_EQ(restoredLevel.firstIndex, startLevel.firstIndex);
            EXPECT_EQ(restoredLevel.indexCount, startLevel.indexCount);
            EXPECT_EQ(restoredLevel.triangleStride, startLevel.triangleStride);
            EXPECT_FLOAT_EQ(restoredLevel.switchDistancePixels,
                            startLevel.switchDistancePixels);
        }
        foundAnimatedClip = true;
    }

    EXPECT_TRUE(foundAnimatedClip)
        << "The FBX fixture needs a clip that deforms skinned vertices so LOD "
           "generation is checked at multiple poses.";
    if (firstVisitedClipIndex >= 0) {
        QVector<QVector3D> restoredFirstClipLodPositions;
        QVector<unsigned int> restoredFirstClipLodIndices;
        QVector<Mesh::MeshletLODLevel> restoredFirstClipLodLevels;
        QVector<QVector2D> restoredFirstClipLodUvs;
        QVector<QVector2D> restoredFirstClipLodUvs1;
        verifyLODAtTime(firstVisitedClipStartTime,
                        firstVisitedClipIndex,
                        &restoredFirstClipLodPositions,
                        &restoredFirstClipLodIndices,
                        &restoredFirstClipLodLevels,
                        &restoredFirstClipLodUvs,
                        &restoredFirstClipLodUvs1);
        EXPECT_FALSE(positionsDiffer(firstVisitedClipStartLodPositions,
                                     restoredFirstClipLodPositions))
            << "Returning to the first animated clip after visiting the other "
               "clips should reproduce its LOD geometry";
        EXPECT_EQ(restoredFirstClipLodIndices,
                  firstVisitedClipStartLodIndices)
            << "switching between clips should not leave stale LOD topology";
        EXPECT_EQ(restoredFirstClipLodUvs, firstVisitedClipStartLodUvs);
        EXPECT_EQ(restoredFirstClipLodUvs1, firstVisitedClipStartLodUvs1);
    }
}

TEST(MeshImporterFbxTest,
     GeneratesDeterministicMeshletLODFromImportedFbxPose)
{
    const QString path = fixturePath("ARTIFACT_FBX_ANIMATION_TEST_FILE");
    ASSERT_FALSE(path.isEmpty())
        << "Set ARTIFACT_FBX_ANIMATION_TEST_FILE to an importable FBX fixture.";
    ASSERT_TRUE(QFileInfo(path).isFile())
        << "FBX fixture does not exist: " << path.toStdString();

    MeshImporter importer;
    auto mesh = importer.importMeshFromFile(UniString(path));
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    ASSERT_EQ(importer.lastBackend(), MeshImporter::Backend::Ufbx);
    ASSERT_GT(mesh->vertexCount(), 0);
    ASSERT_GT(mesh->polygonCount(), 0);

    const auto positions = mesh->vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positions, nullptr);
    const QVector<QVector3D> sourcePositions = positions->data();
    ASSERT_EQ(sourcePositions.size(), mesh->vertexCount());
    const Mesh::RenderData sourceRenderData = mesh->generateRenderData();
    ASSERT_FALSE(sourceRenderData.positions.isEmpty());

    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 4;
    config.maxTrianglesPerMeshlet = 24;
    const Mesh::MeshletLODData first = mesh->generateMeshletLODData(config);
    const Mesh::MeshletLODData repeated = mesh->generateMeshletLODData(config);
    ASSERT_TRUE(first.isValid());
    ASSERT_FALSE(first.levels.isEmpty());
    EXPECT_LE(first.levels.size(), static_cast<qsizetype>(config.maxLODLevels));
    EXPECT_EQ(repeated.levels.size(), first.levels.size());
    EXPECT_EQ(repeated.lodIndices, first.lodIndices);
    EXPECT_EQ(repeated.renderData.positions, first.renderData.positions);
    EXPECT_FALSE(positionsDiffer(sourcePositions, positions->data()))
        << "generating imported-FBX meshlet LOD must not mutate the source pose";

    const auto verifyAttributeAlignment = [&](const auto& sourceAttribute,
                                              const auto& lodAttribute,
                                              const char* attributeName) {
        if (sourceAttribute.isEmpty()) {
            EXPECT_TRUE(lodAttribute.isEmpty()) << attributeName;
            return;
        }
        ASSERT_EQ(sourceAttribute.size(), sourceRenderData.positions.size())
            << attributeName;
        ASSERT_EQ(lodAttribute.size(), first.renderData.positions.size())
            << attributeName;
    };
    verifyAttributeAlignment(sourceRenderData.normals, first.renderData.normals,
                             "normal");
    verifyAttributeAlignment(sourceRenderData.tangents, first.renderData.tangents,
                             "tangent");
    verifyAttributeAlignment(sourceRenderData.uvs, first.renderData.uvs, "uv");
    verifyAttributeAlignment(sourceRenderData.uvs1, first.renderData.uvs1,
                             "uv1");
    if (sourceRenderData.colors.isEmpty()) {
        ASSERT_EQ(first.renderData.colors.size(), first.renderData.positions.size());
        for (const QVector4D& color : first.renderData.colors) {
            EXPECT_EQ(color, QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
        }
    } else {
        verifyAttributeAlignment(sourceRenderData.colors, first.renderData.colors,
                                 "color");
    }

    int previousTriangleStride = 0;
    for (qsizetype levelIndex = 0; levelIndex < first.levels.size();
         ++levelIndex) {
        const Mesh::MeshletLODLevel& level = first.levels[levelIndex];
        EXPECT_EQ(level.level, static_cast<int>(levelIndex));
        EXPECT_GE(level.triangleStride, previousTriangleStride);
        previousTriangleStride = level.triangleStride;
        ASSERT_GT(level.indexCount, 0u);
        EXPECT_EQ(level.indexCount % 3u, 0u);
        EXPECT_LE(level.firstIndex + level.indexCount,
                  static_cast<unsigned int>(first.lodIndices.size()));
        EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                      first, level.switchDistancePixels), level.level);
        for (unsigned int offset = level.firstIndex;
             offset < level.firstIndex + level.indexCount; ++offset) {
            EXPECT_LT(first.lodIndices[offset],
                      static_cast<unsigned int>(first.renderData.positions.size()));
        }
    }
}

TEST(MeshImporterFbxTest, ResolvesEmbeddedBaseColorImageAndUv)
{
    const QString path = fixturePath("ARTIFACT_FBX_BASE_COLOR_TEST_FILE");
    ASSERT_FALSE(path.isEmpty())
        << "Set ARTIFACT_FBX_BASE_COLOR_TEST_FILE to an FBX with an embedded "
           "base-color PNG/JPEG, or set ARTIFACT_FBX_TEXTURE_TEST_FILE "
           "for a shared fixture.";
    ASSERT_TRUE(QFileInfo(path).isFile())
        << "Base-color FBX fixture does not exist: " << path.toStdString();

    MeshImporter importer;
    auto mesh = importer.importMeshFromFile(UniString(path));
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    ASSERT_EQ(importer.lastBackend(), MeshImporter::Backend::Ufbx);
    ASSERT_GT(mesh->vertexCount(), 0);

    bool foundEmbeddedBaseColor = false;
    int materialIndex = 0;
    std::ostringstream materialDiagnostics;
    for (const MeshImporter::ImportedPbrMaterial& material :
         importer.importedPbrMaterials()) {
        const QString texturePath = material.baseColorTexture.toQString();
        materialDiagnostics << "material[" << materialIndex << "] name="
            << material.name.toQString().toStdString() << " base="
            << texturePath.toStdString() << " exists="
            << QFileInfo(texturePath).isFile() << " embedded="
            << isEmbeddedTextureCachePath(texturePath) << " signature="
            << hasSupportedImageSignature(texturePath) << " decoded="
            << isFullyDecodedByRendererImporter(texturePath) << " uvSet="
            << material.baseColorUv.uvSet << " uvValid="
            << hasUsableUvSet(*mesh, material.baseColorUv.uvSet)
            << " baseFactor=" << material.baseColor[0] << ','
            << material.baseColor[1] << ',' << material.baseColor[2]
            << " hasBaseFactor=" << material.hasBaseColor
            << " metallic=" << material.metallic << " hasMetallic=" << material.hasMetallic
            << " roughness=" << material.roughness << " hasRoughness=" << material.hasRoughness
            << " metallicRoughnessMap=" << material.metallicRoughnessTexture.toQString().toStdString()
            << " specular=" << material.specular << " clearcoat=" << material.clearcoat << '\n';
        if (!QFileInfo(texturePath).isFile() ||
            !isEmbeddedTextureCachePath(texturePath) ||
            !hasSupportedImageSignature(texturePath) ||
            !isFullyDecodedByRendererImporter(texturePath)) {
            ++materialIndex;
            continue;
        }
        EXPECT_TRUE(hasUsableUvSet(*mesh, material.baseColorUv.uvSet))
            << "Embedded base-color texture has no usable UV set.";
        EXPECT_TRUE(hasFiniteTextureTransform(material.baseColorUv))
            << "Embedded base-color texture has an invalid UV transform.";
        EXPECT_TRUE(isMaterialReferencedByMesh(*mesh, materialIndex))
            << "No mesh material slot references the material with the "
               "embedded base-color texture.";
        foundEmbeddedBaseColor = true;
        break;
    }
    EXPECT_TRUE(foundEmbeddedBaseColor)
        << "No embedded base-color PNG/JPEG was extracted and fully decoded "
           "through the importer and Renderer ImageImporter. Materials:\n"
        << materialDiagnostics.str();
    RecordProperty("materialDiagnostics", materialDiagnostics.str());
}

TEST(MeshImporterFbxTest, ResolvesEmbeddedBaseColorAndNormalTextures)
{
    const QString path = fixturePath("ARTIFACT_FBX_TEXTURE_TEST_FILE");
    ASSERT_FALSE(path.isEmpty())
        << "Set ARTIFACT_FBX_TEXTURE_TEST_FILE to an FBX with embedded "
           "base-color and normal PNG/JPEG textures, or set "
           "ARTIFACT_FBX_TEST_FILE for a shared fixture.";
    ASSERT_TRUE(QFileInfo(path).isFile())
        << "Texture FBX fixture does not exist: " << path.toStdString();

    MeshImporter importer;
    auto mesh = importer.importMeshFromFile(UniString(path));
    ASSERT_NE(mesh, nullptr) << importer.lastError().toStdString();
    ASSERT_EQ(importer.lastBackend(), MeshImporter::Backend::Ufbx);
    ASSERT_GT(mesh->vertexCount(), 0);

    bool foundEmbeddedBaseColor = false;
    bool foundEmbeddedNormal = false;
    bool foundBaseColorUv = false;
    bool foundNormalUv = false;
    bool foundBaseColorTransform = false;
    bool foundNormalTransform = false;
    bool foundAssignedBaseColorMaterial = false;
    bool foundAssignedNormalMaterial = false;
    const auto& materials = importer.importedPbrMaterials();
    ASSERT_FALSE(materials.empty());
    std::ostringstream materialDiagnostics;
    for (const Mesh::MaterialSlot& slot : mesh->materialSlots()) {
        EXPECT_GE(slot.firstIndex, 0);
        EXPECT_GT(slot.indexCount, 0);
        EXPECT_GE(slot.materialIndex, 0);
        EXPECT_LT(slot.materialIndex, static_cast<int>(materials.size()));
    }
    int materialIndex = 0;
    for (const MeshImporter::ImportedPbrMaterial& material : materials) {
        const QString baseColorPath = material.baseColorTexture.toQString();
        const QString normalPath = material.normalTexture.toQString();
        const auto isResolvedImage = [](const QString& texturePath) {
            const QFileInfo info(texturePath);
            return info.isFile() && info.size() > 8 &&
                isEmbeddedTextureCachePath(texturePath) &&
                hasSupportedImageSignature(texturePath) &&
                isFullyDecodedByRendererImporter(texturePath);
        };
        const bool hasBaseColor = isResolvedImage(baseColorPath);
        const bool hasNormal = isResolvedImage(normalPath);
        materialDiagnostics << "material[" << materialIndex << "] name="
            << material.name.toQString().toStdString() << " base="
            << baseColorPath.toStdString() << " normal="
            << normalPath.toStdString() << " baseResolved=" << hasBaseColor
            << " normalResolved=" << hasNormal << " baseUv="
            << material.baseColorUv.uvSet << " normalUv="
            << material.normalUv.uvSet << '\n';
        foundEmbeddedBaseColor = foundEmbeddedBaseColor || hasBaseColor;
        foundEmbeddedNormal = foundEmbeddedNormal || hasNormal;
        foundBaseColorUv = foundBaseColorUv || (hasBaseColor &&
            hasUsableUvSet(*mesh, material.baseColorUv.uvSet));
        foundNormalUv = foundNormalUv || (hasNormal &&
            hasUsableUvSet(*mesh, material.normalUv.uvSet));
        foundBaseColorTransform = foundBaseColorTransform || (hasBaseColor &&
            hasFiniteTextureTransform(material.baseColorUv));
        foundNormalTransform = foundNormalTransform || (hasNormal &&
            hasFiniteTextureTransform(material.normalUv));
        foundAssignedBaseColorMaterial = foundAssignedBaseColorMaterial ||
            (hasBaseColor &&
             isMaterialReferencedByMesh(*mesh, materialIndex));
        foundAssignedNormalMaterial = foundAssignedNormalMaterial ||
            (hasNormal && isMaterialReferencedByMesh(*mesh, materialIndex));
        ++materialIndex;
    }

    EXPECT_TRUE(foundEmbeddedBaseColor)
        << "No embedded base-color PNG/JPEG was extracted to the importer "
           "texture cache.";
    EXPECT_TRUE(foundEmbeddedNormal)
        << "No embedded normal PNG/JPEG was extracted to the importer texture cache.";
    EXPECT_TRUE(foundBaseColorUv)
        << "Embedded base-color texture has no finite, non-degenerate imported UV set.";
    EXPECT_TRUE(foundNormalUv)
        << "Embedded normal texture has no finite, non-degenerate imported UV set.";
    EXPECT_TRUE(foundBaseColorTransform)
        << "Embedded base-color texture has an invalid UV transform.";
    EXPECT_TRUE(foundNormalTransform)
        << "Embedded normal texture has an invalid UV transform.";
    EXPECT_TRUE(foundAssignedBaseColorMaterial)
        << "No mesh material slot references a material with the embedded "
           "base-color texture.";
    EXPECT_TRUE(foundAssignedNormalMaterial)
        << "No mesh material slot references a material with the embedded "
           "normal texture. Materials:\n" << materialDiagnostics.str();
}
