#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <QJsonObject>

import Animation.Value;

using namespace ArtifactCore;

namespace {

AutomationClipPattern twoPointPattern() {
    AutomationClipPattern pattern;
    pattern.id = 1;
    pattern.name = "test";
    AutomationClipPoint a;
    a.time = 0.0;
    a.value = 0.0f;
    AutomationClipPoint b;
    b.time = 2.0;
    b.value = 10.0f;
    pattern.points = {a, b};
    return pattern;
}

AutomationClipInstance instanceFor(std::uint32_t patternId) {
    AutomationClipInstance instance;
    instance.patternId = patternId;
    instance.targetPath = "transform.position.x";
    return instance;
}

}  // namespace

TEST(AutomationClipTest, EvaluatedPathAllowlistIsExact) {
    EXPECT_TRUE(isAutomationClipEvaluatedPath("transform.position.x"));
    EXPECT_TRUE(isAutomationClipEvaluatedPath("transform.position.y"));
    EXPECT_TRUE(isAutomationClipEvaluatedPath("transform.rotation"));
    EXPECT_TRUE(isAutomationClipEvaluatedPath("transform.scale.x"));
    EXPECT_TRUE(isAutomationClipEvaluatedPath("transform.scale.y"));
    EXPECT_TRUE(isAutomationClipEvaluatedPath("layer.opacity"));

    EXPECT_FALSE(isAutomationClipEvaluatedPath("transform.position"));
    EXPECT_FALSE(isAutomationClipEvaluatedPath("transform.position.z"));
    EXPECT_FALSE(isAutomationClipEvaluatedPath("transform.scale"));
    EXPECT_FALSE(isAutomationClipEvaluatedPath("layer.opacity.extra"));
    EXPECT_FALSE(isAutomationClipEvaluatedPath("effects.blur"));
}

TEST(AutomationClipTest, LinearEvaluationClampsAtEnds) {
    const auto pattern = twoPointPattern();
    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(pattern, -1.0), 0.0f);
    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(pattern, 0.0), 0.0f);
    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(pattern, 1.0), 5.0f);
    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(pattern, 2.0), 10.0f);
    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(pattern, 99.0), 10.0f);
}

TEST(AutomationClipTest, SinglePointIsConstant) {
    AutomationClipPattern pattern;
    pattern.id = 2;
    AutomationClipPoint only;
    only.time = 1.0;
    only.value = 7.0f;
    pattern.points = {only};
    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(pattern, 0.0), 7.0f);
    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(pattern, 500.0), 7.0f);
    EXPECT_FLOAT_EQ(pattern.duration(), 1.0);
}

TEST(AutomationClipTest, NonFiniteLocalTimeFallsBackToTheStart)
{
    auto pattern = twoPointPattern();
    pattern.points.front().value = 3.0f;

    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(
                        pattern, std::numeric_limits<double>::quiet_NaN()),
                    3.0f);
    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(
                        pattern, std::numeric_limits<double>::infinity()),
                    3.0f);
    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(
                        pattern, -std::numeric_limits<double>::infinity()),
                    3.0f);
}

TEST(AutomationClipTest, ConstantInterpolationHoldsUntilTheNextPoint)
{
    auto pattern = twoPointPattern();
    pattern.points.front().interpolation = InterpolationType::Constant;

    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(pattern, 0.0), 0.0f);
    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(pattern, 1.999), 0.0f);
    EXPECT_FLOAT_EQ(evaluateAutomationClipPattern(pattern, 2.0), 10.0f);
}

TEST(AutomationClipTest, BezierInterpolationUsesPointControlValues)
{
    auto pattern = twoPointPattern();
    auto& first = pattern.points.front();
    first.interpolation = InterpolationType::Bezier;
    first.cp1_y = 0.0f;
    first.cp2_y = 0.0f;

    // Symmetric default x controls map the midpoint to t=0.5; the two zero
    // y controls therefore produce the cubic Bezier value 1/8 of the range.
    EXPECT_NEAR(evaluateAutomationClipPattern(pattern, 1.0), 1.25f, 1e-4f);
}

