#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

#include <QObject>
#include <QMetaMethod>

import Artifact.LOD.Manager;
import Artifact.Render.IRenderer;

using namespace Artifact;

TEST(LODManagerContractTest, SelectsDetailLevelAtDefaultZoomBoundaries)
{
    LODManager manager;

    EXPECT_EQ(manager.getDetailLevel(0.0f), LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(0.25f), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(0.75f), LODManager::DetailLevel::High);
}

TEST(LODManagerContractTest, RendererRetainsEachRequestedDetailLevelAcrossTransitions)
{
    ArtifactIRenderer renderer;
    EXPECT_EQ(renderer.detailLevel(), LODManager::DetailLevel::High);

    for (const LODManager::DetailLevel level : {
             LODManager::DetailLevel::Low,
             LODManager::DetailLevel::Medium,
             LODManager::DetailLevel::High,
             LODManager::DetailLevel::Low}) {
        renderer.setDetailLevel(level);
        EXPECT_EQ(renderer.detailLevel(), level);
    }
}

TEST(LODManagerContractTest, ExposesStableSignalContractsThroughMetaObject)
{
    const QMetaObject& metaObject = LODManager::staticMetaObject;
    bool foundDetailLevelChanged = false;
    bool foundThresholdsChanged = false;

    for (int methodIndex = metaObject.methodOffset();
         methodIndex < metaObject.methodCount(); ++methodIndex) {
        const QMetaMethod method = metaObject.method(methodIndex);
        if (method.methodType() != QMetaMethod::Signal) continue;

        const QByteArray signature = method.methodSignature();
        if (signature == QByteArrayLiteral("detailLevelChanged(int,int)")) {
            foundDetailLevelChanged = true;
            EXPECT_EQ(method.parameterCount(), 2);
            EXPECT_EQ(method.parameterTypes().at(0), QByteArrayLiteral("int"));
            EXPECT_EQ(method.parameterTypes().at(1), QByteArrayLiteral("int"));
        } else if (signature == QByteArrayLiteral("thresholdsChanged()")) {
            foundThresholdsChanged = true;
            EXPECT_EQ(method.parameterCount(), 0);
        }
    }

    EXPECT_TRUE(foundDetailLevelChanged);
    EXPECT_TRUE(foundThresholdsChanged);
}

TEST(LODManagerContractTest,
     DetailLevelNotificationsReportOnlyActualTransitions)
{
    LODManager manager;
    std::vector<std::pair<int, int>> transitions;
    QObject::connect(&manager, &LODManager::detailLevelChanged,
                     [&transitions](const int oldLevel, const int newLevel) {
                         transitions.emplace_back(oldLevel, newLevel);
                     });

    EXPECT_EQ(manager.getDetailLevel(0.1f), LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(0.2f), LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(0.5f), LODManager::DetailLevel::Medium);
    EXPECT_NEAR(manager.calculateLODFactor(0.9f), 0.9f, 1.0e-6f);
    EXPECT_EQ(manager.getDetailLevel(0.9f), LODManager::DetailLevel::High);
    EXPECT_EQ(manager.getDetailLevel(0.9f), LODManager::DetailLevel::High);

    ASSERT_EQ(transitions.size(), 3u);
    EXPECT_EQ(transitions[0], (std::pair{
        static_cast<int>(LODManager::DetailLevel::Medium),
        static_cast<int>(LODManager::DetailLevel::Low)}));
    EXPECT_EQ(transitions[1], (std::pair{
        static_cast<int>(LODManager::DetailLevel::Low),
        static_cast<int>(LODManager::DetailLevel::Medium)}));
    EXPECT_EQ(transitions[2], (std::pair{
        static_cast<int>(LODManager::DetailLevel::Medium),
        static_cast<int>(LODManager::DetailLevel::High)}));
}

