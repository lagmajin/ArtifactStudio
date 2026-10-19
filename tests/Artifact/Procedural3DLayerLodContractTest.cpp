#include <gtest/gtest.h>

#include <array>
#include <utility>

#include <QGuiApplication>
#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QVariant>

import Artifact.Layer.Abstract;
import Artifact.Layer.Procedural3D;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.IRenderer;

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

        auto restored = Artifact::ArtifactProcedural3DLayer::fromJson(serialized);
        ASSERT_TRUE(restored);
        const QJsonObject roundTrip = restored->toJson();
        EXPECT_EQ(roundTrip.value(QStringLiteral("terrain")).toObject()
                      .value(QStringLiteral("quality")).toInt(), quality);
        EXPECT_EQ(roundTrip.value(QStringLiteral("pathTube")).toObject()
                      .value(QStringLiteral("quality")).toInt(), quality);
        EXPECT_EQ(roundTrip.value(QStringLiteral("textExtrude")).toObject()
                      .value(QStringLiteral("quality")).toInt(), quality);
    }
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
