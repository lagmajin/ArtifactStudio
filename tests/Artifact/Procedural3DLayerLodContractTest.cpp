#include <gtest/gtest.h>

#include <array>
#include <utility>

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <QRectF>
#include <QString>
#include <QVariant>

import Artifact.Layer.Abstract;
import Artifact.Layer.Procedural3D;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.IRenderer;
import Font.FreeFont;

namespace {

class DefaultLodLayer final : public Artifact::ArtifactAbstractLayer {
public:
    void draw(Artifact::ArtifactIRenderer*) override { ++drawCalls; }

    int drawCalls = 0;
};

class DetailAwareLayer final : public Artifact::ArtifactAbstractLayer {
public:
    void draw(Artifact::ArtifactIRenderer*) override { ++drawCalls; }
    void drawLOD(Artifact::ArtifactIRenderer*, Artifact::DetailLevel detail) override
    {
        ++lodDrawCalls;
        lastDetail = detail;
    }
    QRectF localBounds() const override { return QRectF(0.0, 0.0, 10.0, 10.0); }

    int drawCalls = 0;
    int lodDrawCalls = 0;
    Artifact::DetailLevel lastDetail = Artifact::DetailLevel::High;
};

QString renderStateForLod(Artifact::Procedural3DLayerKind kind,
                          Artifact::DetailLevel lod)
{
    Artifact::ArtifactProcedural3DLayer layer(kind);
    Artifact::ArtifactIRenderer renderer;
    layer.setVisible(true);
    layer.drawLOD(&renderer, lod);
    return layer.debugState();
}

void expectQuality(const QString& state, int quality)
{
    EXPECT_NE(state.indexOf(QStringLiteral("quality=%1").arg(quality)), -1)
        << state.toStdString();
}

int vertexCountFromState(const QString& state)
{
    const qsizetype start = state.indexOf(QStringLiteral("vertices="));
    const qsizetype end = state.indexOf(QLatin1Char(' '), start);
    if (start < 0 || end < 0) {
        ADD_FAILURE() << "Missing vertex count in " << state.toStdString();
        return -1;
    }
    bool parsed = false;
    const int count = state.mid(start + 9, end - start - 9).toInt(&parsed);
    EXPECT_TRUE(parsed) << state.toStdString();
    return parsed ? count : -1;
}

} // namespace

TEST(Procedural3DLayerLodContractTest,
     BaseLayerDrawLodDelegatesEveryDetailLevelToRegularDraw)
{
    DefaultLodLayer layer;
    const std::array<Artifact::DetailLevel, 4> levels{
        Artifact::DetailLevel::Low,
        Artifact::DetailLevel::Medium,
        Artifact::DetailLevel::High,
        static_cast<Artifact::DetailLevel>(99)};

    for (const Artifact::DetailLevel level : levels) {
        layer.drawLOD(nullptr, level);
    }

    EXPECT_EQ(layer.drawCalls, static_cast<int>(levels.size()));
}

TEST(Procedural3DLayerLodContractTest,
     CompositionViewDispatchesRequestedDetailToLayerDrawLOD)
{
    DetailAwareLayer layer;
    Artifact::ArtifactIRenderer renderer;
    layer.setVisible(true);

    for (const Artifact::DetailLevel detail : {
             Artifact::DetailLevel::Low,
             Artifact::DetailLevel::Medium,
             Artifact::DetailLevel::High}) {
        Artifact::drawLayerForCompositionView(
            &layer, &renderer, 1.0f, nullptr, nullptr, nullptr, 0, false,
            detail);
        EXPECT_EQ(layer.lastDetail, detail);
    }

    EXPECT_EQ(layer.drawCalls, 0)
        << "the composition view should dispatch detail-aware layers through drawLOD";
    EXPECT_EQ(layer.lodDrawCalls, 3);
}

TEST(Procedural3DLayerLodContractTest,
     CompositionViewDefaultsOmittedDetailToHigh)
{
    DetailAwareLayer layer;
    Artifact::ArtifactIRenderer renderer;
    layer.setVisible(true);

    Artifact::drawLayerForCompositionView(&layer, &renderer);

    EXPECT_EQ(layer.drawCalls, 0);
    ASSERT_EQ(layer.lodDrawCalls, 1);
    EXPECT_EQ(layer.lastDetail, Artifact::DetailLevel::High);
}

TEST(Procedural3DLayerLodContractTest,
     CompositionViewPreservesUnknownDetailForLayerFallback)
{
    DetailAwareLayer layer;
    Artifact::ArtifactIRenderer renderer;
    layer.setVisible(true);
    constexpr auto unknownDetail = static_cast<Artifact::DetailLevel>(99);

    Artifact::drawLayerForCompositionView(
        &layer, &renderer, 1.0f, nullptr, nullptr, nullptr, 0, false,
        unknownDetail);

    EXPECT_EQ(layer.drawCalls, 0);
    ASSERT_EQ(layer.lodDrawCalls, 1);
    EXPECT_EQ(layer.lastDetail, unknownDetail)
        << "the owning layer should decide how an unknown detail value falls back";
}