TEST(LODManagerContractTest,
     ThresholdNotificationsTrackAcceptedChangesOnly)
{
    LODManager manager;
    int notificationCount = 0;
    QObject::connect(&manager, &LODManager::thresholdsChanged,
                     [&notificationCount] { ++notificationCount; });

    manager.setThresholds(0.2f, 0.6f);
    EXPECT_EQ(notificationCount, 1);
    EXPECT_FLOAT_EQ(manager.lowThreshold(), 0.2f);
    EXPECT_FLOAT_EQ(manager.mediumThreshold(), 0.6f);

    manager.setThresholds(0.7f, 0.6f);
    EXPECT_EQ(notificationCount, 1)
        << "rejected thresholds must not announce a policy change";
    EXPECT_FLOAT_EQ(manager.lowThreshold(), 0.2f);
    EXPECT_FLOAT_EQ(manager.mediumThreshold(), 0.6f);

    manager.setThresholds(0.3f, 0.8f);
    EXPECT_EQ(notificationCount, 2);
    EXPECT_FLOAT_EQ(manager.lowThreshold(), 0.3f);
    EXPECT_FLOAT_EQ(manager.mediumThreshold(), 0.8f);
}

TEST(LODManagerContractTest, CalculatesFactorAcrossDefaultDetailBands)
{
    LODManager manager;

    EXPECT_FLOAT_EQ(manager.calculateLODFactor(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(0.25f), 0.5f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(0.5f), 0.625f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(0.75f), 0.75f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(1.0f), 1.0f);
}

TEST(LODManagerContractTest, FactorStaysBoundedAndMonotonicAcrossValidThresholdPairs)
{
    const std::array<std::pair<float, float>, 5> thresholdPairs{{
        {0.01f, 0.02f},
        {0.05f, 0.95f},
        {0.2f, 0.6f},
        {0.5f, 0.5001f},
        {0.98f, 0.99f},
    }};

    for (const auto& [lowThreshold, mediumThreshold] : thresholdPairs) {
        LODManager manager;
        manager.setThresholds(lowThreshold, mediumThreshold);

        float previousFactor = manager.calculateLODFactor(-1.0f);
        EXPECT_FLOAT_EQ(previousFactor, 0.0f);
        for (int step = 0; step <= 3000; ++step) {
            const float zoom = -1.0f + static_cast<float>(step) * 0.001f;
            const float factor = manager.calculateLODFactor(zoom);
            EXPECT_TRUE(std::isfinite(factor));
            EXPECT_GE(factor, 0.0f);
            EXPECT_LE(factor, 1.0f);
            EXPECT_GE(factor, previousFactor)
                << "factor should not decrease as zoom increases; thresholds="
                << lowThreshold << ", " << mediumThreshold << ", zoom=" << zoom;
            previousFactor = factor;

            const auto expectedLevel = zoom < lowThreshold
                ? LODManager::DetailLevel::Low
                : zoom < mediumThreshold ? LODManager::DetailLevel::Medium
                                         : LODManager::DetailLevel::High;
            EXPECT_EQ(manager.getDetailLevel(zoom), expectedLevel)
                << "detail band should agree with the configured thresholds";
        }

        EXPECT_EQ(manager.getDetailLevel(lowThreshold),
                  LODManager::DetailLevel::Medium);
        EXPECT_EQ(manager.getDetailLevel(mediumThreshold),
                  LODManager::DetailLevel::High);
        EXPECT_FLOAT_EQ(manager.calculateLODFactor(lowThreshold), 0.5f);
        EXPECT_FLOAT_EQ(manager.calculateLODFactor(mediumThreshold), 0.75f);
        EXPECT_FLOAT_EQ(manager.calculateLODFactor(2.0f), 1.0f);
    }
}

TEST(LODManagerContractTest, CustomThresholdsChangeSelectionAndFactor)
{
    LODManager manager;
    manager.setThresholds(0.2f, 0.6f);

    EXPECT_FLOAT_EQ(manager.lowThreshold(), 0.2f);
    EXPECT_FLOAT_EQ(manager.mediumThreshold(), 0.6f);
    EXPECT_EQ(manager.getDetailLevel(0.19f), LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(0.2f), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(0.6f), LODManager::DetailLevel::High);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(0.4f), 0.625f);

    const float belowLow = std::nextafter(
        0.2f, -std::numeric_limits<float>::infinity());
    const float aboveLow = std::nextafter(
        0.2f, std::numeric_limits<float>::infinity());
    const float belowMedium = std::nextafter(
        0.6f, -std::numeric_limits<float>::infinity());
    const float aboveMedium = std::nextafter(
        0.6f, std::numeric_limits<float>::infinity());
    EXPECT_EQ(manager.getDetailLevel(belowLow), LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(aboveLow), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(belowMedium), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(aboveMedium), LODManager::DetailLevel::High);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(0.2f), 0.5f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(0.6f), 0.75f);
    EXPECT_NEAR(manager.calculateLODFactor(belowLow), 0.5f, 1.0e-6f);
    EXPECT_NEAR(manager.calculateLODFactor(aboveLow), 0.5f, 1.0e-6f);
    EXPECT_NEAR(manager.calculateLODFactor(belowMedium), 0.75f, 1.0e-6f);
    EXPECT_NEAR(manager.calculateLODFactor(aboveMedium), 0.75f, 1.0e-6f);
}

