#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>
#include <QTemporaryDir>

import Render.Farm.Master;
import Render.Farm.Types;

using namespace ArtifactCore;

TEST(RenderFarmMasterTemplateContractTest,
     TemplateRoundTripPreservesSchedulingAndRendererPayloadAndSkipsInvalidEntries)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString templateFile = directory.filePath(QStringLiteral("templates.json"));

    RenderFarmMaster source(1);
    RenderJobRequest request;
    request.jobId = QStringLiteral("template-render-job");
    request.compositionName = QStringLiteral("Sequence Master");
    request.range = {101, 125, 4};
    request.outputPath = QStringLiteral("renders/shot_%04d.exr");
    request.autoVersionOutput = true;
    request.enableAudio = true;
    request.priority = 73;
    request.dependencies = {QStringLiteral("plates-ready"), QStringLiteral("color-ready")};
    request.jobPool = QStringLiteral("nightly-gpu");
    request.allowedWorkerIds = {QStringLiteral("worker-a"), QStringLiteral("worker-c")};
    request.jobTimeoutMs = 90000;
    request.frameTimeoutMs = 8500;
    request.requiredCapabilities = QJsonObject{
        {QStringLiteral("gpu"), QJsonObject{
             {QStringLiteral("vendor"), QStringLiteral("VendorX")},
             {QStringLiteral("memoryMiB"), 16384}}},
        {QStringLiteral("codecs"), QJsonArray{QStringLiteral("exr"), QStringLiteral("prores")}},
    };
    request.renderPayload = QJsonObject{
        {QStringLiteral("composition"), QJsonObject{
             {QStringLiteral("width"), 4096},
             {QStringLiteral("height"), 2160},
             {QStringLiteral("colorSpace"), QStringLiteral("ACEScg")}}},
        {QStringLiteral("effects"), QJsonArray{
             QJsonObject{{QStringLiteral("type"), QStringLiteral("denoise")},
                         {QStringLiteral("quality"), QStringLiteral("high")}},
             QJsonObject{{QStringLiteral("type"), QStringLiteral("grain")},
                         {QStringLiteral("seed"), 314159}}}},
    };
    request.rendererExecutable = QStringLiteral("artifact-render-worker");

    EXPECT_FALSE(source.registerJobTemplate({QStringLiteral("  "), request}));
    RenderJobRequest invalidRange = request;
    invalidRange.range = {125, 101, 1};
    EXPECT_FALSE(source.registerJobTemplate({QStringLiteral("invalid-range"), invalidRange}));
    ASSERT_TRUE(source.registerJobTemplate({QStringLiteral("  final-output  "), request}));
    EXPECT_EQ(source.jobTemplateNames(), QStringList{QStringLiteral("final-output")});
    ASSERT_TRUE(source.saveJobTemplates(templateFile));

    // Keep one valid template and inject records that the loader must reject.
    QFile file(templateFile);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError);
    ASSERT_TRUE(document.isObject());
    file.close();
    QJsonObject root = document.object();
    QJsonArray entries = root.value(QStringLiteral("templates")).toArray();
    entries.append(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("missing-renderer")},
        {QStringLiteral("startFrame"), 0},
        {QStringLiteral("endFrame"), 10},
        {QStringLiteral("renderPayload"), QJsonObject{{QStringLiteral("scene"), 1}}},
    });
    entries.append(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("empty-range")},
        {QStringLiteral("startFrame"), 10},
        {QStringLiteral("endFrame"), 10},
        {QStringLiteral("rendererExecutable"), QStringLiteral("worker")},
        {QStringLiteral("renderPayload"), QJsonObject{{QStringLiteral("scene"), 1}}},
    });
    root[QStringLiteral("templates")] = entries;
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray malformedDocument = QJsonDocument(root).toJson(QJsonDocument::Compact);
    ASSERT_EQ(file.write(malformedDocument), malformedDocument.size());
    ASSERT_TRUE(file.flush());
    file.close();

    RenderFarmMaster restored(1);
    ASSERT_TRUE(restored.loadJobTemplates(templateFile));
    EXPECT_EQ(restored.jobTemplateNames(), QStringList{QStringLiteral("final-output")});
    const auto loadedTemplate = restored.jobTemplate(QStringLiteral(" final-output "));
    ASSERT_TRUE(loadedTemplate.has_value());
    const RenderJobRequest& loaded = *loadedTemplate;
    EXPECT_EQ(loaded.jobId, request.jobId);
    EXPECT_EQ(loaded.compositionName, request.compositionName);
    EXPECT_EQ(loaded.range.startFrame, request.range.startFrame);
    EXPECT_EQ(loaded.range.endFrame, request.range.endFrame);
    EXPECT_EQ(loaded.range.step, request.range.step);
    EXPECT_EQ(loaded.range.count(), request.range.count());
    EXPECT_EQ(loaded.outputPath, request.outputPath);
    EXPECT_EQ(loaded.autoVersionOutput, request.autoVersionOutput);
    EXPECT_EQ(loaded.enableAudio, request.enableAudio);
    EXPECT_EQ(loaded.priority, request.priority);
    EXPECT_EQ(loaded.dependencies, request.dependencies);
    EXPECT_EQ(loaded.jobPool, request.jobPool);
    EXPECT_EQ(loaded.allowedWorkerIds, request.allowedWorkerIds);
    EXPECT_EQ(loaded.jobTimeoutMs, request.jobTimeoutMs);
    EXPECT_EQ(loaded.frameTimeoutMs, request.frameTimeoutMs);
    EXPECT_EQ(loaded.requiredCapabilities, request.requiredCapabilities);
    EXPECT_EQ(loaded.renderPayload, request.renderPayload);
    EXPECT_EQ(loaded.rendererExecutable, request.rendererExecutable);
    EXPECT_FALSE(static_cast<bool>(loaded.renderFrame));
}
