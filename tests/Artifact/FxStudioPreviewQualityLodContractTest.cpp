#include <gtest/gtest.h>

#include <utility>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

import Artifact.FxStudio.Session;
import Artifact.FxStudio.ViewportModel;

using Artifact::FxStudio::PreviewQuality;
using Artifact::FxStudio::Session;

namespace {

QJsonObject sessionWithQuality(const int quality)
{
    QJsonObject object;
    object.insert(QStringLiteral("schemaVersion"), 1);
    object.insert(QStringLiteral("eventTrack"),
                  QJsonObject{{QStringLiteral("schemaVersion"), 1}});
    object.insert(QStringLiteral("quality"), quality);
    return object;
}

} // namespace

TEST(FxStudioPreviewQualityLodContractTest, EveryQualityRoundTripsThroughSessionJson)
{
    for (const PreviewQuality quality : {PreviewQuality::Draft,
                                         PreviewQuality::Interactive,
                                         PreviewQuality::Final}) {
        Session source;
        source.viewport().setQuality(quality);

        const QJsonObject serialized = source.toJson();
        EXPECT_EQ(serialized.value(QStringLiteral("quality")).toInt(),
                  static_cast<int>(quality));

        Session restored;
        ASSERT_TRUE(restored.fromJson(serialized));
        EXPECT_EQ(restored.viewport().quality(), quality);
        EXPECT_EQ(restored.toJson().value(QStringLiteral("quality")).toInt(),
                  static_cast<int>(quality));
    }
}

TEST(FxStudioPreviewQualityLodContractTest, MissingQualityUsesInteractiveDefault)
{
    Session restored;
    ASSERT_TRUE(restored.fromJson(sessionWithQuality(1)));
    EXPECT_EQ(restored.viewport().quality(), PreviewQuality::Interactive);

    QJsonObject legacy = sessionWithQuality(1);
    legacy.remove(QStringLiteral("quality"));
    ASSERT_TRUE(restored.fromJson(legacy));
    EXPECT_EQ(restored.viewport().quality(), PreviewQuality::Interactive);
}

TEST(FxStudioPreviewQualityLodContractTest, OutOfRangeQualityValuesClampToSupportedLevels)
{
    for (const auto [serialized, expected] : {
             std::pair{-100, PreviewQuality::Draft},
             std::pair{3, PreviewQuality::Final},
             std::pair{100, PreviewQuality::Final}}) {
        Session restored;
        ASSERT_TRUE(restored.fromJson(sessionWithQuality(serialized)));
        EXPECT_EQ(restored.viewport().quality(), expected);
    }
}

TEST(FxStudioPreviewQualityLodContractTest, NonIntegerQualityFallsBackToInteractiveDefault)
{
    const auto expectInteractiveFallback = [](const QJsonValue& quality) {
        QJsonObject object = sessionWithQuality(0);
        object.insert(QStringLiteral("quality"), quality);

        Session restored;
        ASSERT_TRUE(restored.fromJson(object));
        EXPECT_EQ(restored.viewport().quality(), PreviewQuality::Interactive);
    };

    expectInteractiveFallback(QJsonValue(QStringLiteral("Final")));
    expectInteractiveFallback(QJsonValue(1.5));
    expectInteractiveFallback(QJsonValue(true));
    expectInteractiveFallback(QJsonValue(QJsonValue::Null));
}

TEST(FxStudioPreviewQualityLodContractTest,
     ViewportQualityTransitionsRemainIndependentAcrossModels)
{
    Artifact::FxStudio::ViewportModel first;
    Artifact::FxStudio::ViewportModel second;

    EXPECT_EQ(first.quality(), PreviewQuality::Interactive);
    EXPECT_EQ(second.quality(), PreviewQuality::Interactive);

    first.setQuality(PreviewQuality::Draft);
    EXPECT_EQ(first.quality(), PreviewQuality::Draft);
    EXPECT_EQ(second.quality(), PreviewQuality::Interactive);

    first.setQuality(PreviewQuality::Final);
    second.setQuality(PreviewQuality::Draft);
    EXPECT_EQ(first.quality(), PreviewQuality::Final);
    EXPECT_EQ(second.quality(), PreviewQuality::Draft);

    first.setQuality(PreviewQuality::Interactive);
    EXPECT_EQ(first.quality(), PreviewQuality::Interactive);
    EXPECT_EQ(second.quality(), PreviewQuality::Draft);
}

TEST(FxStudioPreviewQualityLodContractTest,
     FailedSessionRestoreDoesNotPartiallyChangePreviewQuality)
{
    Session session;
    session.viewport().setQuality(PreviewQuality::Final);

    QJsonObject invalidSession = sessionWithQuality(
        static_cast<int>(PreviewQuality::Draft));
    invalidSession.insert(QStringLiteral("eventTrack"),
                          QJsonObject{{QStringLiteral("schemaVersion"), 99}});

    EXPECT_FALSE(session.fromJson(invalidSession));
    EXPECT_EQ(session.viewport().quality(), PreviewQuality::Final)
        << "a rejected session must not partially apply its Draft LOD value";
}
