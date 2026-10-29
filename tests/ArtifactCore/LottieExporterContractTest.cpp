#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTemporaryDir>
#include <QVector2D>

#include <any>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

import Export.Lottie.Exporter;
import Export.Lottie.RigExporter;
import Export.Lottie.Types;
import ArtifactCore.Rig2D;

namespace {

using namespace ArtifactCore::Export::Lottie;

LottieDocument validDocument()
{
    LottieDocument document;
    document.fr = 24.0;
    document.ip = 0;
    document.op = 24;
    document.w = 1280;
    document.h = 720;
    document.nm = "contract";

    LottieLayer layer;
    layer.ind = 1;
    layer.ty = 4;
    layer.nm = "shape";
    layer.ip = 0;
    layer.op = 24;
    layer.position.k = {320.0, 180.0};
    LottieShapeRect rectangle;
    rectangle.p.k = {0.0, 0.0};
    rectangle.s.k = {80.0, 40.0};
    rectangle.r.k = {6.0};
    layer.shapes.emplace_back(rectangle);
    document.layers.push_back(layer);
    return document;
}

LottieKeyframe keyframe(double time, double value, int hold = 0)
{
    LottieKeyframe result;
    result.t = time;
    result.s = {value};
    result.e = {value};
    result.hold = hold;
    return result;
}

} // namespace

TEST(LottieExporterContractTest,
     KeyframeCompressionRemovesOnlyWithinToleranceLinearSamples)
{
    std::vector<LottieKeyframe> keyframes{
        keyframe(0.0, 0.0), keyframe(5.0, 5.005), keyframe(10.0, 10.0)};

    LottieExporter::compressKeyframes(keyframes, 0.01);

    ASSERT_EQ(keyframes.size(), 2u);
    EXPECT_DOUBLE_EQ(keyframes[0].t, 0.0);
    EXPECT_DOUBLE_EQ(keyframes[1].t, 10.0);

    std::vector<LottieKeyframe> nonlinear{
        keyframe(0.0, 0.0), keyframe(5.0, 5.02), keyframe(10.0, 10.0)};
    LottieExporter::compressKeyframes(nonlinear, 0.01);
    ASSERT_EQ(nonlinear.size(), 3u);

    std::vector<LottieKeyframe> held{
        keyframe(0.0, 0.0), keyframe(5.0, 5.0, 1), keyframe(10.0, 10.0)};
    LottieExporter::compressKeyframes(held, 1.0);
    ASSERT_EQ(held.size(), 3u);
    EXPECT_EQ(held[1].hold, 1);
}

TEST(LottieExporterContractTest,
     KeyframeCompressionKeepsMismatchedShapeAndUnorderedTimeSamples)
{
    std::vector<LottieKeyframe> mismatched{
        keyframe(0.0, 0.0), keyframe(5.0, 5.0), keyframe(10.0, 10.0)};
    mismatched[1].s = {5.0, 50.0};
    LottieExporter::compressKeyframes(mismatched, 100.0);
    EXPECT_EQ(mismatched.size(), 3u);

    std::vector<LottieKeyframe> descendingTimes{
        keyframe(0.0, 0.0), keyframe(8.0, 8.0), keyframe(4.0, 4.0)};
    LottieExporter::compressKeyframes(descendingTimes, 100.0);
    EXPECT_EQ(descendingTimes.size(), 3u);

    std::vector<LottieKeyframe> nonFiniteTime{
        keyframe(0.0, 0.0),
        keyframe(std::numeric_limits<double>::quiet_NaN(), 5.0),
        keyframe(10.0, 10.0)};
    LottieExporter::compressKeyframes(nonFiniteTime, 100.0);
    EXPECT_EQ(nonFiniteTime.size(), 3u);
}

