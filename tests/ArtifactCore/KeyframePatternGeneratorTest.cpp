#include <gtest/gtest.h>

#include <QPointF>
#include <QString>
#include <QVector>

#include <cmath>
#include <limits>

import Animation.KeyframePatternGenerator;
import Math.Interpolate;
import Time.Rational;

using namespace ArtifactCore;

TEST(KeyframePatternGeneratorTest, EveryPresetProducesFiniteOrderedKeysAtRequestedScale)
{
    const KeyframePatternPreset presets[] = {
        KeyframePatternPreset::Stagger,
        KeyframePatternPreset::Pulse,
        KeyframePatternPreset::Bounce,
        KeyframePatternPreset::Shake,
        KeyframePatternPreset::Loop,
        KeyframePatternPreset::Ramp,
        KeyframePatternPreset::Wave,
        KeyframePatternPreset::Step,
        KeyframePatternPreset::RandomHold,
        KeyframePatternPreset::Overshoot,
        KeyframePatternPreset::Settle,
        KeyframePatternPreset::BeatSync,
    };

    for (const auto preset : presets) {
        KeyframePatternRequest request;
        request.preset = preset;
        request.baseValue = 2.0;
        request.targetValue = 8.0;
        request.startFrame = 10.0;
        request.endFrame = 20.0;
        request.frameScale = 24;
        request.sampleCount = 8;
        request.stepCount = 4;
        request.seed = 71;

        const auto result = KeyframePatternGenerator::generate(request);

        ASSERT_FALSE(result.keyframes.isEmpty())
            << KeyframePatternGenerator::presetLabel(preset).toStdString();
        double previousFrame = -std::numeric_limits<double>::infinity();
        for (const auto& keyframe : result.keyframes) {
            bool numeric = false;
            const double value = keyframe.value.toDouble(&numeric);
            ASSERT_TRUE(numeric)
                << KeyframePatternGenerator::presetLabel(preset).toStdString();
            EXPECT_TRUE(std::isfinite(value));
            EXPECT_EQ(keyframe.time.scale(), 24);
            const double frame = static_cast<double>(keyframe.time.value());
            EXPECT_GE(frame, previousFrame)
                << KeyframePatternGenerator::presetLabel(preset).toStdString();
            previousFrame = frame;
        }
    }
}

TEST(KeyframePatternGeneratorTest, RampKeepsExactEndpointsAndNumericType)
{
    KeyframePatternRequest request;
    request.preset = KeyframePatternPreset::Ramp;
    request.baseValue = 2.0;
    request.targetValue = 8.0;
    request.startFrame = 10.0;
    request.endFrame = 20.0;
    request.frameScale = 30;

    const auto result = KeyframePatternGenerator::generate(request);

    ASSERT_EQ(result.keyframes.size(), 2);
    EXPECT_EQ(result.keyframes[0].time.value(), 10);
    EXPECT_EQ(result.keyframes[0].time.scale(), 30);
    EXPECT_DOUBLE_EQ(result.keyframes[0].value.toDouble(), 2.0);
    EXPECT_EQ(result.keyframes[1].time.value(), 20);
    EXPECT_EQ(result.keyframes[1].time.scale(), 30);
    EXPECT_DOUBLE_EQ(result.keyframes[1].value.toDouble(), 8.0);
}

TEST(KeyframePatternGeneratorTest, StaggerOffsetsKeysBySelectionIndexAndDelay)
{
    KeyframePatternRequest request;
    request.preset = KeyframePatternPreset::Stagger;
    request.baseValue = 0.0;
    request.targetValue = 1.0;
    request.startFrame = 10.0;
    request.endFrame = 20.0;
    request.delayFrames = 3.0;
    request.selectionIndex = 2;

    const auto result = KeyframePatternGenerator::generate(request);

    ASSERT_EQ(result.keyframes.size(), 2);
    EXPECT_EQ(result.keyframes[0].time.value(), 16);
    EXPECT_EQ(result.keyframes[1].time.value(), 26);
}

