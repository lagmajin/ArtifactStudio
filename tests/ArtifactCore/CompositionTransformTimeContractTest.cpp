#include <gtest/gtest.h>

#include <limits>
#include <utility>

#include <QSize>
#include <QString>

import Composition.Registry;
import Composition.ParametricComposition;
import Container.NameMap;
import Transform;
import Layer.Blend;
import Layer.Matte;
import Time.Rational;
import Time.TimeRemap;
import Frame.Rate;

using namespace ArtifactCore;

namespace {

// NOTE: Layer2D is intentionally not covered here. Its header declares
// StaticTransform2D::toQTransform() / setTransform2D() without any definition
// in the codebase, and Layer2D::transformedLayer() calls toQTransform().
// Referencing any Layer2D symbol would pull that object file from the static
// library and fail the link (LNK2019). Those definitions must be added before
// a Layer2D ownership test can link.

class RegistryGuard {
public:
    RegistryGuard() { CompositionRegistry::global().clear(); }
    ~RegistryGuard() { CompositionRegistry::global().clear(); }
};

} // namespace

TEST(CompositionRegistryTest, RegisterFindAndReplace)
{
    RegistryGuard guard;
    CompositionRegistry& registry = CompositionRegistry::global();
    int first = 1;
    int second = 2;

    // Direct NameMap probe to isolate registry wiring from hashing.
    NameMap<QString, void*> probe{ContainerName{"RegistryProbe"}};
    probe[QStringLiteral("key")] = &first;
    EXPECT_TRUE(probe.contains(QStringLiteral("key")));
    EXPECT_EQ(probe[QStringLiteral("key")], &first);

    EXPECT_EQ(registry.findComposition(QStringLiteral("Comp")), nullptr);

    registry.registerComposition(QStringLiteral("Comp"), &first);
    EXPECT_EQ(registry.findComposition(QStringLiteral("Comp")), &first);

    // A later registration for the same name wins.
    registry.registerComposition(QStringLiteral("Comp"), &second);
    EXPECT_EQ(registry.findComposition(QStringLiteral("Comp")), &second);

    // A stale owner must not evict the newer entry.
    registry.unregisterComposition(QStringLiteral("Comp"), &first);
    EXPECT_EQ(registry.findComposition(QStringLiteral("Comp")), &second);

    registry.unregisterComposition(QStringLiteral("Comp"), &second);
    EXPECT_EQ(registry.findComposition(QStringLiteral("Comp")), nullptr);
}

TEST(CompositionRegistryTest, GuardsAndNameNormalization)
{
    RegistryGuard guard;
    CompositionRegistry& registry = CompositionRegistry::global();
    int composition = 3;

    registry.registerComposition(QString(), &composition);
    registry.registerComposition(QStringLiteral("   "), &composition);
    registry.registerComposition(QStringLiteral("Valid"), nullptr);
    EXPECT_TRUE(registry.registeredNames().isEmpty());

    registry.registerComposition(QStringLiteral("  Padded  "), &composition);
    EXPECT_EQ(registry.findComposition(QStringLiteral("Padded")), &composition);
    EXPECT_EQ(registry.findComposition(QStringLiteral("  Padded  ")), &composition);
    EXPECT_EQ(registry.findComposition(QString()), nullptr);
    ASSERT_EQ(registry.registeredNames().size(), 1);
    EXPECT_EQ(registry.registeredNames().front(), QStringLiteral("Padded"));

    // Unregistering an unknown name or a mismatched pointer is inert.
    registry.unregisterComposition(QStringLiteral("Missing"), &composition);
    registry.unregisterComposition(QStringLiteral("Padded"), nullptr);
    EXPECT_EQ(registry.findComposition(QStringLiteral("Padded")), &composition);

    registry.clear();
    EXPECT_TRUE(registry.registeredNames().isEmpty());
}