TEST(LODManagerContractTest, FactorClampsOutsideTheNominalZoomRange)
{
    LODManager manager;

    EXPECT_FLOAT_EQ(manager.calculateLODFactor(-1.0f), 0.0f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(2.0f), 1.0f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(
                        -std::numeric_limits<float>::max()),
                    0.0f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(
                        std::numeric_limits<float>::max()),
                    1.0f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(
                        -std::numeric_limits<float>::infinity()),
                    0.0f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(
                        std::numeric_limits<float>::infinity()),
                    1.0f);
}

TEST(LODManagerContractTest, NonFiniteZoomUsesComparisonFallbacks)
{
    LODManager manager;

    EXPECT_EQ(manager.getDetailLevel(-std::numeric_limits<float>::infinity()),
              LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(std::numeric_limits<float>::infinity()),
              LODManager::DetailLevel::High);
    EXPECT_EQ(manager.getDetailLevel(std::numeric_limits<float>::quiet_NaN()),
              LODManager::DetailLevel::High);

    EXPECT_FLOAT_EQ(manager.calculateLODFactor(
                        -std::numeric_limits<float>::infinity()),
                    0.0f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(
                        std::numeric_limits<float>::infinity()),
                    1.0f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(
                        std::numeric_limits<float>::quiet_NaN()),
                    0.0f);
}

TEST(LODManagerContractTest, InvalidThresholdPairsLeaveTheCurrentConfigurationIntact)
{
    LODManager manager;
    manager.setThresholds(0.2f, 0.6f);

    manager.setThresholds(0.0f, 0.8f);
    EXPECT_FLOAT_EQ(manager.lowThreshold(), 0.2f);
    EXPECT_FLOAT_EQ(manager.mediumThreshold(), 0.6f);

    manager.setThresholds(0.7f, 0.6f);
    EXPECT_FLOAT_EQ(manager.lowThreshold(), 0.2f);
    EXPECT_FLOAT_EQ(manager.mediumThreshold(), 0.6f);

    manager.setThresholds(0.4f, 0.4f);
    EXPECT_FLOAT_EQ(manager.lowThreshold(), 0.2f);
    EXPECT_FLOAT_EQ(manager.mediumThreshold(), 0.6f);

    manager.setThresholds(0.2f, 1.0f);
    EXPECT_FLOAT_EQ(manager.lowThreshold(), 0.2f);
    EXPECT_FLOAT_EQ(manager.mediumThreshold(), 0.6f);

}

TEST(LODManagerContractTest, NonFiniteThresholdsAreRejectedWithoutChangingPriorValues)
{
    const auto expectRejected = [](float low, float medium) {
        LODManager manager;
        manager.setThresholds(0.2f, 0.6f);
        manager.setThresholds(low, medium);
        EXPECT_FLOAT_EQ(manager.lowThreshold(), 0.2f);
        EXPECT_FLOAT_EQ(manager.mediumThreshold(), 0.6f);
    };

    const float infinity = std::numeric_limits<float>::infinity();
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    expectRejected(notANumber, 0.6f);
    expectRejected(0.2f, notANumber);
    expectRejected(infinity, 0.6f);
    expectRejected(-infinity, 0.6f);
    expectRejected(0.2f, infinity);
    expectRejected(0.2f, -infinity);
}

