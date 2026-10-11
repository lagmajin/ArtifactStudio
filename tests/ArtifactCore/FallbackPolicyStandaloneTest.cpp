#include <gtest/gtest.h>

#include <QDateTime>
#include <QString>

#include <functional>

import Core.Diagnostics.FallbackPolicy;
import Utils.Optional;

using namespace ArtifactCore;

namespace {
FallbackTracker& resetTracker()
{
    auto& tracker = *FallbackTracker::instance();
    tracker.clear();
    tracker.setWarningsEnabled(false);
    tracker.setPolicy(FallbackCategory::Font, FallbackPolicy::defaultFont());
    tracker.setPolicy(FallbackCategory::Image, FallbackPolicy::defaultImage());
    tracker.setPolicy(FallbackCategory::Color, FallbackPolicy::defaultColor());
    tracker.setPolicy(FallbackCategory::Effect, FallbackPolicy::defaultEffect());
    return tracker;
}
}

TEST(FallbackPolicyStandaloneTest, DefaultPoliciesMatchEachResourceCategory)
{
    auto& tracker = resetTracker();
    EXPECT_EQ(tracker.policy(FallbackCategory::Font).fallbackValue, QStringLiteral("Arial"));
    EXPECT_EQ(tracker.policy(FallbackCategory::Image).fallbackValue, QStringLiteral("placeholder"));
    EXPECT_EQ(tracker.policy(FallbackCategory::Color).action, FallbackAction::Warning);
    EXPECT_EQ(tracker.policy(FallbackCategory::Effect).action, FallbackAction::Bypass);
}

TEST(FallbackPolicyStandaloneTest, SuccessfulResolverReturnsValueWithoutCallingFallback)
{
    auto& tracker = resetTracker();
    int fallbackCalls = 0;
    const auto result = tryFallbackPolicy<int>(FallbackCategory::Font,
        [] { return Optional<int>(17); },
        [&fallbackCalls] { ++fallbackCalls; return 99; },
        QStringLiteral("primary"));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 17);
    EXPECT_EQ(fallbackCalls, 0);
    EXPECT_EQ(tracker.totalCount(), 0);
}

TEST(FallbackPolicyStandaloneTest, MissingResourceUsesFallbackAndRecordsEvent)
{
    auto& tracker = resetTracker();
    int resolverCalls = 0;
    int fallbackCalls = 0;
    const auto result = tryFallbackPolicy<int>(FallbackCategory::Font,
        [&resolverCalls] { ++resolverCalls; return Optional<int>{}; },
        [&fallbackCalls] { ++fallbackCalls; return 42; },
        QStringLiteral("missing-font"));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
    EXPECT_EQ(resolverCalls, 1);
    EXPECT_EQ(fallbackCalls, 1);
    ASSERT_EQ(tracker.totalCount(), 1);
    const auto event = tracker.getEvents().front();
    EXPECT_EQ(event.category, FallbackCategory::Font);
    EXPECT_EQ(event.action, FallbackAction::Fallback);
    EXPECT_EQ(event.originalId, QStringLiteral("missing-font"));
    EXPECT_EQ(event.resolvedId, QStringLiteral("Arial"));
}

TEST(FallbackPolicyStandaloneTest, DisabledPolicyReturnsResolverMissWithoutFallback)
{
    auto& tracker = resetTracker();
    auto policy = FallbackPolicy::defaultFont();
    policy.enabled = false;
    tracker.setPolicy(FallbackCategory::Font, policy);
    int fallbackCalls = 0;
    const auto result = tryFallbackPolicy<int>(FallbackCategory::Font,
        [] { return Optional<int>{}; },
        [&fallbackCalls] { ++fallbackCalls; return 42; },
        QStringLiteral("disabled"));
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(fallbackCalls, 0);
    ASSERT_EQ(tracker.totalCount(), 1);
    EXPECT_EQ(tracker.getEvents().front().action, FallbackAction::Strict);
}

TEST(FallbackPolicyStandaloneTest, DisabledPolicyStillReturnsResolvedResource)
{
    auto& tracker = resetTracker();
    auto policy = FallbackPolicy::defaultImage();
    policy.enabled = false;
    tracker.setPolicy(FallbackCategory::Image, policy);
    const auto result = tryFallbackPolicy<int>(FallbackCategory::Image,
        [] { return Optional<int>(8); }, [] { return 9; }, QStringLiteral("resolved"));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 8);
    EXPECT_EQ(tracker.totalCount(), 0);
}

TEST(FallbackPolicyStandaloneTest, BypassActionSkipsFallbackAndRecordsBypass)
{
    auto& tracker = resetTracker();
    int fallbackCalls = 0;
    const auto result = tryFallbackPolicy<int>(FallbackCategory::Effect,
        [] { return Optional<int>{}; },
        [&fallbackCalls] { ++fallbackCalls; return 1; }, QStringLiteral("effect"));
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(fallbackCalls, 0);
    const auto events = tracker.getEvents();
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].action, FallbackAction::Bypass);
    EXPECT_EQ(events[0].resolvedId, QStringLiteral("[bypass]"));
}

TEST(FallbackPolicyStandaloneTest, StrictActionSkipsFallbackAndRecordsStrictFailure)
{
    auto& tracker = resetTracker();
    auto policy = FallbackPolicy::defaultFont();
    policy.action = FallbackAction::Strict;
    tracker.setPolicy(FallbackCategory::Font, policy);
    int fallbackCalls = 0;
    const auto result = tryFallbackPolicy<int>(FallbackCategory::Font,
        [] { return Optional<int>{}; },
        [&fallbackCalls] { ++fallbackCalls; return 1; }, QStringLiteral("strict"));
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(fallbackCalls, 0);
    ASSERT_EQ(tracker.totalCount(), 1);
    EXPECT_EQ(tracker.getEvents().front().action, FallbackAction::Strict);
}