TEST(LottieExporterContractTest,
     InvalidCompressionToleranceLeavesInputExactlyUnchanged)
{
    const std::vector<double> invalidTolerances{
        -1.0, std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity()};
    for (const double tolerance : invalidTolerances) {
        SCOPED_TRACE(tolerance);
        std::vector<LottieKeyframe> keyframes{
            keyframe(0.0, 0.0), keyframe(5.0, 5.0), keyframe(10.0, 10.0)};
        const auto original = keyframes;

        LottieExporter::compressKeyframes(keyframes, tolerance);

        ASSERT_EQ(keyframes.size(), original.size());
        for (std::size_t i = 0; i < keyframes.size(); ++i) {
            EXPECT_DOUBLE_EQ(keyframes[i].t, original[i].t);
            EXPECT_EQ(keyframes[i].s, original[i].s);
            EXPECT_EQ(keyframes[i].e, original[i].e);
        }
    }
}

TEST(LottieExporterContractTest,
     ValidationReportsFailuresAndClearsErrorAfterValidDocument)
{
    QString error = QStringLiteral("stale error");
    auto document = validDocument();
    ASSERT_TRUE(LottieExporter::validate(document, &error));
    EXPECT_TRUE(error.isEmpty());

    document = validDocument();
    document.layers.front().shapes.clear();
    LottieShapePath malformedPath;
    malformedPath.vertices = {0.0, 0.0, 10.0};
    document.layers.front().shapes.emplace_back(malformedPath);
    EXPECT_FALSE(LottieExporter::validate(document, &error));
    EXPECT_FALSE(error.isEmpty());

    document = validDocument();
    document.layers.front().position.keyframes.push_back(
        keyframe(5.0, std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(LottieExporter::validate(document, &error));
    EXPECT_FALSE(error.isEmpty());

    document = validDocument();
    auto duplicate = document.layers.front();
    document.layers.push_back(duplicate);
    EXPECT_FALSE(LottieExporter::validate(document, &error));
    EXPECT_FALSE(error.isEmpty());
}

TEST(LottieExporterContractTest,
     ExportImportRoundTripPreservesDocumentAndHonorsExportOptions)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("animation.json"));
    auto document = validDocument();
    document.layers.front().position.keyframes = {
        keyframe(0.0, 0.0), keyframe(12.0, 12.0), keyframe(24.0, 24.0)};

    LottieExportOptions options;
    options.prettyPrint = true;
    options.compressKeyframes = true;
    options.keyframeTolerance = 0.0;
    options.name = QStringLiteral("  round trip  ");
    ASSERT_TRUE(LottieExporter::exportToFile(document, path, options));

    QFile output(path);
    ASSERT_TRUE(output.open(QIODevice::ReadOnly));
    const QByteArray jsonBytes = output.readAll();
    output.close();
    const auto json = QJsonDocument::fromJson(jsonBytes);
    ASSERT_TRUE(json.isObject());
    EXPECT_EQ(json.object().value(QStringLiteral("nm")).toString(),
              QStringLiteral("round trip"));
    EXPECT_NE(jsonBytes.indexOf('\n'), -1);

    QString error;
    const auto restored = LottieExporter::importFromFile(path, &error);
    ASSERT_TRUE(restored);
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(restored->nm, std::string("round trip"));
    ASSERT_EQ(restored->layers.size(), 1u);
    EXPECT_EQ(restored->layers.front().nm, std::string("shape"));
    ASSERT_EQ(restored->layers.front().shapes.size(), 1u);
    ASSERT_EQ(restored->layers.front().position.keyframes.size(), 2u);
    EXPECT_DOUBLE_EQ(restored->layers.front().position.keyframes[0].t, 0.0);
    EXPECT_DOUBLE_EQ(restored->layers.front().position.keyframes[1].t, 24.0);
}