TEST(LODManagerContractTest, AcceptsExtremeValidThresholdEndpoints)
{
    LODManager manager;
    const float lowThreshold = std::nextafter(0.0f, 1.0f);
    const float mediumThreshold = std::nextafter(1.0f, 0.0f);

    manager.setThresholds(lowThreshold, mediumThreshold);

    EXPECT_FLOAT_EQ(manager.lowThreshold(), lowThreshold);
    EXPECT_FLOAT_EQ(manager.mediumThreshold(), mediumThreshold);
    EXPECT_EQ(manager.getDetailLevel(0.0f), LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(lowThreshold), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(mediumThreshold), LODManager::DetailLevel::High);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(lowThreshold), 0.5f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(mediumThreshold), 0.75f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(1.0f),
                    0.75f + (1.0f - mediumThreshold));
}

TEST(LODManagerContractTest, SelectionTracksZoomInBothDirections)
{
    LODManager manager;
    EXPECT_EQ(manager.getDetailLevel(0.1f), LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(0.5f), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(0.9f), LODManager::DetailLevel::High);
    EXPECT_EQ(manager.getDetailLevel(0.5f), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(0.1f), LODManager::DetailLevel::Low);
}

TEST(LODManagerContractTest,
     AdjacentZoomValuesCrossBothDefaultBoundariesWithoutHysteresis)
{
    LODManager manager;
    constexpr float lowBoundary = 0.25f;
    constexpr float highBoundary = 0.75f;
    const float belowLow = std::nextafter(
        lowBoundary, -std::numeric_limits<float>::infinity());
    const float aboveLow = std::nextafter(
        lowBoundary, std::numeric_limits<float>::infinity());
    const float belowHigh = std::nextafter(
        highBoundary, -std::numeric_limits<float>::infinity());
    const float aboveHigh = std::nextafter(
        highBoundary, std::numeric_limits<float>::infinity());

    EXPECT_EQ(manager.getDetailLevel(belowLow), LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(lowBoundary),
              LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(belowLow), LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(aboveLow), LODManager::DetailLevel::Medium);

    EXPECT_EQ(manager.getDetailLevel(belowHigh), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(highBoundary),
              LODManager::DetailLevel::High);
    EXPECT_EQ(manager.getDetailLevel(belowHigh), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(aboveHigh), LODManager::DetailLevel::High);
}

TEST(LODManagerContractTest, NonFiniteZoomDoesNotPoisonFollowingSelections)
{
    LODManager manager;
    struct InvalidZoomCase {
        float zoom;
        LODManager::DetailLevel expectedLevel;
    };
    const InvalidZoomCase invalidValues[] = {
        {std::numeric_limits<float>::quiet_NaN(),
         LODManager::DetailLevel::High},
        {std::numeric_limits<float>::infinity(),
         LODManager::DetailLevel::High},
        {-std::numeric_limits<float>::infinity(),
         LODManager::DetailLevel::Low},
    };

    for (const InvalidZoomCase& testCase : invalidValues) {
        EXPECT_EQ(manager.getDetailLevel(testCase.zoom), testCase.expectedLevel);
        EXPECT_EQ(manager.getDetailLevel(0.5f),
                  LODManager::DetailLevel::Medium);
        EXPECT_EQ(manager.getDetailLevel(0.1f),
                  LODManager::DetailLevel::Low);
        EXPECT_EQ(manager.getDetailLevel(0.9f),
                  LODManager::DetailLevel::High);
    }
}

TEST(LODManagerContractTest,
     NonFiniteZoomFactorsStayFiniteAndDoNotPoisonFollowingFactors)
{
    LODManager manager;
    struct InvalidZoomCase {
        float zoom;
        LODManager::DetailLevel expectedLevel;
        float expectedFactor;
    };
    const InvalidZoomCase invalidValues[] = {
        {std::numeric_limits<float>::quiet_NaN(),
         LODManager::DetailLevel::High, 0.0f},
        {std::numeric_limits<float>::infinity(),
         LODManager::DetailLevel::High, 1.0f},
        {-std::numeric_limits<float>::infinity(),
         LODManager::DetailLevel::Low, 0.0f},
    };

    for (const InvalidZoomCase& testCase : invalidValues) {
        EXPECT_EQ(manager.getDetailLevel(testCase.zoom), testCase.expectedLevel);
        const float factor = manager.calculateLODFactor(testCase.zoom);
        EXPECT_TRUE(std::isfinite(factor));
        EXPECT_FLOAT_EQ(factor, testCase.expectedFactor);
        EXPECT_EQ(manager.getDetailLevel(0.5f), LODManager::DetailLevel::Medium);
        EXPECT_FLOAT_EQ(manager.calculateLODFactor(0.5f), 0.625f);
    }
}

