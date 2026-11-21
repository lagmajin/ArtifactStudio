#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>

import Procedural3DGenerators;
import Font.FreeFont;

using namespace ArtifactCore;

namespace {

void expectValidLodMesh(const Procedural3DResult& result)
{
    EXPECT_EQ(result.mesh.indices.size() % 3u, 0u);
    for (const std::uint32_t index : result.mesh.indices) {
        EXPECT_LT(index, result.mesh.vertices.size());
    }
    for (const Procedural3DVertex& vertex : result.mesh.vertices) {
        EXPECT_TRUE(std::isfinite(vertex.px));
        EXPECT_TRUE(std::isfinite(vertex.py));
        EXPECT_TRUE(std::isfinite(vertex.pz));
        EXPECT_TRUE(std::isfinite(vertex.nx));
        EXPECT_TRUE(std::isfinite(vertex.ny));
        EXPECT_TRUE(std::isfinite(vertex.nz));
        const float normalLength = std::sqrt(
            vertex.nx * vertex.nx + vertex.ny * vertex.ny + vertex.nz * vertex.nz);
        EXPECT_NEAR(normalLength, 1.0f, 1.0e-3f);
        EXPECT_TRUE(std::isfinite(vertex.u));
        EXPECT_TRUE(std::isfinite(vertex.v));
    }
}

void expectSameMesh(const Procedural3DMeshData& lhs,
                    const Procedural3DMeshData& rhs)
{
    ASSERT_EQ(lhs.vertices.size(), rhs.vertices.size());
    ASSERT_EQ(lhs.indices.size(), rhs.indices.size());
    for (std::size_t index = 0; index < lhs.vertices.size(); ++index) {
        const Procedural3DVertex& a = lhs.vertices[index];
        const Procedural3DVertex& b = rhs.vertices[index];
        EXPECT_FLOAT_EQ(a.px, b.px);
        EXPECT_FLOAT_EQ(a.py, b.py);
        EXPECT_FLOAT_EQ(a.pz, b.pz);
        EXPECT_FLOAT_EQ(a.nx, b.nx);
        EXPECT_FLOAT_EQ(a.ny, b.ny);
        EXPECT_FLOAT_EQ(a.nz, b.nz);
        EXPECT_FLOAT_EQ(a.u, b.u);
        EXPECT_FLOAT_EQ(a.v, b.v);
    }
    EXPECT_EQ(lhs.indices, rhs.indices);
}

void expectSameBounds(const Procedural3DResult& lhs,
                      const Procedural3DResult& rhs,
                      float tolerance = 1.0e-5f)
{
    const Procedural3DBounds a = Procedural3DGenerators::previewInfo(lhs).bounds;
    const Procedural3DBounds b = Procedural3DGenerators::previewInfo(rhs).bounds;
    EXPECT_NEAR(a.minX, b.minX, tolerance);
    EXPECT_NEAR(a.minY, b.minY, tolerance);
    EXPECT_NEAR(a.minZ, b.minZ, tolerance);
    EXPECT_NEAR(a.maxX, b.maxX, tolerance);
    EXPECT_NEAR(a.maxY, b.maxY, tolerance);
    EXPECT_NEAR(a.maxZ, b.maxZ, tolerance);
}

} // namespace