TEST(LottieExporterContractTest,
     InvalidExportDoesNotReplaceExistingDestination)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("existing.json"));
    const QByteArray sentinel = QByteArrayLiteral("keep this file");
    QFile initial(path);
    ASSERT_TRUE(initial.open(QIODevice::WriteOnly | QIODevice::Truncate));
    ASSERT_EQ(initial.write(sentinel), sentinel.size());
    initial.close();

    auto invalidDocument = validDocument();
    invalidDocument.op = invalidDocument.ip;
    EXPECT_FALSE(LottieExporter::exportToFile(invalidDocument, path));

    QFile afterInvalidDocument(path);
    ASSERT_TRUE(afterInvalidDocument.open(QIODevice::ReadOnly));
    EXPECT_EQ(afterInvalidDocument.readAll(), sentinel);
    afterInvalidDocument.close();

    auto document = validDocument();
    LottieExportOptions invalidOptions;
    invalidOptions.keyframeTolerance = -0.01;
    EXPECT_FALSE(LottieExporter::exportToFile(document, path, invalidOptions));

    QFile afterInvalidOptions(path);
    ASSERT_TRUE(afterInvalidOptions.open(QIODevice::ReadOnly));
    EXPECT_EQ(afterInvalidOptions.readAll(), sentinel);
}

TEST(LottieExporterContractTest,
     ImportRejectsMalformedJsonAndMissingLayerArrayWithDiagnostics)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QString error;

    const auto missing = LottieExporter::importFromFile(
        directory.filePath(QStringLiteral("missing.json")), &error);
    EXPECT_FALSE(missing);
    EXPECT_FALSE(error.isEmpty());

    const QString malformedPath = directory.filePath(QStringLiteral("malformed.json"));
    QFile malformed(malformedPath);
    ASSERT_TRUE(malformed.open(QIODevice::WriteOnly));
    ASSERT_EQ(malformed.write(QByteArrayLiteral("{not json")), 9);
    malformed.close();
    error.clear();
    EXPECT_FALSE(LottieExporter::importFromFile(malformedPath, &error));
    EXPECT_FALSE(error.isEmpty());

    const QString noLayersPath = directory.filePath(QStringLiteral("no-layers.json"));
    QFile noLayers(noLayersPath);
    ASSERT_TRUE(noLayers.open(QIODevice::WriteOnly));
    ASSERT_NE(noLayers.write(QByteArrayLiteral("{}")), -1);
    noLayers.close();
    error.clear();
    EXPECT_FALSE(LottieExporter::importFromFile(noLayersPath, &error));
    EXPECT_FALSE(error.isEmpty());
}

TEST(LottieExporterContractTest,
     RigExportSamplesBoneTransformsAndPreservesParentLayerReferences)
{
    ArtifactCore::Rig2D rig;
    auto* root = rig.addBone(QStringLiteral("root"));
    auto* child = rig.addBone(QStringLiteral("child"), root);
    ASSERT_NE(root, nullptr);
    ASSERT_NE(child, nullptr);
    root->setLength(20.0f);
    root->setLocalTransform({QVector2D(2.0f, 3.0f), 15.0f,
                             QVector2D(1.5f, 0.5f)});
    child->setLength(8.0f);
    child->setLocalTransform({QVector2D(7.0f, -4.0f), -25.0f,
                              QVector2D(0.75f, 1.25f)});

    std::vector<LottieLayer> layers;
    ASSERT_TRUE(appendRigAnimation(rig, 0, 2, 24, layers));
    ASSERT_EQ(layers.size(), 2u);

    const auto& rootLayer = layers[0];
    EXPECT_EQ(rootLayer.ind, 1);
    EXPECT_EQ(rootLayer.parent, 0);
    EXPECT_EQ(rootLayer.nm, std::string("root"));
    EXPECT_EQ(rootLayer.ty, 4);
    EXPECT_EQ(rootLayer.ip, 0);
    EXPECT_EQ(rootLayer.op, 3);
    ASSERT_EQ(rootLayer.position.keyframes.size(), 3u);
    EXPECT_DOUBLE_EQ(rootLayer.position.keyframes.front().t, 0.0);
    EXPECT_DOUBLE_EQ(rootLayer.position.keyframes.front().s[0], 2.0);
    EXPECT_DOUBLE_EQ(rootLayer.position.keyframes.front().s[1], 3.0);
    EXPECT_DOUBLE_EQ(rootLayer.scale.keyframes.front().s[0], 150.0);
    EXPECT_DOUBLE_EQ(rootLayer.scale.keyframes.front().s[1], 50.0);
    EXPECT_DOUBLE_EQ(rootLayer.rotation.keyframes.front().s[0], 15.0);
    ASSERT_EQ(rootLayer.shapes.size(), 2u);
    const auto* rectangle = std::any_cast<LottieShapeRect>(&rootLayer.shapes[0]);
    ASSERT_NE(rectangle, nullptr);
    EXPECT_DOUBLE_EQ(rectangle->s.k[0], 20.0);
    EXPECT_DOUBLE_EQ(rectangle->s.k[1], 4.0);

    const auto& childLayer = layers[1];
    EXPECT_EQ(childLayer.ind, 2);
    EXPECT_EQ(childLayer.parent, rootLayer.ind);
    EXPECT_EQ(childLayer.nm, std::string("child"));
    EXPECT_DOUBLE_EQ(childLayer.position.keyframes.front().s[0], 7.0);
    EXPECT_DOUBLE_EQ(childLayer.position.keyframes.front().s[1], -4.0);
}