TEST(KeyframePatternGeneratorTest, StepUsesConstantInterpolationAndIncludesBothEndpoints)
{
    KeyframePatternRequest request;
    request.preset = KeyframePatternPreset::Step;
    request.baseValue = 0.0;
    request.targetValue = 8.0;
    request.startFrame = 3.0;
    request.endFrame = 11.0;
    request.stepCount = 4;

    const auto result = KeyframePatternGenerator::generate(request);

    ASSERT_EQ(result.keyframes.size(), 5);
    EXPECT_EQ(result.keyframes.front().time.value(), 3);
    EXPECT_DOUBLE_EQ(result.keyframes.front().value.toDouble(), 0.0);
    EXPECT_EQ(result.keyframes.back().time.value(), 11);
    EXPECT_DOUBLE_EQ(result.keyframes.back().value.toDouble(), 8.0);
    for (const auto& keyframe : result.keyframes) {
        EXPECT_EQ(keyframe.interpolation, InterpolationType::Constant);
    }
}

TEST(KeyframePatternGeneratorTest,
     BeatSyncKeepsRoundedKeysOrderedInsidePartialFinalBeat)
{
    KeyframePatternRequest request;
    request.preset = KeyframePatternPreset::BeatSync;
    request.baseValue = 0.0;
    request.targetValue = 1.0;
    request.startFrame = 10.0;
    request.endFrame = 35.0;
    request.bpm = 120.0;
    request.frameScale = 24;

    const auto result = KeyframePatternGenerator::generate(request);

    ASSERT_FALSE(result.keyframes.isEmpty());
    double previousFrame = request.startFrame;
    for (const auto& keyframe : result.keyframes) {
        const double frame = static_cast<double>(keyframe.time.value());
        EXPECT_GE(frame, request.startFrame);
        EXPECT_LE(frame, request.endFrame);
        EXPECT_GE(frame, previousFrame);
        previousFrame = frame;
    }
    EXPECT_DOUBLE_EQ(static_cast<double>(result.keyframes.back().time.value()),
                     request.endFrame);
}

TEST(KeyframePatternGeneratorTest, ShakeUsesSeedForRepeatableBoundedSamples)
{
    KeyframePatternRequest request;
    request.preset = KeyframePatternPreset::Shake;
    request.baseValue = 5.0;
    request.startFrame = 0.0;
    request.endFrame = 6.0;
    request.amplitude = 2.0;
    request.sampleCount = 7;
    request.seed = 1234;

    const auto first = KeyframePatternGenerator::generate(request);
    const auto second = KeyframePatternGenerator::generate(request);

    ASSERT_EQ(first.keyframes.size(), 7);
    ASSERT_EQ(second.keyframes.size(), first.keyframes.size());
    for (qsizetype index = 0; index < first.keyframes.size(); ++index) {
        const double value = first.keyframes[index].value.toDouble();
        EXPECT_DOUBLE_EQ(value, second.keyframes[index].value.toDouble());
        EXPECT_GE(value, 3.0);
        EXPECT_LE(value, 7.0);
    }
}

TEST(KeyframePatternGeneratorTest, TrajectoryFiltersNonFiniteSamplesAndResamplesUniformly)
{
    const QVector<QPointF> path = {
        QPointF(0.0, 0.0),
        QPointF(10.0, 20.0),
        QPointF(std::numeric_limits<double>::quiet_NaN(), 99.0),
        QPointF(20.0, 40.0),
    };

    const auto result = KeyframePatternGenerator::generateFromTrajectory(
        path, 5, 5, 30);

    ASSERT_EQ(result.size(), 5);
    const QPointF expected[] = {
        QPointF(0.0, 0.0), QPointF(5.0, 10.0), QPointF(10.0, 20.0),
        QPointF(15.0, 30.0), QPointF(20.0, 40.0),
    };
    for (qsizetype index = 0; index < result.size(); ++index) {
        EXPECT_EQ(result[index].time.value(), 5 + index);
        EXPECT_EQ(result[index].time.scale(), 30);
        EXPECT_DOUBLE_EQ(result[index].value.x(), expected[index].x());
        EXPECT_DOUBLE_EQ(result[index].value.y(), expected[index].y());
    }
}