TEST(Procedural3DLayerLodContractTest,
     UnknownDetailLevelUsesFullProceduralQuality)
{
    const QString high = renderStateForLod(
        Artifact::Procedural3DLayerKind::Terrain,
        Artifact::DetailLevel::High);
    const QString unknown = renderStateForLod(
        Artifact::Procedural3DLayerKind::Terrain,
        static_cast<Artifact::DetailLevel>(99));

    expectQuality(high, 2);
    expectQuality(unknown, 2);
    EXPECT_EQ(vertexCountFromState(unknown), vertexCountFromState(high));
}

TEST(Procedural3DLayerLodContractTest,
     UnknownDetailLevelUsesFullQualityForEveryGenerator)
{
    constexpr std::array<Artifact::Procedural3DLayerKind, 3> kinds{
        Artifact::Procedural3DLayerKind::Terrain,
        Artifact::Procedural3DLayerKind::PathTube,
        Artifact::Procedural3DLayerKind::TextExtrude};
    constexpr auto unknownLevel = static_cast<Artifact::DetailLevel>(99);

    for (const Artifact::Procedural3DLayerKind kind : kinds) {
        const QString high = renderStateForLod(
            kind, Artifact::DetailLevel::High);
        const QString unknown = renderStateForLod(kind, unknownLevel);

        expectQuality(unknown, 2);
        EXPECT_EQ(vertexCountFromState(unknown), vertexCountFromState(high))
            << "unknown detail level should keep the generator's full-quality "
               "mesh for kind "
            << static_cast<int>(kind);
    }
}

TEST(Procedural3DLayerLodContractTest,
     CompositionViewPassesRequestedLODToEveryProceduralGenerator)
{
    constexpr struct LodCase {
        Artifact::DetailLevel detail;
        int expectedQuality;
    } cases[] = {
        {Artifact::DetailLevel::Low, 0},
        {Artifact::DetailLevel::Medium, 1},
        {Artifact::DetailLevel::High, 2},
        {static_cast<Artifact::DetailLevel>(99), 2},
    };

    constexpr std::array<Artifact::Procedural3DLayerKind, 3> kinds{
        Artifact::Procedural3DLayerKind::Terrain,
        Artifact::Procedural3DLayerKind::PathTube,
        Artifact::Procedural3DLayerKind::TextExtrude};
    int argc = 1;
    char applicationName[] = "Procedural3DCompositionLodTest";
    char* argv[] = {applicationName, nullptr};
    QGuiApplication application(argc, argv);
    Artifact::ArtifactIRenderer renderer;

    for (const Artifact::Procedural3DLayerKind kind : kinds) {
        Artifact::ArtifactProcedural3DLayer layer(kind);
        ASSERT_TRUE(layer.setLayerPropertyValue(
            QStringLiteral("procedural.quality"), 2));
        if (kind == Artifact::Procedural3DLayerKind::TextExtrude) {
            ASSERT_TRUE(layer.setLayerPropertyValue(
                QStringLiteral("text.value"), QVariant(QStringLiteral("LOD"))));
        }
        layer.setVisible(true);

        for (const LodCase& testCase : cases) {
            Artifact::drawLayerForCompositionView(
                &layer, &renderer, 1.0f, nullptr, nullptr, nullptr, 0, false,
                testCase.detail);
            expectQuality(layer.debugState(), testCase.expectedQuality);
        }
    }
}

TEST(Procedural3DLayerLodContractTest, TerrainMapsDetailLevelsToGeneratorQuality)
{
    expectQuality(renderStateForLod(Artifact::Procedural3DLayerKind::Terrain,
                                    Artifact::DetailLevel::Low), 0);
    expectQuality(renderStateForLod(Artifact::Procedural3DLayerKind::Terrain,
                                    Artifact::DetailLevel::Medium), 1);
    expectQuality(renderStateForLod(Artifact::Procedural3DLayerKind::Terrain,
                                    Artifact::DetailLevel::High), 2);
}

TEST(Procedural3DLayerLodContractTest, QualityChangesRegenerateWithinTheSameFrame)
{
    Artifact::ArtifactProcedural3DLayer layer(
        Artifact::Procedural3DLayerKind::Terrain);
    Artifact::ArtifactIRenderer renderer;
    layer.setVisible(true);

    layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
    const QString lowState = layer.debugState();
    expectQuality(lowState, 0);
    layer.drawLOD(&renderer, Artifact::DetailLevel::High);
    const QString highState = layer.debugState();
    expectQuality(highState, 2);
    layer.drawLOD(&renderer, Artifact::DetailLevel::Medium);
    const QString mediumState = layer.debugState();
    expectQuality(mediumState, 1);
    EXPECT_LT(vertexCountFromState(lowState), vertexCountFromState(mediumState));
    EXPECT_LT(vertexCountFromState(mediumState), vertexCountFromState(highState));

    layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
    const QString restoredLowState = layer.debugState();
    expectQuality(restoredLowState, 0);
    EXPECT_EQ(vertexCountFromState(restoredLowState), vertexCountFromState(lowState))
        << "returning to Low in the same frame should regenerate the original mesh density";
}