TEST(LottieExporterContractTest,
     RigExportRejectsInvalidRequestsWithoutMutatingCallerOutputs)
{
    ArtifactCore::Rig2D rig;
    auto* bone = rig.addBone(QStringLiteral("root"));
    ASSERT_NE(bone, nullptr);

    LottieLayer outputLayer;
    outputLayer.ind = 91;
    outputLayer.nm = "preserved";
    EXPECT_FALSE(appendRigBoneAnimation(rig, QString(), 0, 1, 24,
                                        outputLayer));
    EXPECT_EQ(outputLayer.ind, 91);
    EXPECT_EQ(outputLayer.nm, std::string("preserved"));

    std::vector<LottieLayer> outputLayers(1);
    outputLayers.front().ind = 73;
    EXPECT_FALSE(appendRigAnimation(rig, 5, 4, 24, outputLayers));
    ASSERT_EQ(outputLayers.size(), 1u);
    EXPECT_EQ(outputLayers.front().ind, 73);

    bone->setLocalTransform({
        QVector2D(std::numeric_limits<float>::quiet_NaN(), 0.0f), 0.0f,
        QVector2D(1.0f, 1.0f)});
    EXPECT_FALSE(appendRigAnimation(rig, 0, 1, 24, outputLayers));
    ASSERT_EQ(outputLayers.size(), 1u);
    EXPECT_EQ(outputLayers.front().ind, 73);

    LottieDocument document = validDocument();
    const auto original = LottieExporter::toJson(document);
    EXPECT_FALSE(appendRigToDocument(rig, 0, 720, 24.0, 0, 1,
                                     QStringLiteral("invalid"), document));
    EXPECT_EQ(LottieExporter::toJson(document), original);
}

TEST(LottieExporterContractTest,
     RigExportUsesExclusiveOutFrameAndCompressesStaticSamplesInDocument)
{
    ArtifactCore::Rig2D rig;
    auto* bone = rig.addBone(QStringLiteral("static"));
    ASSERT_NE(bone, nullptr);
    bone->setLocalTransform({QVector2D(4.0f, 9.0f), 30.0f,
                             QVector2D(1.0f, 1.0f)});

    LottieDocument document;
    ASSERT_TRUE(appendRigToDocument(rig, 1920, 1080, 23.976, 3, 5,
                                   QStringLiteral("Rig"), document));

    EXPECT_DOUBLE_EQ(document.fr, 23.976);
    EXPECT_EQ(document.ip, 3);
    EXPECT_EQ(document.op, 6);
    EXPECT_EQ(document.w, 1920);
    EXPECT_EQ(document.h, 1080);
    EXPECT_EQ(document.nm, std::string("Rig"));
    ASSERT_EQ(document.layers.size(), 1u);
    EXPECT_EQ(document.layers.front().ip, 3);
    EXPECT_EQ(document.layers.front().op, 6);
    ASSERT_EQ(document.layers.front().position.keyframes.size(), 2u);
    EXPECT_DOUBLE_EQ(document.layers.front().position.keyframes.front().t, 3.0);
    EXPECT_DOUBLE_EQ(document.layers.front().position.keyframes.back().t, 5.0);
}
