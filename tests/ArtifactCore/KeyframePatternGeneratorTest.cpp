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