TEST(Procedural3DLayerLodContractTest, PathTubeMapsDetailLevelsToGeneratorQuality)
{
    const QString low = renderStateForLod(Artifact::Procedural3DLayerKind::PathTube,
                                          Artifact::DetailLevel::Low);
    const QString medium = renderStateForLod(Artifact::Procedural3DLayerKind::PathTube,
                                             Artifact::DetailLevel::Medium);
    const QString high = renderStateForLod(Artifact::Procedural3DLayerKind::PathTube,
                                           Artifact::DetailLevel::High);
    expectQuality(low, 0);
    expectQuality(medium, 1);
    expectQuality(high, 2);
    EXPECT_LT(vertexCountFromState(low), vertexCountFromState(medium));
    EXPECT_LT(vertexCountFromState(medium), vertexCountFromState(high));
}

TEST(Procedural3DLayerLodContractTest,
     UnknownDetailLevelUsesFullGeneratorQualityForEveryKind)
{
    constexpr std::array<Artifact::Procedural3DLayerKind, 3> kinds{
        Artifact::Procedural3DLayerKind::Terrain,
        Artifact::Procedural3DLayerKind::PathTube,
        Artifact::Procedural3DLayerKind::TextExtrude};
    constexpr auto unknownDetail = static_cast<Artifact::DetailLevel>(99);

    for (const Artifact::Procedural3DLayerKind kind : kinds) {
        SCOPED_TRACE(static_cast<int>(kind));
        Artifact::ArtifactProcedural3DLayer layer(kind);
        Artifact::ArtifactIRenderer renderer;
        layer.setVisible(true);

        layer.drawLOD(&renderer, Artifact::DetailLevel::High);
        const QString high = layer.debugState();
        layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
        const QString low = layer.debugState();
        layer.drawLOD(&renderer, unknownDetail);
        const QString unknown = layer.debugState();
        layer.drawLOD(&renderer, Artifact::DetailLevel::High);
        const QString highAfterUnknown = layer.debugState();
        layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
        const QString lowAfterUnknown = layer.debugState();

        expectQuality(high, 2);
        expectQuality(low, 0);
        expectQuality(unknown, 2);
        expectQuality(highAfterUnknown, 2);
        expectQuality(lowAfterUnknown, 0);
        EXPECT_EQ(vertexCountFromState(unknown), vertexCountFromState(high))
            << "unknown detail values should use the full-quality mesh "
               "for generator kind " << static_cast<int>(kind);
        EXPECT_EQ(vertexCountFromState(highAfterUnknown),
                  vertexCountFromState(high));
        EXPECT_EQ(vertexCountFromState(lowAfterUnknown),
                  vertexCountFromState(low))
            << "returning to Low after an unknown value should restore the "
               "original mesh density";
    }
}

TEST(Procedural3DLayerLodContractTest,
     PathTubeQualityChangesRegenerateWithinTheSameFrame)
{
    Artifact::ArtifactProcedural3DLayer layer(
        Artifact::Procedural3DLayerKind::PathTube);
    Artifact::ArtifactIRenderer renderer;
    layer.setVisible(true);

    layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
    const QString lowState = layer.debugState();
    layer.drawLOD(&renderer, Artifact::DetailLevel::High);
    const QString highState = layer.debugState();
    layer.drawLOD(&renderer, Artifact::DetailLevel::Medium);
    const QString mediumState = layer.debugState();

    expectQuality(lowState, 0);
    expectQuality(mediumState, 1);
    expectQuality(highState, 2);
    EXPECT_LT(vertexCountFromState(lowState), vertexCountFromState(mediumState));
    EXPECT_LT(vertexCountFromState(mediumState), vertexCountFromState(highState));

    layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
    const QString restoredLowState = layer.debugState();
    expectQuality(restoredLowState, 0);
    EXPECT_EQ(vertexCountFromState(restoredLowState), vertexCountFromState(lowState));
}

TEST(Procedural3DLayerLodContractTest,
     FrameAndQualityChangesRegenerateProceduralMeshesAndRestoreThePriorFrame)
{
    constexpr std::array<Artifact::Procedural3DLayerKind, 2> animatedKinds{
        Artifact::Procedural3DLayerKind::Terrain,
        Artifact::Procedural3DLayerKind::PathTube};
    Artifact::ArtifactIRenderer renderer;

    for (const Artifact::Procedural3DLayerKind kind : animatedKinds) {
        Artifact::ArtifactProcedural3DLayer layer(kind);
        layer.setVisible(true);

        layer.goToFrame(0);
        layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
        const QString firstFrameLow = layer.debugState();
        expectQuality(firstFrameLow, 0);
        EXPECT_NE(firstFrameLow.indexOf(QStringLiteral("frame=0")), -1)
            << firstFrameLow.toStdString();

        layer.goToFrame(12);
        layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
        const QString laterFrameLow = layer.debugState();
        expectQuality(laterFrameLow, 0);
        EXPECT_NE(laterFrameLow.indexOf(QStringLiteral("frame=12")), -1)
            << laterFrameLow.toStdString();
        EXPECT_EQ(vertexCountFromState(firstFrameLow),
                  vertexCountFromState(laterFrameLow))
            << "animation time should not change the selected LOD density";

        layer.drawLOD(&renderer, Artifact::DetailLevel::High);
        const QString laterFrameHigh = layer.debugState();
        expectQuality(laterFrameHigh, 2);
        EXPECT_GT(vertexCountFromState(laterFrameHigh),
                  vertexCountFromState(laterFrameLow));

        layer.goToFrame(0);
        layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
        const QString restoredFirstFrameLow = layer.debugState();
        expectQuality(restoredFirstFrameLow, 0);
        EXPECT_NE(restoredFirstFrameLow.indexOf(QStringLiteral("frame=0")), -1)
            << restoredFirstFrameLow.toStdString();
        EXPECT_EQ(vertexCountFromState(restoredFirstFrameLow),
                  vertexCountFromState(firstFrameLow))
            << "returning to the starting frame should restore its LOD density";
    }
}