TEST(LayerBlendMatteContractTest, MatteModeStringsAndPredicates)
{
    EXPECT_EQ(MatteModeUtils::toString(MatteMode::None), QStringLiteral("None"));
    EXPECT_EQ(MatteModeUtils::fromString(QStringLiteral("alpha")), MatteMode::Alpha);
    EXPECT_EQ(MatteModeUtils::fromString(QStringLiteral("  ALPHA INVERTED ")),
              MatteMode::AlphaInverted);
    EXPECT_EQ(MatteModeUtils::fromString(QStringLiteral("luma matte")),
              MatteMode::Luminance);
    EXPECT_EQ(MatteModeUtils::fromString(QStringLiteral("nonsense")), MatteMode::None);

    // Display strings survive a parse round-trip.
    for (const MatteMode mode : {MatteMode::None, MatteMode::Alpha,
                                 MatteMode::AlphaInverted, MatteMode::Luminance,
                                 MatteMode::LuminanceInverted}) {
        EXPECT_EQ(MatteModeUtils::fromString(MatteModeUtils::toString(mode)), mode);
    }

    EXPECT_TRUE(MatteModeUtils::isInverted(MatteMode::AlphaInverted));
    EXPECT_TRUE(MatteModeUtils::isInverted(MatteMode::LuminanceInverted));
    EXPECT_FALSE(MatteModeUtils::isInverted(MatteMode::Alpha));
    EXPECT_TRUE(MatteModeUtils::isLuminance(MatteMode::Luminance));
    EXPECT_TRUE(MatteModeUtils::isLuminance(MatteMode::LuminanceInverted));
    EXPECT_FALSE(MatteModeUtils::isLuminance(MatteMode::Alpha));

    EXPECT_NE(BlendMode::Normal, BlendMode::Multiply);
}