TEST(KeyframePatternGeneratorTest, BeatGridDetectsTempoFromSyntheticEnergyEnvelope)
{
    // 100 Hz envelope, one pulse every 0.5 s -> 120 BPM -> 15 frames at 30 fps.
    QVector<double> envelope(400, 0.0);
    for (int i = 0; i < envelope.size(); i += 50) {
        envelope[i] = 1.0;
    }

    BeatGridDetector::Config config;
    config.minBpm = 60.0;
    config.maxBpm = 180.0;
    config.fps = 30.0;
    config.frameScale = 24;

    const BeatGrid grid = BeatGridDetector::detectFromEnergyEnvelope(envelope, 100.0, config);

    ASSERT_TRUE(grid.valid());
    EXPECT_NEAR(grid.bpm, 120.0, 2.0);
    EXPECT_DOUBLE_EQ(grid.fps, 30.0);
    EXPECT_EQ(grid.frameScale, 24);
    EXPECT_DOUBLE_EQ(grid.framesPerBeat(), 15.0);
    EXPECT_GT(grid.confidence, 0.0);
    EXPECT_DOUBLE_EQ(grid.beatFrames.front(), 0.0);
    for (qsizetype i = 1; i < grid.beatFrames.size(); ++i) {
        EXPECT_NEAR(grid.beatFrames[i] - grid.beatFrames[i - 1], 15.0, 0.5);
    }
}

TEST(KeyframePatternGeneratorTest, BeatGridQuantizesToNearestBeatAndHandlesEmptyGrid)
{
    BeatGrid grid;
    EXPECT_FALSE(grid.valid());
    EXPECT_DOUBLE_EQ(grid.quantizeFrame(37.0), 37.0);

    grid.bpm = 120.0;
    grid.fps = 30.0;
    grid.beatFrames = { 0.0, 15.0, 30.0, 45.0 };
    EXPECT_DOUBLE_EQ(grid.nearestBeatFrame(7.0), 0.0);
    EXPECT_DOUBLE_EQ(grid.nearestBeatFrame(23.0), 30.0);
    EXPECT_DOUBLE_EQ(grid.quantizeFrame(28.0), 30.0);
}

TEST(KeyframePatternGeneratorTest, MotionRecipeResolvesAllKindsToFiniteOrderedKeys)
{
    const MotionRecipeKind kinds[] = {
        MotionRecipeKind::PopIn, MotionRecipeKind::SnapBack,
        MotionRecipeKind::StaggerWave, MotionRecipeKind::BounceIn,
        MotionRecipeKind::Wiggle, MotionRecipeKind::SpringFollow,
        MotionRecipeKind::BeatPulse,
    };

    for (const MotionRecipeKind kind : kinds) {
        MotionRecipe recipe;
        recipe.kind = kind;
        recipe.durationFrames = 18.0;
        recipe.selectionIndex = 2;
        recipe.seed = 91;

        const auto result = MotionRecipeService::apply(
            recipe, 0.0, 10.0, 6.0, 30.0, 24, nullptr);

        ASSERT_FALSE(result.keyframes.keyframes.isEmpty());
        double previous = -std::numeric_limits<double>::infinity();
        for (const auto& keyframe : result.keyframes.keyframes) {
            const double frame = static_cast<double>(keyframe.time.value());
            EXPECT_GE(frame, previous);
            previous = frame;
            EXPECT_TRUE(std::isfinite(keyframe.value.toDouble()));
            EXPECT_EQ(keyframe.time.scale(), 24);
        }
    }
}

TEST(KeyframePatternGeneratorTest, MotionRecipeAlignToBeatSnapsStartAndUsesGridBpm)
{
    BeatGrid grid;
    grid.bpm = 120.0;
    grid.fps = 30.0;
    grid.beatFrames = { 0.0, 15.0, 30.0, 45.0 };

    MotionRecipe recipe;
    recipe.kind = MotionRecipeKind::BeatPulse;
    recipe.durationFrames = 20.0;
    recipe.alignToBeat = true;

    const auto result = MotionRecipeService::apply(
        recipe, 0.0, 1.0, 23.0, 30.0, 30, &grid);

    ASSERT_FALSE(result.keyframes.keyframes.isEmpty());
    EXPECT_DOUBLE_EQ(static_cast<double>(result.keyframes.keyframes.front().time.value()), 30.0);
    // 20 frames rounds to two 15-frame beats.
    EXPECT_DOUBLE_EQ(static_cast<double>(result.keyframes.keyframes.back().time.value()), 60.0);
    EXPECT_TRUE(result.warning.isEmpty());
}