TEST(AutomationClipTest, LoopAndPingPongWrap) {
    EXPECT_DOUBLE_EQ(mapAutomationClipLoop(5.0, 2.0, AutomationClipLoopMode::Loop), 1.0);
    EXPECT_DOUBLE_EQ(mapAutomationClipLoop(-1.0, 2.0, AutomationClipLoopMode::Loop), 1.0);
    EXPECT_DOUBLE_EQ(mapAutomationClipLoop(2.0, 2.0, AutomationClipLoopMode::Loop), 0.0);
    EXPECT_DOUBLE_EQ(
        mapAutomationClipLoop(5.0, 2.0, AutomationClipLoopMode::Off), 2.0);
    EXPECT_DOUBLE_EQ(
        mapAutomationClipLoop(3.0, 2.0, AutomationClipLoopMode::PingPong), 1.0);
    EXPECT_DOUBLE_EQ(
        mapAutomationClipLoop(4.0, 2.0, AutomationClipLoopMode::PingPong), 0.0);

    const auto pattern = twoPointPattern();
    AutomationClipInstance looped = instanceFor(1);
    looped.loop = AutomationClipLoopMode::Loop;
    // t=5s wraps to 1s -> value 5.
    EXPECT_FLOAT_EQ(applyAutomationClipInstance(0.0f, pattern, looped, 5.0), 5.0f);
}

TEST(AutomationClipTest, LoopMappingHandlesNonPositiveDurationAndNonFiniteTime)
{
    const AutomationClipLoopMode modes[] = {
        AutomationClipLoopMode::Off,
        AutomationClipLoopMode::Loop,
        AutomationClipLoopMode::PingPong};
    for (const auto mode : modes) {
        EXPECT_DOUBLE_EQ(mapAutomationClipLoop(4.0, 0.0, mode), 0.0);
        EXPECT_DOUBLE_EQ(mapAutomationClipLoop(4.0, -2.0, mode), 0.0);
        EXPECT_DOUBLE_EQ(mapAutomationClipLoop(
                            std::numeric_limits<double>::infinity(), 2.0, mode),
                        0.0);
        EXPECT_DOUBLE_EQ(mapAutomationClipLoop(
                            std::numeric_limits<double>::quiet_NaN(), 2.0, mode),
                        0.0);
    }
}

TEST(AutomationClipTest, WeightMixingIsNonDestructive) {
    const auto pattern = twoPointPattern();
    auto instance = instanceFor(1);
    // t=2s -> clip value 10.
    EXPECT_FLOAT_EQ(applyAutomationClipInstance(4.0f, pattern, instance, 2.0), 10.0f);

    instance.weight = 0.0f;
    EXPECT_FLOAT_EQ(applyAutomationClipInstance(4.0f, pattern, instance, 2.0), 4.0f);

    instance.weight = 0.5f;
    EXPECT_FLOAT_EQ(applyAutomationClipInstance(4.0f, pattern, instance, 2.0), 7.0f);

    instance.weight = 1.0f;
    instance.enabled = false;
    EXPECT_FLOAT_EQ(applyAutomationClipInstance(4.0f, pattern, instance, 2.0), 4.0f);

    instance.enabled = true;
    instance.patternId = 999;
    EXPECT_FLOAT_EQ(applyAutomationClipInstance(4.0f, pattern, instance, 2.0), 4.0f);
}

TEST(AutomationClipTest, WeightClampsAndNonFiniteWeightUsesFullEffect)
{
    const auto pattern = twoPointPattern();
    auto instance = instanceFor(1);

    instance.weight = -0.5f;
    EXPECT_FLOAT_EQ(applyAutomationClipInstance(4.0f, pattern, instance, 2.0),
                    4.0f);
    instance.weight = 1.5f;
    EXPECT_FLOAT_EQ(applyAutomationClipInstance(4.0f, pattern, instance, 2.0),
                    10.0f);
    instance.weight = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FLOAT_EQ(applyAutomationClipInstance(4.0f, pattern, instance, 2.0),
                    10.0f);
}

TEST(AutomationClipTest, OffsetStretchAndDeterminism) {
    const auto pattern = twoPointPattern();
    auto instance = instanceFor(1);
    instance.offsetSeconds = 10.0;
    instance.stretch = 2.0;
    // (14 - 10) / 2 = 2s -> clip value 10.
    EXPECT_FLOAT_EQ(applyAutomationClipInstance(0.0f, pattern, instance, 14.0), 10.0f);
    EXPECT_FLOAT_EQ(
        applyAutomationClipInstance(0.0f, pattern, instance, 14.0),
        applyAutomationClipInstance(0.0f, pattern, instance, 14.0));

    AutomationClipInstance degenerate = instanceFor(1);
    degenerate.stretch = 0.0;  // sanitized to 1.0
    EXPECT_FLOAT_EQ(
        applyAutomationClipInstance(0.0f, pattern, degenerate, 1.0), 5.0f);
}