TEST(Procedural3DLayerLodContractTest, TextExtrudeRecordsRequestedDetailQuality)
{
    expectQuality(renderStateForLod(Artifact::Procedural3DLayerKind::TextExtrude,
                                    Artifact::DetailLevel::Low), 0);
    expectQuality(renderStateForLod(Artifact::Procedural3DLayerKind::TextExtrude,
                                    Artifact::DetailLevel::Medium), 1);
    expectQuality(renderStateForLod(Artifact::Procedural3DLayerKind::TextExtrude,
                                    Artifact::DetailLevel::High), 2);
}

TEST(Procedural3DLayerLodContractTest, TextExtrudeMeshDensityTracksDetailLevel)
{
    int argc = 1;
    char applicationName[] = "Procedural3DLayerLodContractTest";
    char* argv[] = {applicationName, nullptr};
    QGuiApplication application(argc, argv);

    Artifact::ArtifactProcedural3DLayer layer(
        Artifact::Procedural3DLayerKind::TextExtrude);
    Artifact::ArtifactIRenderer renderer;
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("text.value"), QVariant(QStringLiteral("LOD"))));
    layer.setVisible(true);

    layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
    const QString lowState = layer.debugState();
    layer.drawLOD(&renderer, Artifact::DetailLevel::Medium);
    const QString mediumState = layer.debugState();
    layer.drawLOD(&renderer, Artifact::DetailLevel::High);
    const QString highState = layer.debugState();
    layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
    const QString restoredLowState = layer.debugState();

    expectQuality(lowState, 0);
    expectQuality(mediumState, 1);
    expectQuality(highState, 2);
    expectQuality(restoredLowState, 0);
    EXPECT_GT(vertexCountFromState(lowState), 0);
    EXPECT_LT(vertexCountFromState(lowState), vertexCountFromState(mediumState));
    EXPECT_LT(vertexCountFromState(mediumState), vertexCountFromState(highState));
    EXPECT_EQ(vertexCountFromState(restoredLowState), vertexCountFromState(lowState));
}

TEST(Procedural3DLayerLodContractTest, OrdinaryTerrainDrawRestoresItsConfiguredQualityAfterLodOverride)
{
    Artifact::ArtifactProcedural3DLayer layer(
        Artifact::Procedural3DLayerKind::Terrain);
    Artifact::ArtifactIRenderer renderer;
    layer.setVisible(true);

    layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
    expectQuality(layer.debugState(), 0);

    layer.draw(&renderer);

    const QString ordinaryState = layer.debugState();
    expectQuality(ordinaryState, 1);
    EXPECT_GT(vertexCountFromState(ordinaryState), 0);
}

TEST(Procedural3DLayerLodContractTest, OrdinaryPathTubeDrawRestoresItsConfiguredQualityAfterLodOverride)
{
    Artifact::ArtifactProcedural3DLayer layer(
        Artifact::Procedural3DLayerKind::PathTube);
    Artifact::ArtifactIRenderer renderer;
    layer.setVisible(true);

    layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
    expectQuality(layer.debugState(), 0);

    layer.draw(&renderer);

    const QString ordinaryState = layer.debugState();
    expectQuality(ordinaryState, 1);
    EXPECT_GT(vertexCountFromState(ordinaryState), 0);
}

TEST(Procedural3DLayerLodContractTest,
     OrdinaryTextExtrudeDrawRestoresItsConfiguredQualityAfterLodOverride)
{
    int argc = 1;
    char applicationName[] = "Procedural3DLayerLodContractTest";
    char* argv[] = {applicationName, nullptr};
    QGuiApplication application(argc, argv);

    Artifact::ArtifactProcedural3DLayer layer(
        Artifact::Procedural3DLayerKind::TextExtrude);
    Artifact::ArtifactIRenderer renderer;
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("text.value"), QVariant(QStringLiteral("LOD"))));
    layer.setVisible(true);

    layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
    const QString lowState = layer.debugState();
    expectQuality(lowState, 0);
    layer.draw(&renderer);

    const QString ordinaryState = layer.debugState();
    expectQuality(ordinaryState, 1);
    EXPECT_GT(vertexCountFromState(ordinaryState),
              vertexCountFromState(lowState));
}