TEST(Procedural3DQualityLodTest, TerrainResolutionGrowsAcrossQualityLevels)
{
    const auto draftSettings = Procedural3DGenerators::makeTerrainPreset(
        17u, Procedural3DQuality::Draft);
    const auto previewSettings = Procedural3DGenerators::makeTerrainPreset(
        17u, Procedural3DQuality::Preview);
    const auto fullSettings = Procedural3DGenerators::makeTerrainPreset(
        17u, Procedural3DQuality::Full);

    const auto draft = Procedural3DGenerators::generateTerrainResult(draftSettings);
    const auto preview = Procedural3DGenerators::generateTerrainResult(previewSettings);
    const auto full = Procedural3DGenerators::generateTerrainResult(fullSettings);

    ASSERT_TRUE(draft.valid);
    ASSERT_TRUE(preview.valid);
    ASSERT_TRUE(full.valid);
    EXPECT_EQ(draft.quality, Procedural3DQuality::Draft);
    EXPECT_EQ(preview.quality, Procedural3DQuality::Preview);
    EXPECT_EQ(full.quality, Procedural3DQuality::Full);
    EXPECT_LT(draft.mesh.vertices.size(), preview.mesh.vertices.size());
    EXPECT_LT(preview.mesh.vertices.size(), full.mesh.vertices.size());
    EXPECT_LT(draft.mesh.indices.size(), preview.mesh.indices.size());
    EXPECT_LT(preview.mesh.indices.size(), full.mesh.indices.size());

    const Procedural3DPreviewInfo draftInfo =
        Procedural3DGenerators::previewInfo(draft);
    const Procedural3DPreviewInfo fullInfo =
        Procedural3DGenerators::previewInfo(full);
    EXPECT_EQ(draftInfo.vertexCount, draft.mesh.vertices.size());
    EXPECT_EQ(fullInfo.vertexCount, full.mesh.vertices.size());
    EXPECT_EQ(draftInfo.quality, Procedural3DQuality::Draft);
    EXPECT_EQ(fullInfo.quality, Procedural3DQuality::Full);
    EXPECT_FLOAT_EQ(draftInfo.bounds.minX, fullInfo.bounds.minX);
    EXPECT_FLOAT_EQ(draftInfo.bounds.maxX, fullInfo.bounds.maxX);
    EXPECT_FLOAT_EQ(draftInfo.bounds.minY, fullInfo.bounds.minY);
    EXPECT_FLOAT_EQ(draftInfo.bounds.maxY, fullInfo.bounds.maxY);
    EXPECT_TRUE(std::isfinite(draftInfo.bounds.minZ));
    EXPECT_TRUE(std::isfinite(fullInfo.bounds.maxZ));
    for (const auto* result : {&draft, &preview, &full}) {
        expectValidLodMesh(*result);
    }

    const auto repeatedFull = Procedural3DGenerators::generateTerrainResult(
        fullSettings);
    expectSameMesh(full.mesh, repeatedFull.mesh);
    expectSameBounds(draft, full);
}

TEST(Procedural3DQualityLodTest, PathTubeResolutionGrowsAcrossQualityLevels)
{
    const auto draftSettings = Procedural3DGenerators::makePathTubePreset(
        29u, Procedural3DQuality::Draft);
    const auto previewSettings = Procedural3DGenerators::makePathTubePreset(
        29u, Procedural3DQuality::Preview);
    const auto fullSettings = Procedural3DGenerators::makePathTubePreset(
        29u, Procedural3DQuality::Full);

    const auto draft = Procedural3DGenerators::generatePathTubeResult(draftSettings);
    const auto preview = Procedural3DGenerators::generatePathTubeResult(previewSettings);
    const auto full = Procedural3DGenerators::generatePathTubeResult(fullSettings);

    ASSERT_TRUE(draft.valid);
    ASSERT_TRUE(preview.valid);
    ASSERT_TRUE(full.valid);
    EXPECT_EQ(draft.quality, Procedural3DQuality::Draft);
    EXPECT_EQ(preview.quality, Procedural3DQuality::Preview);
    EXPECT_EQ(full.quality, Procedural3DQuality::Full);
    EXPECT_LT(draft.mesh.vertices.size(), preview.mesh.vertices.size());
    EXPECT_LT(preview.mesh.vertices.size(), full.mesh.vertices.size());
    EXPECT_LT(draft.mesh.indices.size(), preview.mesh.indices.size());
    EXPECT_LT(preview.mesh.indices.size(), full.mesh.indices.size());

    for (const auto* result : {&draft, &preview, &full}) {
        expectValidLodMesh(*result);
    }

    const auto repeatedPreview = Procedural3DGenerators::generatePathTubeResult(
        previewSettings);
    expectSameMesh(preview.mesh, repeatedPreview.mesh);
    auto sharedPathSettings = previewSettings;
    sharedPathSettings.quality = Procedural3DQuality::Draft;
    const auto sameProfileDraft = Procedural3DGenerators::generatePathTubeResult(
        sharedPathSettings);
    sharedPathSettings.quality = Procedural3DQuality::Full;
    const auto sameProfileFull = Procedural3DGenerators::generatePathTubeResult(
        sharedPathSettings);
    expectSameBounds(sameProfileDraft, sameProfileFull, 0.05f);
}