TEST(LODManagerContractTest, ThresholdUpdatesTakeEffectImmediatelyAndRemainStable)
{
    LODManager manager;
    manager.setThresholds(0.2f, 0.6f);

    EXPECT_FLOAT_EQ(manager.lowThreshold(), 0.2f);
    EXPECT_FLOAT_EQ(manager.mediumThreshold(), 0.6f);
    EXPECT_EQ(manager.getDetailLevel(0.199f), LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(0.2f), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(0.599f), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(0.6f), LODManager::DetailLevel::High);

    manager.setThresholds(0.2f, 0.6f);
    EXPECT_EQ(manager.getDetailLevel(0.2f), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(0.6f), LODManager::DetailLevel::High);
}

TEST(LODManagerContractTest,
     ChangingThresholdsReclassifiesAnUnchangedZoomImmediately)
{
    LODManager manager;
    constexpr float zoom = 0.4f;

    EXPECT_EQ(manager.getDetailLevel(zoom), LODManager::DetailLevel::Medium);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(zoom), 0.575f);

    manager.setThresholds(0.5f, 0.9f);
    EXPECT_EQ(manager.getDetailLevel(zoom), LODManager::DetailLevel::Low);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(zoom), 0.4f);

    manager.setThresholds(0.2f, 0.3f);
    EXPECT_EQ(manager.getDetailLevel(zoom), LODManager::DetailLevel::High);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(zoom), 0.85f);
}

TEST(LODManagerContractTest,
     ThresholdRoundTripsNotifyTheReclassifiedLevelExactlyOnce)
{
    LODManager manager;
    std::vector<std::pair<int, int>> transitions;
    QObject::connect(&manager, &LODManager::detailLevelChanged,
                     [&transitions](const int oldLevel, const int newLevel) {
                         transitions.emplace_back(oldLevel, newLevel);
                     });
    constexpr float unchangedZoom = 0.4f;

    EXPECT_EQ(manager.getDetailLevel(unchangedZoom),
              LODManager::DetailLevel::Medium);
    manager.setThresholds(0.5f, 0.9f);
    EXPECT_EQ(manager.getDetailLevel(unchangedZoom),
              LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(unchangedZoom),
              LODManager::DetailLevel::Low);
    manager.setThresholds(0.2f, 0.3f);
    EXPECT_EQ(manager.getDetailLevel(unchangedZoom),
              LODManager::DetailLevel::High);
    EXPECT_EQ(manager.getDetailLevel(unchangedZoom),
              LODManager::DetailLevel::High);

    ASSERT_EQ(transitions.size(), 2u);
    EXPECT_EQ(transitions[0], (std::pair{
        static_cast<int>(LODManager::DetailLevel::Medium),
        static_cast<int>(LODManager::DetailLevel::Low)}));
    EXPECT_EQ(transitions[1], (std::pair{
        static_cast<int>(LODManager::DetailLevel::Low),
        static_cast<int>(LODManager::DetailLevel::High)}));
}

TEST(LODManagerContractTest, ThresholdTransitionsAreStableAtAdjacentFloatValues)
{
    LODManager manager;
    constexpr float lowBoundary = 0.25f;
    constexpr float highBoundary = 0.75f;
    const float belowLow = std::nextafter(
        lowBoundary, -std::numeric_limits<float>::infinity());
    const float aboveLow = std::nextafter(
        lowBoundary, std::numeric_limits<float>::infinity());
    const float belowHigh = std::nextafter(
        highBoundary, -std::numeric_limits<float>::infinity());
    const float aboveHigh = std::nextafter(
        highBoundary, std::numeric_limits<float>::infinity());

    EXPECT_EQ(manager.getDetailLevel(belowLow), LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(lowBoundary), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(aboveLow), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(belowHigh), LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(highBoundary), LODManager::DetailLevel::High);
    EXPECT_EQ(manager.getDetailLevel(aboveHigh), LODManager::DetailLevel::High);
}