TEST(Procedural3DLayerLodContractTest, QualityPersistsForEveryGeneratorAcrossJsonRoundTrip)
{
    for (int quality = 0; quality <= 2; ++quality) {
        Artifact::ArtifactProcedural3DLayer source;
        ASSERT_TRUE(source.setLayerPropertyValue(
            QStringLiteral("procedural.quality"), quality));

        const QJsonObject serialized = source.toJson();
        EXPECT_EQ(serialized.value(QStringLiteral("terrain")).toObject()
                      .value(QStringLiteral("quality")).toInt(), quality);
        EXPECT_EQ(serialized.value(QStringLiteral("pathTube")).toObject()
                      .value(QStringLiteral("quality")).toInt(), quality);
        EXPECT_EQ(serialized.value(QStringLiteral("textExtrude")).toObject()
                      .value(QStringLiteral("quality")).toInt(), quality);
        EXPECT_EQ(serialized.value(QStringLiteral("textExtrude")).toObject()
                      .value(QStringLiteral("maxGlyphCount")).toInt(), 4096);

        auto restored = Artifact::ArtifactProcedural3DLayer::fromJson(serialized);
        ASSERT_TRUE(restored);
        const QJsonObject roundTrip = restored->toJson();
        EXPECT_EQ(roundTrip.value(QStringLiteral("terrain")).toObject()
                      .value(QStringLiteral("quality")).toInt(), quality);
        EXPECT_EQ(roundTrip.value(QStringLiteral("pathTube")).toObject()
                      .value(QStringLiteral("quality")).toInt(), quality);
        EXPECT_EQ(roundTrip.value(QStringLiteral("textExtrude")).toObject()
                      .value(QStringLiteral("quality")).toInt(), quality);
        EXPECT_EQ(roundTrip.value(QStringLiteral("textExtrude")).toObject()
                      .value(QStringLiteral("maxGlyphCount")).toInt(), 4096);
    }
}