TEST(AutomationClipTest, SanitizeSortsAndCaps) {
    AutomationClipPattern pattern;
    pattern.id = 3;
    AutomationClipPoint late;
    late.time = 5.0;
    late.value = 1.0f;
    AutomationClipPoint early;
    early.time = 1.0;
    early.value = 2.0f;
    pattern.points = {late, early};
    EXPECT_TRUE(sanitizeAutomationClipPattern(pattern));
    EXPECT_DOUBLE_EQ(pattern.points.front().time, 1.0);
    EXPECT_DOUBLE_EQ(pattern.duration(), 5.0);

    AutomationClipPattern empty;
    empty.id = 4;
    EXPECT_FALSE(sanitizeAutomationClipPattern(empty));
}

TEST(AutomationClipTest, SanitizerCapsPointCountAndReplacesNonFiniteFields)
{
    AutomationClipPattern pattern;
    pattern.points.reserve(kMaxAutomationClipPoints + 1u);
    for (std::size_t index = 0; index <= kMaxAutomationClipPoints; ++index) {
        AutomationClipPoint point;
        point.time = static_cast<double>(index + 1u);
        point.value = static_cast<float>(index);
        pattern.points.push_back(point);
    }
    auto& malformed = pattern.points.front();
    malformed.time = std::numeric_limits<double>::quiet_NaN();
    malformed.value = std::numeric_limits<float>::infinity();
    malformed.curvature = std::numeric_limits<float>::quiet_NaN();
    malformed.cp1_x = std::numeric_limits<float>::infinity();
    malformed.cp1_y = std::numeric_limits<float>::quiet_NaN();
    malformed.cp2_x = -std::numeric_limits<float>::infinity();
    malformed.cp2_y = std::numeric_limits<float>::quiet_NaN();

    ASSERT_TRUE(sanitizeAutomationClipPattern(pattern));

    ASSERT_EQ(pattern.points.size(), kMaxAutomationClipPoints);
    EXPECT_DOUBLE_EQ(pattern.points.front().time, 0.0);
    EXPECT_FLOAT_EQ(pattern.points.front().value, 0.0f);
    EXPECT_FLOAT_EQ(pattern.points.front().curvature, 0.0f);
    EXPECT_FLOAT_EQ(pattern.points.front().cp1_x, 0.42f);
    EXPECT_FLOAT_EQ(pattern.points.front().cp1_y, 0.0f);
    EXPECT_FLOAT_EQ(pattern.points.front().cp2_x, 0.58f);
    EXPECT_FLOAT_EQ(pattern.points.front().cp2_y, 1.0f);
}

TEST(AutomationClipTest, JsonRoundTrip) {
    auto pattern = twoPointPattern();
    pattern.seed = 123u;
    const QJsonObject patternJson = automationClipPatternToJson(pattern);
    AutomationClipPattern restored;
    ASSERT_TRUE(automationClipPatternFromJson(patternJson, restored));
    EXPECT_EQ(restored.id, 1u);
    EXPECT_EQ(restored.name, "test");
    EXPECT_EQ(restored.seed, 123u);
    ASSERT_EQ(restored.points.size(), 2u);
    EXPECT_DOUBLE_EQ(restored.points[1].time, 2.0);
    EXPECT_FLOAT_EQ(restored.points[1].value, 10.0f);

    auto instance = instanceFor(1);
    instance.offsetSeconds = 0.5;
    instance.loop = AutomationClipLoopMode::PingPong;
    instance.timePolicy = AutomationClipTimePolicy::FreeTime;
    const QJsonObject instanceJson = automationClipInstanceToJson(instance);
    AutomationClipInstance restoredInstance;
    ASSERT_TRUE(automationClipInstanceFromJson(instanceJson, restoredInstance));
    EXPECT_TRUE(automationClipInstancesEqual({instance}, {restoredInstance}));

    AutomationClipPattern rejected;
    EXPECT_FALSE(automationClipPatternFromJson(QJsonObject{}, rejected));
    AutomationClipInstance rejectedInstance;
    EXPECT_FALSE(automationClipInstanceFromJson(QJsonObject{}, rejectedInstance));
}

TEST(AutomationClipTest, InstanceJsonClampsEnumsAndTrimsTargetPath) {
    QJsonObject object;
    object[QStringLiteral("patternId")] = 7;
    object[QStringLiteral("targetPath")] = QStringLiteral("  layer.opacity  ");
    object[QStringLiteral("loop")] = 99;
    object[QStringLiteral("timePolicy")] = -1;

    AutomationClipInstance instance;
    ASSERT_TRUE(automationClipInstanceFromJson(object, instance));
    EXPECT_EQ(instance.patternId, 7u);
    EXPECT_EQ(instance.targetPath, "layer.opacity");
    EXPECT_EQ(instance.loop, AutomationClipLoopMode::PingPong);
    EXPECT_EQ(instance.timePolicy, AutomationClipTimePolicy::ParentFollow);
}