TEST(Procedural3DQualityLodTest, TextExtrusionGeometryGrowsAcrossQualityLevels)
{
    int argc = 1;
    char applicationName[] = "Procedural3DQualityLodTest";
    char* argv[] = {applicationName, nullptr};
    QGuiApplication application(argc, argv);

    const auto draftSettings = Procedural3DGenerators::makeTextExtrudePreset(
        Procedural3DQuality::Draft);
    const auto previewSettings = Procedural3DGenerators::makeTextExtrudePreset(
        Procedural3DQuality::Preview);
    const auto fullSettings = Procedural3DGenerators::makeTextExtrudePreset(
        Procedural3DQuality::Full);

    const auto draft = Procedural3DGenerators::generateTextExtrudeResult(draftSettings);
    const auto preview = Procedural3DGenerators::generateTextExtrudeResult(previewSettings);
    const auto full = Procedural3DGenerators::generateTextExtrudeResult(fullSettings);

    ASSERT_TRUE(draft.valid);
    ASSERT_TRUE(preview.valid);
    ASSERT_TRUE(full.valid);
    EXPECT_EQ(draft.quality, Procedural3DQuality::Draft);
    EXPECT_EQ(preview.quality, Procedural3DQuality::Preview);
    EXPECT_EQ(full.quality, Procedural3DQuality::Full);
    EXPECT_LT(draft.mesh.vertices.size(), preview.mesh.vertices.size());
    EXPECT_LT(preview.mesh.vertices.size(), full.mesh.vertices.size());
    EXPECT_LT(draft.mesh.indices.size(), preview.mesh.indices.size());
    EXPECT_LT(preview.mesh.indices.size(), full.mesh.indices.size());

    for (const auto* result : {&draft, &preview, &full}) {
        expectValidLodMesh(*result);
    }

    const auto repeatedDraft = Procedural3DGenerators::generateTextExtrudeResult(
        draftSettings);
    expectSameMesh(draft.mesh, repeatedDraft.mesh);
    auto sharedTextSettings = previewSettings;
    sharedTextSettings.quality = Procedural3DQuality::Draft;
    const auto sameTextDraft = Procedural3DGenerators::generateTextExtrudeResult(
        sharedTextSettings);
    sharedTextSettings.quality = Procedural3DQuality::Full;
    const auto sameTextFull = Procedural3DGenerators::generateTextExtrudeResult(
        sharedTextSettings);
    expectSameBounds(sameTextDraft, sameTextFull, 1.0e-4f);
}