TEST(Procedural3DLayerLodContractTest,
     TextFontFingerprintSnapshotPersistsAndIsRecheckedAfterLoad)
{
    int argc = 1;
    char applicationName[] = "Procedural3DTextFontSnapshotTest";
    char* argv[] = {applicationName, nullptr};
    QGuiApplication application(argc, argv);

    const QDir sourceDirectory(
        QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
    const QString bundledFontPath = sourceDirectory.filePath(QStringLiteral(
        "../../libs/openusd/pxr/usdImaging/usdviewq/fonts/Roboto/Roboto-Regular.ttf"));
    ASSERT_TRUE(QFileInfo::exists(bundledFontPath));
    ASSERT_TRUE(ArtifactCore::FontManager::loadFontFromFile(bundledFontPath));

    Artifact::ArtifactProcedural3DLayer source(
        Artifact::Procedural3DLayerKind::TextExtrude);
    ASSERT_TRUE(source.setLayerPropertyValue(
        QStringLiteral("text.value"), QStringLiteral("A")));
    ASSERT_TRUE(source.setLayerPropertyValue(
        QStringLiteral("text.fontFamily"), QStringLiteral("Roboto")));
    source.setVisible(true);
    Artifact::ArtifactIRenderer renderer;
    source.draw(&renderer);

    const QJsonObject sourceText = source.toJson()
        .value(QStringLiteral("procedural3D")).toObject()
        .value(QStringLiteral("textExtrude")).toObject();
    EXPECT_FALSE(sourceText.value(QStringLiteral("resolvedFontFamilies"))
                     .toArray().isEmpty());
    ASSERT_TRUE(sourceText.value(QStringLiteral("fontIdentityComplete")).toBool());
    EXPECT_FALSE(sourceText.value(QStringLiteral("resolvedFontFingerprints"))
                     .toArray().isEmpty());
    bool sourceFontHelpFound = false;
    for (const ArtifactCore::PropertyGroup& group : source.getLayerPropertyGroups()) {
        const auto fontProperty = group.findProperty(QStringLiteral("text.fontFamily"));
        if (!fontProperty) continue;
        sourceFontHelpFound = fontProperty->metadata().inlineHelp.contains(
            QStringLiteral("Resolved fonts:"));
    }
    EXPECT_TRUE(sourceFontHelpFound);

    QJsonObject modifiedProject = source.toJson();
    QJsonObject procedural = modifiedProject.value(
        QStringLiteral("procedural3D")).toObject();
    QJsonObject textSettings = procedural.value(
        QStringLiteral("textExtrude")).toObject();
    QJsonArray expectedFingerprints = textSettings.value(
        QStringLiteral("expectedFontFingerprints")).toArray();
    ASSERT_FALSE(expectedFingerprints.isEmpty());
    expectedFingerprints[0] = QStringLiteral("Roboto|Regular|0000000000000000");
    textSettings[QStringLiteral("expectedFontFingerprints")] = expectedFingerprints;
    procedural[QStringLiteral("textExtrude")] = textSettings;
    modifiedProject[QStringLiteral("procedural3D")] = procedural;

    Artifact::ArtifactProcedural3DLayer restored(
        Artifact::Procedural3DLayerKind::TextExtrude);
    restored.fromJsonProperties(modifiedProject);
    restored.setVisible(true);
    restored.draw(&renderer);
    const QString state = restored.debugState();
    EXPECT_NE(state.indexOf(QStringLiteral("font-identity:changed")), -1);
    bool changedFontHelpFound = false;
    for (const ArtifactCore::PropertyGroup& group : restored.getLayerPropertyGroups()) {
        const auto fontProperty = group.findProperty(QStringLiteral("text.fontFamily"));
        if (!fontProperty) continue;
        changedFontHelpFound = fontProperty->metadata().inlineHelp.contains(
            QStringLiteral("differs from the saved project snapshot"));
    }
    EXPECT_TRUE(changedFontHelpFound);
}

TEST(Procedural3DLayerLodContractTest,
     TextExtrusionTrackingLeadingAndAlignmentPersist)
{
    Artifact::ArtifactProcedural3DLayer source(
        Artifact::Procedural3DLayerKind::TextExtrude);
    ASSERT_TRUE(source.setLayerPropertyValue(
        QStringLiteral("text.letterSpacing"), 18.0));
    ASSERT_TRUE(source.setLayerPropertyValue(
        QStringLiteral("text.lineSpacing"), 1.35));
    ASSERT_TRUE(source.setLayerPropertyValue(
        QStringLiteral("text.alignment"), 2));
    ASSERT_TRUE(source.setLayerPropertyValue(
        QStringLiteral("text.maxGlyphCount"), 8192));

    const QJsonObject sourceText = source.toJson()
        .value(QStringLiteral("procedural3D")).toObject()
        .value(QStringLiteral("textExtrude")).toObject();
    EXPECT_DOUBLE_EQ(sourceText.value(QStringLiteral("letterSpacing")).toDouble(), 18.0);
    EXPECT_DOUBLE_EQ(sourceText.value(QStringLiteral("lineSpacing")).toDouble(), 1.35);
    EXPECT_EQ(sourceText.value(QStringLiteral("alignment")).toInt(), 2);
    EXPECT_EQ(sourceText.value(QStringLiteral("maxGlyphCount")).toInt(), 8192);

    const auto restored = Artifact::ArtifactProcedural3DLayer::fromJson(source.toJson());
    ASSERT_TRUE(restored);
    const QJsonObject roundTripText = restored->toJson()
        .value(QStringLiteral("procedural3D")).toObject()
        .value(QStringLiteral("textExtrude")).toObject();
    EXPECT_DOUBLE_EQ(roundTripText.value(QStringLiteral("letterSpacing")).toDouble(), 18.0);
    EXPECT_DOUBLE_EQ(roundTripText.value(QStringLiteral("lineSpacing")).toDouble(), 1.35);
    EXPECT_EQ(roundTripText.value(QStringLiteral("alignment")).toInt(), 2);
    EXPECT_EQ(roundTripText.value(QStringLiteral("maxGlyphCount")).toInt(), 8192);
}

TEST(Procedural3DLayerLodContractTest,
     TextInputPreservesConfiguredShapingBudgetWithoutSilentTruncation)
{
    Artifact::ArtifactProcedural3DLayer layer(
        Artifact::Procedural3DLayerKind::TextExtrude);
    const QString longText(4097, QLatin1Char('A'));
    ASSERT_TRUE(layer.setLayerPropertyValue(QStringLiteral("text.value"), longText));
    QJsonObject project = layer.toJson();
    QJsonObject procedural = project.value(QStringLiteral("procedural3D")).toObject();
    QJsonObject text = procedural.value(QStringLiteral("textExtrude")).toObject();
    EXPECT_EQ(text.value(QStringLiteral("text")).toString().size(), longText.size());

    const auto restored = Artifact::ArtifactProcedural3DLayer::fromJson(project);
    ASSERT_TRUE(restored);
    const QString restoredText = restored->toJson()
        .value(QStringLiteral("procedural3D")).toObject()
        .value(QStringLiteral("textExtrude")).toObject()
        .value(QStringLiteral("text")).toString();
    EXPECT_EQ(restoredText.size(), longText.size());

    text[QStringLiteral("text")] = QString(70000, QLatin1Char('A'));
    procedural[QStringLiteral("textExtrude")] = text;
    project[QStringLiteral("procedural3D")] = procedural;
    const auto boundedRestore = Artifact::ArtifactProcedural3DLayer::fromJson(project);
    ASSERT_TRUE(boundedRestore);
    const QString boundedText = boundedRestore->toJson()
        .value(QStringLiteral("procedural3D")).toObject()
        .value(QStringLiteral("textExtrude")).toObject()
        .value(QStringLiteral("text")).toString();
    EXPECT_EQ(boundedText.size(), 65536);
}

TEST(Procedural3DLayerLodContractTest,
     LegacyTextExtrudeJsonLoadsWithStableDefaults)
{
    Artifact::ArtifactProcedural3DLayer source(
        Artifact::Procedural3DLayerKind::TextExtrude);
    ASSERT_TRUE(source.setLayerPropertyValue(
        QStringLiteral("text.value"), QStringLiteral("Legacy")));
    QJsonObject legacyProject = source.toJson();
    QJsonObject procedural = legacyProject.value(
        QStringLiteral("procedural3D")).toObject();
    QJsonObject legacyText = procedural.value(
        QStringLiteral("textExtrude")).toObject();
    for (const QString& key : {
             QStringLiteral("letterSpacing"),
             QStringLiteral("lineSpacing"),
             QStringLiteral("alignment"),
             QStringLiteral("maxGlyphCount"),
             QStringLiteral("resolvedFontFamilies"),
             QStringLiteral("resolvedFontFingerprints"),
             QStringLiteral("expectedFontFingerprints"),
             QStringLiteral("fontIdentityComplete"),
             QStringLiteral("fontIdentityChanged"),
             QStringLiteral("fontIdentityUnverified"),
             QStringLiteral("missingGlyphCount"),
             QStringLiteral("missingGlyphLocations"),
             QStringLiteral("missingGlyphLocationsComplete"),
             QStringLiteral("generationStatus"),
             QStringLiteral("generationMessage")}) {
        legacyText.remove(key);
    }
    procedural[QStringLiteral("textExtrude")] = legacyText;
    legacyProject[QStringLiteral("procedural3D")] = procedural;

    const auto restored =
        Artifact::ArtifactProcedural3DLayer::fromJson(legacyProject);
    ASSERT_TRUE(restored);
    const QJsonObject restoredText = restored->toJson()
        .value(QStringLiteral("procedural3D")).toObject()
        .value(QStringLiteral("textExtrude")).toObject();
    EXPECT_EQ(restoredText.value(QStringLiteral("text")).toString(),
              QStringLiteral("Legacy"));
    EXPECT_DOUBLE_EQ(restoredText.value(QStringLiteral("letterSpacing")).toDouble(),
                     0.0);
    EXPECT_DOUBLE_EQ(restoredText.value(QStringLiteral("lineSpacing")).toDouble(),
                     1.0);
    EXPECT_EQ(restoredText.value(QStringLiteral("alignment")).toInt(), 1);
    EXPECT_EQ(restoredText.value(QStringLiteral("maxGlyphCount")).toInt(), 4096);
    EXPECT_TRUE(restoredText.value(
        QStringLiteral("missingGlyphLocationsComplete")).toBool());
}

TEST(Procedural3DLayerLodContractTest,
     MalformedFontDiagnosticsAreBoundedAndMarkedUnverified)
{
    Artifact::ArtifactProcedural3DLayer source(
        Artifact::Procedural3DLayerKind::TextExtrude);
    QJsonObject project = source.toJson();
    QJsonObject procedural = project.value(
        QStringLiteral("procedural3D")).toObject();
    QJsonObject text = procedural.value(
        QStringLiteral("textExtrude")).toObject();

    QJsonArray families;
    QJsonArray fingerprints;
    QJsonArray expectedFingerprints;
    QJsonArray missingLocations;
    for (int index = 0; index < 70; ++index) {
        families.push_back(QStringLiteral("Family").repeated(50));
        fingerprints.push_back(QStringLiteral("Fingerprint").repeated(60));
        expectedFingerprints.push_back(QStringLiteral("Expected").repeated(60));
    }
    for (int index = 0; index < 140; ++index) {
        missingLocations.push_back(QStringLiteral("UTF-16 offset U+10FFFF ").repeated(12));
    }
    text[QStringLiteral("resolvedFontFamilies")] = families;
    text[QStringLiteral("resolvedFontFingerprints")] = fingerprints;
    text[QStringLiteral("expectedFontFingerprints")] = expectedFingerprints;
    text[QStringLiteral("fontIdentityComplete")] = true;
    text[QStringLiteral("fontIdentityChanged")] = true;
    text[QStringLiteral("missingGlyphCount")] = 20000;
    text[QStringLiteral("missingGlyphLocations")] = missingLocations;
    text[QStringLiteral("missingGlyphLocationsComplete")] = true;
    procedural[QStringLiteral("textExtrude")] = text;
    project[QStringLiteral("procedural3D")] = procedural;

    const auto restored = Artifact::ArtifactProcedural3DLayer::fromJson(project);
    ASSERT_TRUE(restored);
    const QJsonObject restoredText = restored->toJson()
        .value(QStringLiteral("procedural3D")).toObject()
        .value(QStringLiteral("textExtrude")).toObject();
    EXPECT_EQ(restoredText.value(QStringLiteral("resolvedFontFamilies"))
                  .toArray().size(), 64);
    EXPECT_EQ(restoredText.value(QStringLiteral("resolvedFontFamilies"))
                  .toArray().at(0).toString().size(), 256);
    EXPECT_EQ(restoredText.value(QStringLiteral("resolvedFontFingerprints"))
                  .toArray().size(), 64);
    EXPECT_EQ(restoredText.value(QStringLiteral("resolvedFontFingerprints"))
                  .toArray().at(0).toString().size(), 512);
    EXPECT_FALSE(restoredText.value(QStringLiteral("fontIdentityComplete")).toBool());
    EXPECT_TRUE(restoredText.value(QStringLiteral("fontIdentityUnverified")).toBool());
    EXPECT_FALSE(restoredText.value(QStringLiteral("fontIdentityChanged")).toBool());
    EXPECT_EQ(restoredText.value(QStringLiteral("missingGlyphCount")).toInt(), 16384);
    EXPECT_EQ(restoredText.value(QStringLiteral("missingGlyphLocations"))
                  .toArray().size(), 128);
    EXPECT_EQ(restoredText.value(QStringLiteral("missingGlyphLocations"))
                  .toArray().at(0).toString().size(), 128);
    EXPECT_FALSE(restoredText.value(
        QStringLiteral("missingGlyphLocationsComplete")).toBool());
}

TEST(Procedural3DLayerLodContractTest,
     TemporaryLodOverrideDoesNotChangeSerializedGeneratorQuality)
{
    int argc = 1;
    char applicationName[] = "Procedural3DLayerLodContractTest";
    char* argv[] = {applicationName, nullptr};
    QGuiApplication application(argc, argv);

    constexpr std::array<Artifact::Procedural3DLayerKind, 3> kinds{
        Artifact::Procedural3DLayerKind::Terrain,
        Artifact::Procedural3DLayerKind::PathTube,
        Artifact::Procedural3DLayerKind::TextExtrude};
    Artifact::ArtifactIRenderer renderer;

    for (const Artifact::Procedural3DLayerKind kind : kinds) {
        Artifact::ArtifactProcedural3DLayer layer(kind);
        ASSERT_TRUE(layer.setLayerPropertyValue(
            QStringLiteral("procedural.quality"), 2));
        layer.setVisible(true);

        layer.drawLOD(&renderer, Artifact::DetailLevel::Low);

        const QJsonObject serialized = layer.toJson();
        EXPECT_EQ(serialized.value(QStringLiteral("terrain")).toObject()
                      .value(QStringLiteral("quality")).toInt(), 2);
        EXPECT_EQ(serialized.value(QStringLiteral("pathTube")).toObject()
                      .value(QStringLiteral("quality")).toInt(), 2);
        EXPECT_EQ(serialized.value(QStringLiteral("textExtrude")).toObject()
                      .value(QStringLiteral("quality")).toInt(), 2);
    }
}

TEST(Procedural3DLayerLodContractTest,
     FullConfiguredQualityIsRestoredAfterTemporaryLowLodDraw)
{
    int argc = 1;
    char applicationName[] = "Procedural3DLayerFullQualityRestoreTest";
    char* argv[] = {applicationName, nullptr};
    QGuiApplication application(argc, argv);

    constexpr std::array<Artifact::Procedural3DLayerKind, 3> kinds{
        Artifact::Procedural3DLayerKind::Terrain,
        Artifact::Procedural3DLayerKind::PathTube,
        Artifact::Procedural3DLayerKind::TextExtrude};
    Artifact::ArtifactIRenderer renderer;

    for (const Artifact::Procedural3DLayerKind kind : kinds) {
        Artifact::ArtifactProcedural3DLayer layer(kind);
        ASSERT_TRUE(layer.setLayerPropertyValue(
            QStringLiteral("procedural.quality"), 2));
        if (kind == Artifact::Procedural3DLayerKind::TextExtrude) {
            ASSERT_TRUE(layer.setLayerPropertyValue(
                QStringLiteral("text.value"), QVariant(QStringLiteral("LOD"))));
        }
        layer.setVisible(true);

        layer.drawLOD(&renderer, Artifact::DetailLevel::Low);
        const QString lowLodState = layer.debugState();
        expectQuality(lowLodState, 0);
        ASSERT_GT(vertexCountFromState(lowLodState), 0);

        layer.draw(&renderer);
        const QString restoredState = layer.debugState();
        expectQuality(restoredState, 2);
        EXPECT_GT(vertexCountFromState(restoredState),
                  vertexCountFromState(lowLodState))
            << "the ordinary draw should restore the configured Full geometry";
    }
}

TEST(Procedural3DLayerLodContractTest, JsonClampsInvalidQualityPerGenerator)
{
    for (const auto [invalidQuality, expectedQuality] : {
             std::pair{-100, 0}, std::pair{100, 2}}) {
        Artifact::ArtifactProcedural3DLayer source;
        QJsonObject serialized = source.toJson();
        for (const auto& field : {QStringLiteral("terrain"),
                                  QStringLiteral("pathTube"),
                                  QStringLiteral("textExtrude")}) {
            QJsonObject generator = serialized.value(field).toObject();
            generator.insert(QStringLiteral("quality"), invalidQuality);
            serialized.insert(field, generator);
        }

        auto restored = Artifact::ArtifactProcedural3DLayer::fromJson(serialized);
        ASSERT_TRUE(restored);
        const QJsonObject roundTrip = restored->toJson();
        for (const auto& field : {QStringLiteral("terrain"),
                                  QStringLiteral("pathTube"),
                                  QStringLiteral("textExtrude")}) {
            EXPECT_EQ(roundTrip.value(field).toObject()
                          .value(QStringLiteral("quality")).toInt(), expectedQuality)
                << field.toStdString()
                << " input quality " << invalidQuality;
        }
    }
}

TEST(Procedural3DLayerLodContractTest, JsonPreservesIndependentGeneratorQualityValues)
{
    Artifact::ArtifactProcedural3DLayer source;
    QJsonObject serialized = source.toJson();
    const struct GeneratorQuality {
        QString field;
        int quality;
    } cases[] = {
        {QStringLiteral("terrain"), 0},
        {QStringLiteral("pathTube"), 1},
        {QStringLiteral("textExtrude"), 2},
    };
    for (const auto& testCase : cases) {
        QJsonObject generator = serialized.value(testCase.field).toObject();
        generator.insert(QStringLiteral("quality"), testCase.quality);
        serialized.insert(testCase.field, generator);
    }

    auto restored = Artifact::ArtifactProcedural3DLayer::fromJson(serialized);
    ASSERT_TRUE(restored);
    const QJsonObject roundTrip = restored->toJson();
    for (const auto& testCase : cases) {
        EXPECT_EQ(roundTrip.value(testCase.field).toObject()
                      .value(QStringLiteral("quality")).toInt(), testCase.quality)
            << testCase.field.toStdString();
    }
}