TEST(LODManagerContractTest, FactorIsMonotonicAndContinuousAcrossThresholds)
{
    LODManager manager;
    constexpr float lowBoundary = 0.25f;
    constexpr float highBoundary = 0.75f;
    const float zooms[] = {
        std::nextafter(lowBoundary, -std::numeric_limits<float>::infinity()),
        lowBoundary,
        std::nextafter(lowBoundary, std::numeric_limits<float>::infinity()),
        std::nextafter(highBoundary, -std::numeric_limits<float>::infinity()),
        highBoundary,
        std::nextafter(highBoundary, std::numeric_limits<float>::infinity()),
    };

    float previousFactor = manager.calculateLODFactor(zooms[0]);
    for (std::size_t index = 1; index < std::size(zooms); ++index) {
        const float factor = manager.calculateLODFactor(zooms[index]);
        EXPECT_GE(factor, previousFactor);
        previousFactor = factor;
    }
    EXPECT_NEAR(manager.calculateLODFactor(
                    std::nextafter(lowBoundary,
                                   -std::numeric_limits<float>::infinity())),
                0.5f, 1.0e-6f);
    EXPECT_NEAR(manager.calculateLODFactor(
                    std::nextafter(highBoundary,
                                   -std::numeric_limits<float>::infinity())),
                0.75f, 1.0e-6f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(lowBoundary), 0.5f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(highBoundary), 0.75f);
}

TEST(LODManagerContractTest,
     CustomThresholdFactorStaysBoundedAndMonotonicAcrossZoomRange)
{
    LODManager manager;
    constexpr float lowBoundary = 0.17f;
    constexpr float mediumBoundary = 0.83f;
    manager.setThresholds(lowBoundary, mediumBoundary);

    float previousFactor = manager.calculateLODFactor(-1.0f);
    ASSERT_FLOAT_EQ(previousFactor, 0.0f);
    for (int sample = 1; sample <= 3000; ++sample) {
        const float zoom = -1.0f + static_cast<float>(sample) * 0.001f;
        const float factor = manager.calculateLODFactor(zoom);
        ASSERT_TRUE(std::isfinite(factor)) << "zoom=" << zoom;
        EXPECT_GE(factor, 0.0f) << "zoom=" << zoom;
        EXPECT_LE(factor, 1.0f) << "zoom=" << zoom;
        EXPECT_GE(factor, previousFactor) << "zoom=" << zoom;
        previousFactor = factor;

        const auto expectedLevel = zoom < lowBoundary
            ? LODManager::DetailLevel::Low
            : zoom < mediumBoundary ? LODManager::DetailLevel::Medium
                                    : LODManager::DetailLevel::High;
        EXPECT_EQ(manager.getDetailLevel(zoom), expectedLevel)
            << "zoom=" << zoom;
    }

    EXPECT_FLOAT_EQ(manager.calculateLODFactor(lowBoundary), 0.5f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(mediumBoundary), 0.75f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(2.0f), 1.0f);
}

TEST(LODManagerContractTest,
     AdjacentCustomThresholdsKeepTheNarrowMediumBandFinite)
{
    LODManager manager;
    constexpr float lowBoundary = 0.5f;
    const float mediumBoundary = std::nextafter(
        lowBoundary, std::numeric_limits<float>::infinity());
    ASSERT_GT(mediumBoundary, lowBoundary);

    manager.setThresholds(lowBoundary, mediumBoundary);

    EXPECT_EQ(manager.getDetailLevel(
                  std::nextafter(lowBoundary,
                                 -std::numeric_limits<float>::infinity())),
              LODManager::DetailLevel::Low);
    EXPECT_EQ(manager.getDetailLevel(lowBoundary),
              LODManager::DetailLevel::Medium);
    EXPECT_EQ(manager.getDetailLevel(mediumBoundary),
              LODManager::DetailLevel::High);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(lowBoundary), 0.5f);
    EXPECT_FLOAT_EQ(manager.calculateLODFactor(mediumBoundary), 0.75f);
    EXPECT_TRUE(std::isfinite(manager.calculateLODFactor(lowBoundary)));
    EXPECT_TRUE(std::isfinite(manager.calculateLODFactor(mediumBoundary)));
}