TEST(FallbackPolicyStandaloneTest, PolicyFallbackValueAndMessageAreRecorded)
{
    auto& tracker = resetTracker();
    auto policy = FallbackPolicy::defaultImage();
    policy.fallbackValue = QStringLiteral("transparent-pixel");
    policy.warningMessage = QStringLiteral("image unavailable");
    tracker.setPolicy(FallbackCategory::Image, policy);
    const auto result = tryFallbackPolicy<int>(FallbackCategory::Image,
        [] { return Optional<int>{}; }, [] { return 3; }, QStringLiteral("image-1"));
    ASSERT_TRUE(result.has_value());
    const auto event = tracker.getEvents().front();
    EXPECT_EQ(event.resolvedId, QStringLiteral("transparent-pixel"));
    EXPECT_EQ(event.message, QStringLiteral("image unavailable"));
}

TEST(FallbackPolicyStandaloneTest, LogWarningFalseStillRecordsNonWarningEvent)
{
    auto& tracker = resetTracker();
    auto policy = FallbackPolicy::defaultColor();
    policy.logWarning = false;
    tracker.setPolicy(FallbackCategory::Color, policy);
    const auto result = tryFallbackPolicy<int>(FallbackCategory::Color,
        [] { return Optional<int>{}; }, [] { return 5; }, QStringLiteral("token"));
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(tracker.totalCount(), 1);
    EXPECT_FALSE(tracker.getEvents().front().isWarning);
    EXPECT_FALSE(tracker.hasWarnings());
}

TEST(FallbackPolicyStandaloneTest, RecordOverloadSetsWarningFromAction)
{
    auto& tracker = resetTracker();
    tracker.record(FallbackCategory::Other, FallbackAction::Ignore,
                   QStringLiteral("ignored"), QStringLiteral(""));
    tracker.record(FallbackCategory::Other, FallbackAction::Warning,
                   QStringLiteral("warn"), QStringLiteral(""));
    EXPECT_FALSE(tracker.getEvents()[0].isWarning);
    EXPECT_TRUE(tracker.getEvents()[1].isWarning);
    EXPECT_TRUE(tracker.hasWarnings());
}

TEST(FallbackPolicyStandaloneTest, CategoryQueriesReturnOnlyMatchingEvents)
{
    auto& tracker = resetTracker();
    tracker.record({QDateTime::currentDateTime(), FallbackCategory::Font,
                    FallbackAction::Fallback, "font", "Arial", "font fallback", false});
    tracker.record({QDateTime::currentDateTime(), FallbackCategory::Image,
                    FallbackAction::Bypass, "image", "", "image bypass", false});
    EXPECT_EQ(tracker.totalCount(), 2);
    EXPECT_EQ(tracker.countByCategory(FallbackCategory::Font), 1);
    EXPECT_EQ(tracker.getEventsByCategory(FallbackCategory::Image).size(), 1);
    EXPECT_EQ(tracker.countByCategory(FallbackCategory::Other), 0);
}

TEST(FallbackPolicyStandaloneTest, SinceQueryUsesInclusiveTimestampBoundary)
{
    auto& tracker = resetTracker();
    const QDateTime boundary = QDateTime::fromMSecsSinceEpoch(1000, Qt::UTC);
    tracker.record({boundary, FallbackCategory::Font, FallbackAction::Fallback,
                    "equal", "", "equal", false});
    tracker.record({boundary.addMSecs(-1), FallbackCategory::Image, FallbackAction::Fallback,
                    "before", "", "before", false});
    const auto events = tracker.getEventsSince(boundary);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events.front().originalId, QStringLiteral("equal"));
}

TEST(FallbackPolicyStandaloneTest, ClearRemovesEventsWithoutChangingPolicies)
{
    auto& tracker = resetTracker();
    auto policy = FallbackPolicy::defaultFont();
    policy.fallbackValue = QStringLiteral("custom");
    tracker.setPolicy(FallbackCategory::Font, policy);
    tracker.record(FallbackCategory::Font, FallbackAction::Fallback,
                   QStringLiteral("a"), QStringLiteral("b"));
    tracker.clear();
    EXPECT_EQ(tracker.totalCount(), 0);
    EXPECT_EQ(tracker.policy(FallbackCategory::Font).fallbackValue, QStringLiteral("custom"));
}

TEST(FallbackPolicyStandaloneTest, UnknownCategoryUsesDefaultPolicyAndIgnoresSet)
{
    auto& tracker = resetTracker();
    FallbackPolicy custom;
    custom.fallbackValue = QStringLiteral("not-applied");
    tracker.setPolicy(FallbackCategory::Other, custom);
    EXPECT_EQ(tracker.policy(FallbackCategory::Other).fallbackValue, QString{});
    EXPECT_EQ(tracker.policy(FallbackCategory::Other).action, FallbackAction::Fallback);
}

TEST(FallbackPolicyStandaloneTest, WarningEnableFlagCanBeToggled)
{
    auto& tracker = resetTracker();
    EXPECT_FALSE(tracker.warningsEnabled());
    tracker.setWarningsEnabled(true);
    EXPECT_TRUE(tracker.warningsEnabled());
    tracker.setWarningsEnabled(false);
    EXPECT_FALSE(tracker.warningsEnabled());
}