TEST(Procedural3DQualityLodTest, TextExtrusionResultReportsFontAndInputDiagnostics)
{
    int argc = 1;
    char applicationName[] = "Procedural3DTextExtrusionDiagnosticsTest";
    char* argv[] = {applicationName, nullptr};
    QGuiApplication application(argc, argv);

    const QDir sourceDirectory(
        QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
    const QString bundledFontPath = sourceDirectory.filePath(QStringLiteral(
        "../../libs/openusd/pxr/usdImaging/usdviewq/fonts/Roboto/Roboto-Regular.ttf"));
    ASSERT_TRUE(QFileInfo::exists(bundledFontPath));
    ASSERT_TRUE(FontManager::loadFontFromFile(bundledFontPath));

    TextExtrudeSettings settings;
    settings.text = QStringLiteral("A");
    settings.fontFamily = QStringLiteral("Roboto");
    settings.fontSize = 64;
    settings.depth = 12.0f;
    settings.bevelWidth = 0.0f;
    const Procedural3DResult success =
        Procedural3DGenerators::generateTextExtrudeResult(settings);
    ASSERT_TRUE(success.valid);
    EXPECT_EQ(success.status, Procedural3DGenerationStatus::Success);
    EXPECT_FALSE(success.resolvedFontFamily.trimmed().isEmpty());
    EXPECT_FALSE(success.resolvedFontFamilies.isEmpty());
    EXPECT_EQ(success.missingGlyphCount, 0u);
    ASSERT_TRUE(success.fontIdentityComplete) << success.diagnosticMessage.toStdString();
    ASSERT_FALSE(success.resolvedFontFingerprints.isEmpty());

    const uint unavailableCodePoint = 0x10FFFFu;
    settings.text = QString::fromUcs4(&unavailableCodePoint, 1);
    const Procedural3DResult missingGlyph =
        Procedural3DGenerators::generateTextExtrudeResult(settings);
    EXPECT_GT(missingGlyph.missingGlyphCount, 0u);
    if (missingGlyph.missingGlyphCount > 0u) {
        EXPECT_TRUE(missingGlyph.missingGlyphLocationsComplete);
        EXPECT_FALSE(missingGlyph.missingGlyphLocations.isEmpty());
        EXPECT_NE(missingGlyph.missingGlyphLocations.front().indexOf(
                      QStringLiteral("U+10FFFF")), -1);
    }

    settings.text = QStringLiteral("   ");
    const Procedural3DResult empty =
        Procedural3DGenerators::generateTextExtrudeResult(settings);
    EXPECT_FALSE(empty.valid);
    EXPECT_EQ(empty.status, Procedural3DGenerationStatus::EmptyInput);
    EXPECT_FALSE(empty.diagnosticMessage.isEmpty());

    settings.text = QStringLiteral("A");
    settings.depth = std::numeric_limits<float>::quiet_NaN();
    const Procedural3DResult invalid =
        Procedural3DGenerators::generateTextExtrudeResult(settings);
    EXPECT_FALSE(invalid.valid);
    EXPECT_EQ(invalid.status, Procedural3DGenerationStatus::InvalidSettings);

    settings.depth = 12.0f;
    settings.text = QStringLiteral("AAAA");
    settings.maxGlyphCount = 2;
    const Procedural3DResult limited =
        Procedural3DGenerators::generateTextExtrudeResult(settings);
    EXPECT_FALSE(limited.valid);
    EXPECT_EQ(limited.status, Procedural3DGenerationStatus::ResourceLimitExceeded);

    settings.maxGlyphCount = 4096;
    const auto expectLineBudgetRejection = [&](const QString& text) {
        settings.text = text;
        const Procedural3DResult result =
            Procedural3DGenerators::generateTextExtrudeResult(settings);
        EXPECT_FALSE(result.valid);
        EXPECT_EQ(result.status,
                  Procedural3DGenerationStatus::ResourceLimitExceeded);
        EXPECT_NE(result.diagnosticMessage.indexOf(
                      QStringLiteral("line count")), -1);
    };
    const auto repeatedLines = [](const QString& separator) {
        QString text;
        text.reserve(12288);
        for (int line = 0; line < 4096; ++line) {
            text += QLatin1Char('A');
            text += separator;
        }
        return text;
    };
    expectLineBudgetRejection(repeatedLines(QStringLiteral("\n")));
    expectLineBudgetRejection(repeatedLines(QStringLiteral("\r\n")));
    expectLineBudgetRejection(repeatedLines(
        QString(QChar(QChar::LineSeparator))));
}

TEST(Procedural3DQualityLodTest, TextExtrusionSupportsTrackingLeadingAndLineAlignment)
{
    int argc = 1;
    char applicationName[] = "Procedural3DTextLayoutControlsTest";
    char* argv[] = {applicationName, nullptr};
    QGuiApplication application(argc, argv);

    const QDir sourceDirectory(
        QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
    const QString bundledFontPath = sourceDirectory.filePath(QStringLiteral(
        "../../libs/openusd/pxr/usdImaging/usdviewq/fonts/Roboto/Roboto-Regular.ttf"));
    ASSERT_TRUE(QFileInfo::exists(bundledFontPath));
    ASSERT_TRUE(FontManager::loadFontFromFile(bundledFontPath));

    TextExtrudeSettings settings;
    settings.text = QStringLiteral("I\nIIII");
    settings.fontFamily = QStringLiteral("Roboto");
    settings.fontSize = 96;
    settings.depth = 20.0f;
    settings.bevelWidth = 0.0f;

    const auto base = Procedural3DGenerators::generateTextExtrudeResult(settings);
    ASSERT_TRUE(base.valid);
    const auto bounds = [](const Procedural3DMeshData& mesh) {
        std::array<float, 4> result{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()};
        for (const Procedural3DVertex& vertex : mesh.vertices) {
            result[0] = std::min(result[0], vertex.px);
            result[1] = std::min(result[1], vertex.py);
            result[2] = std::max(result[2], vertex.px);
            result[3] = std::max(result[3], vertex.py);
        }
        return result;
    };

    settings.letterSpacing = 18.0f;
    const auto tracked = Procedural3DGenerators::generateTextExtrudeResult(settings);
    ASSERT_TRUE(tracked.valid);
    EXPECT_GT(bounds(tracked.mesh)[2] - bounds(tracked.mesh)[0],
              bounds(base.mesh)[2] - bounds(base.mesh)[0]);

    settings.letterSpacing = 0.0f;
    settings.lineSpacing = 2.0f;
    const auto looseLeading =
        Procedural3DGenerators::generateTextExtrudeResult(settings);
    ASSERT_TRUE(looseLeading.valid);
    EXPECT_GT(bounds(looseLeading.mesh)[3] - bounds(looseLeading.mesh)[1],
              bounds(base.mesh)[3] - bounds(base.mesh)[1]);

    const auto lineExtents = [&](TextExtrudeAlignment alignment) {
        settings.alignment = alignment;
        const auto result = Procedural3DGenerators::generateTextExtrudeResult(settings);
        EXPECT_TRUE(result.valid);
        std::array<float, 4> extents{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()};
        for (const Procedural3DVertex& vertex : result.mesh.vertices) {
            if (vertex.pz < settings.depth * 0.5f - 1.0e-4f) continue;
            const std::size_t line = vertex.py >= 0.0f ? 0u : 1u;
            extents[line * 2u] = std::min(extents[line * 2u], vertex.px);
            extents[line * 2u + 1u] = std::max(extents[line * 2u + 1u], vertex.px);
        }
        return extents;
    };
    settings.lineSpacing = 1.0f;
    const auto left = lineExtents(TextExtrudeAlignment::Left);
    const auto center = lineExtents(TextExtrudeAlignment::Center);
    const auto right = lineExtents(TextExtrudeAlignment::Right);
    EXPECT_NEAR(left[0], left[2], 1.0e-3f);
    EXPECT_NEAR((center[0] + center[1]) * 0.5f,
                (center[2] + center[3]) * 0.5f, 1.0e-3f);
    EXPECT_NEAR(right[1], right[3], 1.0e-3f);
}

TEST(Procedural3DQualityLodTest,
     UnknownQualityUsesPreviewDensityAcrossEveryProceduralGenerator)
{
    int argc = 1;
    char applicationName[] = "Procedural3DUnknownQualityLodTest";
    char* argv[] = {applicationName, nullptr};
    QGuiApplication application(argc, argv);

    constexpr auto unknownQuality = static_cast<Procedural3DQuality>(255);

    auto previewTerrainSettings = Procedural3DGenerators::makeTerrainPreset(
        53u, Procedural3DQuality::Preview);
    auto unknownTerrainSettings = previewTerrainSettings;
    unknownTerrainSettings.quality = unknownQuality;
    const auto previewTerrain = Procedural3DGenerators::generateTerrainResult(
        previewTerrainSettings);
    const auto unknownTerrain = Procedural3DGenerators::generateTerrainResult(
        unknownTerrainSettings);
    ASSERT_TRUE(previewTerrain.valid);
    ASSERT_TRUE(unknownTerrain.valid);
    EXPECT_EQ(unknownTerrain.quality, unknownQuality);
    expectSameMesh(previewTerrain.mesh, unknownTerrain.mesh);

    auto previewPathSettings = Procedural3DGenerators::makePathTubePreset(
        71u, Procedural3DQuality::Preview);
    auto unknownPathSettings = previewPathSettings;
    unknownPathSettings.quality = unknownQuality;
    const auto previewPath = Procedural3DGenerators::generatePathTubeResult(
        previewPathSettings);
    const auto unknownPath = Procedural3DGenerators::generatePathTubeResult(
        unknownPathSettings);
    ASSERT_TRUE(previewPath.valid);
    ASSERT_TRUE(unknownPath.valid);
    EXPECT_EQ(unknownPath.quality, unknownQuality);
    expectSameMesh(previewPath.mesh, unknownPath.mesh);

    auto previewTextSettings = Procedural3DGenerators::makeTextExtrudePreset(
        Procedural3DQuality::Preview);
    auto unknownTextSettings = previewTextSettings;
    unknownTextSettings.quality = unknownQuality;
    const auto previewText = Procedural3DGenerators::generateTextExtrudeResult(
        previewTextSettings);
    const auto unknownText = Procedural3DGenerators::generateTextExtrudeResult(
        unknownTextSettings);
    ASSERT_TRUE(previewText.valid);
    ASSERT_TRUE(unknownText.valid);
    EXPECT_EQ(unknownText.quality, unknownQuality);
    expectSameMesh(previewText.mesh, unknownText.mesh);
}

TEST(Procedural3DQualityLodTest,
     AnimatedTerrainAndPathKeepLodTopologyAndRestoreAtEarlierTime)
{
    const auto verifyAnimatedLod = [](const Procedural3DResult& start,
                                      const Procedural3DResult& later,
                                      const Procedural3DResult& restored) {
        ASSERT_TRUE(start.valid);
        ASSERT_TRUE(later.valid);
        ASSERT_TRUE(restored.valid);
        ASSERT_EQ(start.quality, later.quality);
        ASSERT_EQ(start.mesh.indices, later.mesh.indices)
            << "time changes should deform generated vertices without changing "
               "the LOD index topology";
        ASSERT_EQ(start.mesh.vertices.size(), later.mesh.vertices.size());

        bool positionsChanged = false;
        for (std::size_t index = 0; index < start.mesh.vertices.size(); ++index) {
            const Procedural3DVertex& first = start.mesh.vertices[index];
            const Procedural3DVertex& second = later.mesh.vertices[index];
            positionsChanged = positionsChanged || first.px != second.px ||
                first.py != second.py || first.pz != second.pz;
        }
        EXPECT_TRUE(positionsChanged)
            << "the animated generator should produce a different pose at a "
               "later sample";
        expectSameMesh(start.mesh, restored.mesh);
    };

    for (const Procedural3DQuality quality : {Procedural3DQuality::Draft,
                                               Procedural3DQuality::Preview,
                                               Procedural3DQuality::Full}) {
        const auto terrainSettings = Procedural3DGenerators::makeTerrainPreset(
            83u, quality);
        verifyAnimatedLod(
            Procedural3DGenerators::generateTerrainResult(terrainSettings, 0.0f),
            Procedural3DGenerators::generateTerrainResult(terrainSettings, 2.0f),
            Procedural3DGenerators::generateTerrainResult(terrainSettings, 0.0f));

        const auto pathSettings = Procedural3DGenerators::makePathTubePreset(
            97u, quality);
        verifyAnimatedLod(
            Procedural3DGenerators::generatePathTubeResult(pathSettings, 0.0f),
            Procedural3DGenerators::generatePathTubeResult(pathSettings, 2.0f),
            Procedural3DGenerators::generatePathTubeResult(pathSettings, 0.0f));
    }
}

TEST(Procedural3DQualityLodTest,
     DowngradingAndRestoringQualityReproducesFullGeometry)
{
    int argc = 1;
    char applicationName[] = "Procedural3DQualityRoundTripLodTest";
    char* argv[] = {applicationName, nullptr};
    QGuiApplication application(argc, argv);

    const auto verifyQualityRoundTrip = [](auto fullSettings, auto generate) {
        fullSettings.quality = Procedural3DQuality::Full;
        const auto fullBefore = generate(fullSettings);
        ASSERT_TRUE(fullBefore.valid);
        ASSERT_EQ(fullBefore.quality, Procedural3DQuality::Full);

        for (const Procedural3DQuality reducedQuality : {
                 Procedural3DQuality::Draft, Procedural3DQuality::Preview}) {
            auto reducedSettings = fullSettings;
            reducedSettings.quality = reducedQuality;
            const auto reduced = generate(reducedSettings);
            ASSERT_TRUE(reduced.valid);
            ASSERT_EQ(reduced.quality, reducedQuality);
            EXPECT_LT(reduced.mesh.vertices.size(),
                      fullBefore.mesh.vertices.size());
            EXPECT_LT(reduced.mesh.indices.size(), fullBefore.mesh.indices.size());
        }

        const auto fullAfter = generate(fullSettings);
        ASSERT_TRUE(fullAfter.valid);
        expectSameMesh(fullBefore.mesh, fullAfter.mesh);
        expectSameBounds(fullBefore, fullAfter);
    };

    verifyQualityRoundTrip(
        Procedural3DGenerators::makeTerrainPreset(
            211u, Procedural3DQuality::Full),
        [](const TerrainSettings& settings) {
            return Procedural3DGenerators::generateTerrainResult(settings, 1.25f);
        });
    verifyQualityRoundTrip(
        Procedural3DGenerators::makePathTubePreset(
            223u, Procedural3DQuality::Full),
        [](const PathTubeSettings& settings) {
            return Procedural3DGenerators::generatePathTubeResult(settings, 1.25f);
        });
    verifyQualityRoundTrip(
        Procedural3DGenerators::makeTextExtrudePreset(Procedural3DQuality::Full),
        [](const TextExtrudeSettings& settings) {
            return Procedural3DGenerators::generateTextExtrudeResult(settings);
        });
}