TEST(StaticTransform2DTest, DefaultsAndSetters)
{
    StaticTransform2D transform;
    EXPECT_FLOAT_EQ(transform.x(), 0.0f);
    EXPECT_FLOAT_EQ(transform.y(), 0.0f);
    EXPECT_FLOAT_EQ(transform.scaleX(), 1.0f);
    EXPECT_FLOAT_EQ(transform.scaleY(), 1.0f);
    EXPECT_FLOAT_EQ(transform.anchorPointX(), 0.0f);
    EXPECT_FLOAT_EQ(transform.anchorPointY(), 0.0f);
    EXPECT_FLOAT_EQ(transform.rotation(), 0.0f);

    transform.setX(12.5f);
    transform.setY(-4.0f);
    transform.setScaleX(2.0f);
    transform.setScaleY(0.5f);
    transform.setAnchorPointX(8.0f);
    transform.setAnchorPointY(9.0f);
    transform.setInitialScale(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(transform.x(), 12.5f);
    EXPECT_FLOAT_EQ(transform.y(), -4.0f);
    EXPECT_FLOAT_EQ(transform.scaleX(), 2.0f);
    EXPECT_FLOAT_EQ(transform.scaleY(), 0.5f);
    EXPECT_FLOAT_EQ(transform.anchorPointX(), 8.0f);
    EXPECT_FLOAT_EQ(transform.anchorPointY(), 9.0f);
}

TEST(StaticTransform2DTest, CopyIsDeepAndMoveIsNullSafe)
{
    StaticTransform2D source;
    source.setX(5.0f);
    source.setScaleX(2.0f);

    StaticTransform2D copy(source);
    EXPECT_FLOAT_EQ(copy.x(), 5.0f);
    copy.setX(99.0f);
    EXPECT_FLOAT_EQ(source.x(), 5.0f);

    StaticTransform2D assigned;
    assigned = source;
    EXPECT_FLOAT_EQ(assigned.scaleX(), 2.0f);
    assigned.setScaleX(7.0f);
    EXPECT_FLOAT_EQ(source.scaleX(), 2.0f);

    StaticTransform2D moved(std::move(copy));
    EXPECT_FLOAT_EQ(moved.x(), 99.0f);

    // Moved-from instances hold nullptr and must still answer safely.
    EXPECT_FLOAT_EQ(copy.x(), 0.0f);
    EXPECT_FLOAT_EQ(copy.scaleX(), 1.0f);
    copy.setX(1.0f);
    EXPECT_FLOAT_EQ(copy.x(), 0.0f);
}

TEST(RationalTimeTest, ConstructionAndConversion)
{
    RationalTime zero;
    EXPECT_EQ(zero.value(), 0);
    EXPECT_EQ(zero.scale(), 1);
    EXPECT_DOUBLE_EQ(zero.toSeconds(), 0.0);

    RationalTime half(1, 2);
    EXPECT_DOUBLE_EQ(half.toSeconds(), 0.5);
    EXPECT_DOUBLE_EQ(half.toDouble(), 0.5);
    EXPECT_EQ(half.rescaledTo(1000), 500);
    EXPECT_EQ(half.toFrameCount(30), 15);

    // Non-positive scales are clamped to 1; non-positive fps yields 0 frames.
    RationalTime clamped(5, -3);
    EXPECT_EQ(clamped.scale(), 1);
    EXPECT_EQ(half.toFrameCount(0), 0);
    EXPECT_EQ(half.toFrameCount(-24), 0);

    // Rounding is half away from zero.
    EXPECT_EQ(RationalTime(1, 3).rescaledTo(1000), 333);
    EXPECT_EQ(RationalTime(2, 3).rescaledTo(1000), 667);
    EXPECT_EQ(RationalTime(-1, 2).rescaledTo(1000), -500);

    EXPECT_EQ(RationalTime::fromFrameCount(48, 24).toFrameCount(24), 48);
    EXPECT_EQ(RationalTime::fromFrameCount(10, 0).scale(), 1);
    const RationalTime fromSeconds = RationalTime::fromSeconds(1.5);
    EXPECT_NEAR(fromSeconds.toSeconds(), 1.5, 1e-6);
    EXPECT_EQ(fromSeconds.toFrameCount(30), 45);
}

TEST(RationalTimeTest, ArithmeticAndComparisonAcrossScales)
{
    EXPECT_TRUE(RationalTime(1, 2) == RationalTime(2, 4));
    EXPECT_TRUE(RationalTime(1, 2) != RationalTime(1, 3));
    EXPECT_TRUE(RationalTime(1, 3) < RationalTime(1, 2));
    EXPECT_TRUE(RationalTime(1, 2) > RationalTime(1, 3));
    EXPECT_TRUE(RationalTime(1, 2) <= RationalTime(2, 4));
    EXPECT_TRUE(RationalTime(1, 2) >= RationalTime(2, 4));
    EXPECT_TRUE(RationalTime(-1, 2) < RationalTime(1, 3));

    const RationalTime sameScale = RationalTime(1, 4) + RationalTime(2, 4);
    EXPECT_EQ(sameScale.value(), 3);
    EXPECT_EQ(sameScale.scale(), 4);

    const RationalTime crossScale = RationalTime(1, 2) + RationalTime(1, 3);
    EXPECT_NEAR(crossScale.toSeconds(), 5.0 / 6.0, 1e-12);

    const RationalTime difference = RationalTime(1, 2) - RationalTime(1, 3);
    EXPECT_NEAR(difference.toSeconds(), 1.0 / 6.0, 1e-12);

    RationalTime assigned;
    assigned = RationalTime(7, 24);
    EXPECT_EQ(assigned.value(), 7);
    EXPECT_EQ(assigned.scale(), 24);
    const RationalTime copied(assigned);
    EXPECT_TRUE(copied == assigned);
}

TEST(TimeRemapProcessorTest, EmptyMappingIsIdentity)
{
    TimeRemapProcessor processor;
    EXPECT_DOUBLE_EQ(processor.mapOutputToSource(4.0), 4.0);
    EXPECT_DOUBLE_EQ(processor.mapSourceToOutput(4.0), 4.0);
    EXPECT_NEAR(processor.getSpeedAtTime(4.0), 1.0, 1e-9);
    EXPECT_EQ(processor.sourceDuration(), 10.0);
    EXPECT_EQ(processor.sourceFrameCount(), 300);
    EXPECT_EQ(processor.frameBlendMode(), FrameBlendMode::None);
    EXPECT_FLOAT_EQ(processor.frameBlendAmount(), 0.5f);
}

TEST(TimeRemapProcessorTest, KeyframeOwnershipRules)
{
    TimeRemapProcessor processor;
    TimeRemapKeyframe first;
    first.outputTime = 5.0;
    first.sourceTime = 1.0;
    TimeRemapKeyframe second;
    second.outputTime = 2.0;
    second.sourceTime = 7.0;
    processor.addKeyframe(first);
    processor.addKeyframe(second);

    // Keyframes are kept sorted by output time.
    ASSERT_EQ(processor.keyframes().size(), 2);
    EXPECT_DOUBLE_EQ(processor.keyframes().front().outputTime, 2.0);

    // Re-adding the same output time replaces instead of duplicating.
    TimeRemapKeyframe replacement;
    replacement.outputTime = 2.0;
    replacement.sourceTime = 9.0;
    processor.addKeyframe(replacement);
    ASSERT_EQ(processor.keyframes().size(), 2);
    EXPECT_DOUBLE_EQ(processor.mapOutputToSource(2.0), 9.0);

    // Non-finite keyframes are rejected.
    TimeRemapKeyframe bad;
    bad.outputTime = std::numeric_limits<double>::quiet_NaN();
    bad.sourceTime = 1.0;
    processor.addKeyframe(bad);
    EXPECT_EQ(processor.keyframes().size(), 2);

    // Out-of-range removal is inert.
    processor.removeKeyframe(-1);
    processor.removeKeyframe(99);
    EXPECT_EQ(processor.keyframes().size(), 2);
    processor.removeKeyframe(0);
    EXPECT_EQ(processor.keyframes().size(), 1);

    // setKeyframes rebuilds through the same ownership rules.
    processor.setKeyframes({first, first});
    EXPECT_EQ(processor.keyframes().size(), 1);

    processor.clearKeyframes();
    EXPECT_TRUE(processor.keyframes().isEmpty());
    EXPECT_DOUBLE_EQ(processor.mapOutputToSource(3.0), 3.0);
}

TEST(TimeRemapProcessorTest, PresetsMapAndInvert)
{
    const TimeRemapProcessor halfSpeed =
        TimeRemapProcessor::createConstantSpeed(2.0);
    EXPECT_DOUBLE_EQ(halfSpeed.mapOutputToSource(5.0), 2.5);
    EXPECT_NEAR(halfSpeed.getSpeedAtTime(5.0), 0.5, 1e-6);
    EXPECT_NEAR(halfSpeed.mapSourceToOutput(2.5), 5.0, 1e-4);
    EXPECT_EQ(halfSpeed.getSourceFrameIndex(5.0), 75);

    const TimeRemapProcessor reversed = TimeRemapProcessor::createReverse();
    EXPECT_DOUBLE_EQ(reversed.mapOutputToSource(0.0), 10.0);
    EXPECT_DOUBLE_EQ(reversed.mapOutputToSource(10.0), 0.0);
    EXPECT_DOUBLE_EQ(reversed.mapOutputToSource(5.0), 5.0);
    EXPECT_NEAR(reversed.getSpeedAtTime(5.0), -1.0, 1e-6);

    const TimeRemapProcessor hold = TimeRemapProcessor::createHoldAt(2.0, 3.0);
    EXPECT_DOUBLE_EQ(hold.mapOutputToSource(2.0), 2.0);
    EXPECT_DOUBLE_EQ(hold.mapOutputToSource(5.0), 2.0);
    EXPECT_NEAR(hold.getSpeedAtTime(3.0), 0.0, 1e-6);

    // Frame indices are clamped into the available range.
    TimeRemapProcessor wide;
    TimeRemapKeyframe wideStart;
    wideStart.outputTime = 0.0;
    wideStart.sourceTime = 0.0;
    TimeRemapKeyframe wideEnd;
    wideEnd.outputTime = 10.0;
    wideEnd.sourceTime = 20.0;
    wide.addKeyframe(wideStart);
    wide.addKeyframe(wideEnd);
    EXPECT_EQ(wide.getSourceFrameIndex(1000.0), 299);
    EXPECT_EQ(wide.getSourceFrameIndex(-1000.0), 0);
    EXPECT_EQ(halfSpeed.getSourceFrameIndex(-1000.0), 0);
}

TEST(TimeRemapProcessorTest, BlendClampFrameConvertAndReset)
{
    TimeRemapProcessor processor;
    processor.setFrameBlendMode(FrameBlendMode::FrameMix);
    EXPECT_EQ(processor.frameBlendMode(), FrameBlendMode::FrameMix);
    processor.setFrameBlendAmount(99.0f);
    EXPECT_FLOAT_EQ(processor.frameBlendAmount(), 1.0f);
    processor.setFrameBlendAmount(-1.0f);
    EXPECT_FLOAT_EQ(processor.frameBlendAmount(), 0.0f);

    processor.setFrameRate(FrameRate(30.0f));
    TimeRemapKeyframe keyframe;
    keyframe.outputTime = 1.0;
    keyframe.sourceTime = 2.0;
    processor.addKeyframe(keyframe);
    processor.convertTimesToFrames();
    ASSERT_EQ(processor.keyframes().size(), 1);
    EXPECT_DOUBLE_EQ(processor.keyframes().front().outputTime, 30.0);
    EXPECT_DOUBLE_EQ(processor.keyframes().front().sourceTime, 60.0);
    processor.convertFramesToTimes();
    EXPECT_NEAR(processor.keyframes().front().outputTime, 1.0, 1e-9);
    EXPECT_NEAR(processor.keyframes().front().sourceTime, 2.0, 1e-9);

    processor.setSourceDuration(20.0);
    processor.setSourceFrameCount(600);
    processor.reset();
    EXPECT_DOUBLE_EQ(processor.sourceDuration(), 10.0);
    EXPECT_EQ(processor.sourceFrameCount(), 300);
    EXPECT_TRUE(processor.keyframes().isEmpty());
    EXPECT_DOUBLE_EQ(processor.mapOutputToSource(4.0), 4.0);

    ParametricCompositionRenderContext context;
    context.timeSeconds = 1.5;
    EXPECT_EQ(context.timeKey(), 1500000);
}
