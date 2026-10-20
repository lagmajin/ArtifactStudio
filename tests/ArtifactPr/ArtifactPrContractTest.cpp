// ArtifactPr contract test: the pure CPU parts of the application.
//
// Everything here is deterministic and needs no media file, no GPU device and
// no window: clip effect math, the CPU compositor, timecode formatting, the
// shortcut registry and export format routing.
#include <gtest/gtest.h>

#include <QJsonObject>
#include <QMap>
#include <QPoint>
#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include <cmath>
#include <initializer_list>
#include <utility>
#include <vector>

#include "ArtifactPrTestApp.h"

import ArtifactPr.ClipEffects;
import ArtifactPr.EditorEngine;
import ArtifactPr.SequenceCompositor;
import ArtifactPr.SequenceExporter;
import ArtifactPr.Shortcut;
import ArtifactPr.TimecodeOverlayWidget;
import FloatRGBA;
import Image.ImageF32x4_RGBA;

namespace {

[[maybe_unused]] const bool artifactPrApplicationRegistered =
    ArtifactPrTest::registerApplicationEnvironment();

constexpr double kPi = 3.14159265358979323846;

using EffectsMap = QMap<QString, QVariant>;

EffectsMap effects(std::initializer_list<std::pair<const char*, const char*>> pairs)
{
    EffectsMap map;
    for (const auto& pair : pairs) {
        map.insert(QString::fromLatin1(pair.first), QString::fromLatin1(pair.second));
    }
    return map;
}

ArtifactPr::TransitionFadeSpan span(ArtifactPr::FramePosition start,
                                    ArtifactPr::FramePosition duration,
                                    bool isLeft,
                                    ArtifactPr::TransitionFadeCurve curve)
{
    ArtifactPr::TransitionFadeSpan value;
    value.startFrame = start;
    value.duration = duration;
    value.isLeft = isLeft;
    value.curve = curve;
    return value;
}

ArtifactCore::ImageF32x4_RGBA solidImage(int width, int height, const ArtifactCore::FloatRGBA& color)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.resize(width, height);
    image.fill(color);
    return image;
}

double rms(const std::vector<float>& samples, int from)
{
    double sum = 0.0;
    for (int i = from; i < static_cast<int>(samples.size()); ++i) {
        sum += static_cast<double>(samples[i]) * static_cast<double>(samples[i]);
    }
    return std::sqrt(sum / static_cast<double>(samples.size() - from));
}

std::vector<float> sine(float frequency, double sampleRate, int frames)
{
    std::vector<float> samples(frames);
    for (int i = 0; i < frames; ++i) {
        samples[i] = static_cast<float>(
            std::sin(2.0 * kPi * frequency * static_cast<double>(i) / sampleRate));
    }
    return samples;
}

} // namespace

// =====================================================================
// Transition opacity modulation
// =====================================================================

TEST(ArtifactPrClipEffectsTest, EmptyTransitionSetLeavesOpacityUnmodulated)
{
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor({}, 0), 1.0);
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor({}, 12345), 1.0);
}

TEST(ArtifactPrClipEffectsTest, LinearFadeRampsAcrossTheHalfOpenSpan)
{
    const QVector<ArtifactPr::TransitionFadeSpan> left{
        span(100, 10, true, ArtifactPr::TransitionFadeCurve::Linear)};

    // Outside the half-open [100, 110) interval the factor is unchanged.
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(left, 99), 1.0);
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(left, 110), 1.0);
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(left, 200), 1.0);

    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(left, 100), 0.0);
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(left, 105), 0.5);
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(left, 109), 0.9);

    const QVector<ArtifactPr::TransitionFadeSpan> right{
        span(100, 10, false, ArtifactPr::TransitionFadeCurve::Linear)};
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(right, 100), 1.0);
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(right, 105), 0.5);
    EXPECT_NEAR(ArtifactPr::transitionOpacityFactor(right, 109), 0.1, 1e-12);
}

TEST(ArtifactPrClipEffectsTest, DipToBlackReachesZeroAtTheSpanMidpoint)
{
    const QVector<ArtifactPr::TransitionFadeSpan> left{
        span(0, 10, true, ArtifactPr::TransitionFadeCurve::DipToBlack)};
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(left, 0), 1.0);
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(left, 5), 0.0);
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(left, 9), 0.0);

    const QVector<ArtifactPr::TransitionFadeSpan> right{
        span(0, 10, false, ArtifactPr::TransitionFadeCurve::DipToBlack)};
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(right, 0), 0.0);
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(right, 5), 0.0);
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(right, 9), 1.0);
}

TEST(ArtifactPrClipEffectsTest, OverlappingSpansKeepTheStrongestAttenuation)
{
    const QVector<ArtifactPr::TransitionFadeSpan> spans{
        span(0, 10, true, ArtifactPr::TransitionFadeCurve::Linear),   // 0.5 at frame 5
        span(4, 10, true, ArtifactPr::TransitionFadeCurve::Linear)};  // 0.1 at frame 5

    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(spans, 5), 0.1);
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(spans, 0), 0.0);
    // frame 13 is only inside the second span: (13-4)/10 = 0.9
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(spans, 13), 0.9);
}

TEST(ArtifactPrClipEffectsTest, ZeroLengthSpanIsIgnored)
{
    const QVector<ArtifactPr::TransitionFadeSpan> spans{
        span(50, 0, true, ArtifactPr::TransitionFadeCurve::Linear)};
    EXPECT_DOUBLE_EQ(ArtifactPr::transitionOpacityFactor(spans, 50), 1.0);
}

// =====================================================================
// Effect parameter reading
// =====================================================================

TEST(ArtifactPrClipEffectsTest, EffectEnabledAcceptsOnlyOneAndTrue)
{
    EXPECT_TRUE(ArtifactPr::effectEnabled(effects({{"fx.glow.enabled", "1"}}), "glow"));
    EXPECT_TRUE(ArtifactPr::effectEnabled(effects({{"fx.glow.enabled", "true"}}), "glow"));
    EXPECT_TRUE(ArtifactPr::effectEnabled(effects({{"fx.glow.enabled", "TRUE"}}), "glow"));
    EXPECT_FALSE(ArtifactPr::effectEnabled(effects({{"fx.glow.enabled", "0"}}), "glow"));
    EXPECT_FALSE(ArtifactPr::effectEnabled(effects({{"fx.glow.enabled", "yes"}}), "glow"));
    EXPECT_FALSE(ArtifactPr::effectEnabled(effects({{"fx.other.enabled", "1"}}), "glow"));
    EXPECT_FALSE(ArtifactPr::effectEnabled({}, "glow"));
}

TEST(ArtifactPrClipEffectsTest, EffectParamFallsBackOnMissingOrUnparseableValue)
{
    EXPECT_DOUBLE_EQ(
        ArtifactPr::effectParam(effects({{"fx.glow.intensity", "2.5"}}), "glow", "intensity", -1.0),
        2.5);
    EXPECT_DOUBLE_EQ(
        ArtifactPr::effectParam(effects({{"fx.glow.intensity", "abc"}}), "glow", "intensity", -1.0),
        -1.0);
    EXPECT_DOUBLE_EQ(ArtifactPr::effectParam({}, "glow", "intensity", 7.0), 7.0);
}

TEST(ArtifactPrClipEffectsTest, ImageEffectFastPathOnlySeesImageEffectIds)
{
    EXPECT_FALSE(ArtifactPr::hasEnabledImageEffects({}));
    EXPECT_FALSE(ArtifactPr::hasEnabledImageEffects(effects({{"fx.audioGain.enabled", "1"}})));
    EXPECT_FALSE(ArtifactPr::hasEnabledImageEffects(effects({{"fx.glow.enabled", "0"}})));
    EXPECT_TRUE(ArtifactPr::hasEnabledImageEffects(effects({{"fx.brightnessContrast.enabled", "1"}})));
    EXPECT_TRUE(ArtifactPr::hasEnabledImageEffects(effects({{"fx.rotate.enabled", "true"}})));
}

// =====================================================================
// Audio effect parameters
// =====================================================================

TEST(ArtifactPrClipEffectsTest, AudioGainFactorConvertsDecibelsToLinear)
{
    EXPECT_DOUBLE_EQ(ArtifactPr::audioGainFactor({}), 1.0);
    // A parameter set without the enabled flag stays at unity gain.
    EXPECT_DOUBLE_EQ(
        ArtifactPr::audioGainFactor(effects({{"fx.audioGain.gainDb", "12"}})), 1.0);

    // 20 * log10(2) = 6.020599913279624 dB
    const auto doubled =
        effects({{"fx.audioGain.enabled", "1"}, {"fx.audioGain.gainDb", "6.020599913279624"}});
    EXPECT_NEAR(ArtifactPr::audioGainFactor(doubled), 2.0, 1e-9);

    const auto muted = effects({{"fx.audioGain.enabled", "1"}, {"fx.audioGain.gainDb", "-100"}});
    EXPECT_NEAR(ArtifactPr::audioGainFactor(muted), 0.0, 1e-4);
}

TEST(ArtifactPrClipEffectsTest, AudioEqualizerDbIsZeroedWhenTheEffectIsDisabled)
{
    double low = 9.0;
    double mid = 9.0;
    double high = 9.0;
    ArtifactPr::audioEqualizerDb(effects({{"fx.audioEqualizer.lowDb", "12"}}), low, mid, high);
    EXPECT_DOUBLE_EQ(low, 0.0);
    EXPECT_DOUBLE_EQ(mid, 0.0);
    EXPECT_DOUBLE_EQ(high, 0.0);

    const auto enabled = effects({{"fx.audioEqualizer.enabled", "1"},
                                  {"fx.audioEqualizer.lowDb", "3"},
                                  {"fx.audioEqualizer.midDb", "-4"},
                                  {"fx.audioEqualizer.highDb", "5"}});
    ArtifactPr::audioEqualizerDb(enabled, low, mid, high);
    EXPECT_DOUBLE_EQ(low, 3.0);
    EXPECT_DOUBLE_EQ(mid, -4.0);
    EXPECT_DOUBLE_EQ(high, 5.0);
}

TEST(ArtifactPrClipEffectsTest, AudioEqualizerActiveUsesAHundredthDecibelThreshold)
{
    EXPECT_FALSE(ArtifactPr::audioEqualizerActive(0.0, 0.0, 0.0));
    EXPECT_FALSE(ArtifactPr::audioEqualizerActive(0.01, 0.0, 0.0));
    EXPECT_TRUE(ArtifactPr::audioEqualizerActive(0.02, 0.0, 0.0));
    EXPECT_TRUE(ArtifactPr::audioEqualizerActive(0.0, -0.011, 0.0));
}

TEST(ArtifactPrClipEffectsTest, EqualizerAtUnityGainDoesNotTouchTheSignal)
{
    ArtifactPr::ClipEqualizer equalizer;
    equalizer.lowDb = 0.0;
    equalizer.midDb = 0.0;
    equalizer.highDb = 0.0;
    equalizer.configure();
    EXPECT_FALSE(equalizer.active);

    std::vector<float> samples = sine(1000.0f, 48000.0, 256);
    const std::vector<float> original = samples;
    equalizer.process(samples.data(), static_cast<int>(samples.size()));
    EXPECT_EQ(samples, original);
}

TEST(ArtifactPrClipEffectsTest, LowShelfBoostRaisesLowFrequencyEnergy)
{
    constexpr int kFrames = 48000;
    constexpr double kSampleRate = 48000.0;

    auto samples = sine(100.0f, kSampleRate, kFrames);
    const double before = rms(samples, kFrames / 2);

    ArtifactPr::ClipEqualizer equalizer;
    equalizer.sampleRate = kSampleRate;
    equalizer.lowDb = 12.0;
    equalizer.midDb = 0.0;
    equalizer.highDb = 0.0;
    equalizer.configure();
    ASSERT_TRUE(equalizer.active);
    equalizer.process(samples.data(), kFrames);

    // +12 dB low shelf at 100 Hz is roughly a 4x amplitude gain.
    EXPECT_GT(rms(samples, kFrames / 2), before * 2.0);
}

TEST(ArtifactPrClipEffectsTest, MidPeakingBoostRaisesOneKilohertzEnergy)
{
    constexpr int kFrames = 48000;
    constexpr double kSampleRate = 48000.0;

    auto samples = sine(1000.0f, kSampleRate, kFrames);
    const double before = rms(samples, kFrames / 2);

    ArtifactPr::ClipEqualizer equalizer;
    equalizer.sampleRate = kSampleRate;
    equalizer.lowDb = 0.0;
    equalizer.midDb = 12.0;
    equalizer.highDb = 0.0;
    equalizer.configure();
    ASSERT_TRUE(equalizer.active);
    equalizer.process(samples.data(), kFrames);

    EXPECT_GT(rms(samples, kFrames / 2), before * 2.0);
}

// =====================================================================
// CPU sequence compositor
// =====================================================================

TEST(ArtifactPrSequenceCompositorTest, CanvasTakesTheRequestedSizeAndBackground)
{
    const QSize canvasSize(64, 48);
    const ArtifactCore::FloatRGBA background(0.25f, 0.5f, 0.75f, 1.0f);
    const auto canvas = ArtifactPr::composeSequenceLayers(canvasSize, {}, background);

    ASSERT_EQ(canvas.width(), 64);
    ASSERT_EQ(canvas.height(), 48);
    for (const QPoint& probe : {QPoint(0, 0), QPoint(31, 23), QPoint(63, 47)}) {
        const ArtifactCore::FloatRGBA pixel = canvas.getPixel(probe.x(), probe.y());
        EXPECT_NEAR(pixel.r(), 0.25f, 1e-5f);
        EXPECT_NEAR(pixel.g(), 0.5f, 1e-5f);
        EXPECT_NEAR(pixel.b(), 0.75f, 1e-5f);
    }
}

TEST(ArtifactPrSequenceCompositorTest, LayerIsAspectFitCenteredOverTheBackground)
{
    const QSize canvasSize(64, 64);
    const ArtifactCore::FloatRGBA background(0.0f, 0.0f, 0.0f, 1.0f);

    ArtifactPr::CompositeLayer layer;
    layer.frame = solidImage(32, 16, ArtifactCore::FloatRGBA(1.0f, 0.0f, 0.0f, 1.0f));
    layer.opacity = 1.0;

    const auto canvas = ArtifactPr::composeSequenceLayers(canvasSize, {layer}, background);

    // 32x16 fits to 64x32 and is centred vertically, so rows 0..15 stay
    // background while the middle band is covered.
    const ArtifactCore::FloatRGBA outside = canvas.getPixel(32, 4);
    EXPECT_NEAR(outside.r(), 0.0f, 1e-5f);
    EXPECT_NEAR(outside.g(), 0.0f, 1e-5f);

    const ArtifactCore::FloatRGBA inside = canvas.getPixel(32, 32);
    EXPECT_GT(inside.r(), 0.9f);
    EXPECT_LT(inside.g(), 0.1f);
    EXPECT_LT(inside.b(), 0.1f);
}

TEST(ArtifactPrSequenceCompositorTest, LayerOpacityScalesTheBlendWeight)
{
    const QSize canvasSize(16, 16);
    const ArtifactCore::FloatRGBA background(0.0f, 0.0f, 0.0f, 1.0f);

    ArtifactPr::CompositeLayer layer;
    layer.frame = solidImage(16, 16, ArtifactCore::FloatRGBA(1.0f, 0.0f, 0.0f, 1.0f));
    layer.opacity = 0.5;

    const auto canvas = ArtifactPr::composeSequenceLayers(canvasSize, {layer}, background);
    const ArtifactCore::FloatRGBA pixel = canvas.getPixel(8, 8);
    EXPECT_NEAR(pixel.r(), 0.5f, 0.05f);
    EXPECT_LT(pixel.g(), 0.05f);
}

TEST(ArtifactPrSequenceCompositorTest, TopLayerWinsAndZeroOpacityLayerIsSkipped)
{
    const QSize canvasSize(16, 16);
    const ArtifactCore::FloatRGBA background(0.0f, 0.0f, 0.0f, 1.0f);

    ArtifactPr::CompositeLayer bottom;
    bottom.frame = solidImage(16, 16, ArtifactCore::FloatRGBA(1.0f, 0.0f, 0.0f, 1.0f));
    bottom.opacity = 1.0;

    ArtifactPr::CompositeLayer invisible;
    invisible.frame = solidImage(16, 16, ArtifactCore::FloatRGBA(0.0f, 1.0f, 0.0f, 1.0f));
    invisible.opacity = 0.0;

    ArtifactPr::CompositeLayer top;
    top.frame = solidImage(16, 16, ArtifactCore::FloatRGBA(0.0f, 0.0f, 1.0f, 1.0f));
    top.opacity = 1.0;

    const auto canvas = ArtifactPr::composeSequenceLayers(
        canvasSize, {bottom, invisible, top}, background);

    const ArtifactCore::FloatRGBA pixel = canvas.getPixel(8, 8);
    EXPECT_LT(pixel.r(), 0.05f);
    EXPECT_LT(pixel.g(), 0.05f);
    EXPECT_GT(pixel.b(), 0.9f);
}

TEST(ArtifactPrSequenceCompositorTest, EmptyLayerFrameIsIgnored)
{
    const QSize canvasSize(8, 8);
    const ArtifactCore::FloatRGBA background(0.1f, 0.2f, 0.3f, 1.0f);

    ArtifactPr::CompositeLayer empty;
    empty.opacity = 1.0;

    const auto canvas = ArtifactPr::composeSequenceLayers(canvasSize, {empty}, background);
    const ArtifactCore::FloatRGBA pixel = canvas.getPixel(4, 4);
    EXPECT_NEAR(pixel.r(), 0.1f, 1e-5f);
    EXPECT_NEAR(pixel.g(), 0.2f, 1e-5f);
    EXPECT_NEAR(pixel.b(), 0.3f, 1e-5f);
}

// =====================================================================
// Export format routing and render plan validity
// =====================================================================

TEST(ArtifactPrSequenceExporterTest, ExportFormatClassificationIsExclusive)
{
    struct Row {
        ArtifactPr::ExportFormat::Value value;
        bool video;
        bool sequence;
        bool audio;
    };
    const Row rows[] = {
        {ArtifactPr::ExportFormat::Value::H264Mp4, true, false, false},
        {ArtifactPr::ExportFormat::Value::HevcMp4, true, false, false},
        {ArtifactPr::ExportFormat::Value::ProResMov, true, false, false},
        {ArtifactPr::ExportFormat::Value::DnxhdMov, true, false, false},
        {ArtifactPr::ExportFormat::Value::PngSequence, false, true, false},
        {ArtifactPr::ExportFormat::Value::JpegSequence, false, true, false},
        {ArtifactPr::ExportFormat::Value::WavAudio, false, false, true},
        {ArtifactPr::ExportFormat::Value::Mp3Audio, false, false, true},
    };

    for (const Row& row : rows) {
        ArtifactPr::ExportFormat format;
        format.value = row.value;
        EXPECT_EQ(format.isVideoFile(), row.video);
        EXPECT_EQ(format.isImageSequence(), row.sequence);
        EXPECT_EQ(format.isAudioOnly(), row.audio);
    }
}

TEST(ArtifactPrSequenceExporterTest, RenderPlanRequiresSnapshotAndOrderedRange)
{
    ArtifactPr::RenderPlan plan;
    EXPECT_FALSE(plan.isValid());

    plan.nleSnapshot = QJsonObject{{QStringLiteral("sequences"), QJsonObject{}}};
    plan.startFrame = 10;
    plan.endFrame = 5;
    EXPECT_FALSE(plan.isValid());

    plan.endFrame = 10;
    EXPECT_TRUE(plan.isValid());
}

// =====================================================================
// Timecode formatting
// =====================================================================

TEST(ArtifactPrTimecodeTest, FrameToTimecodeUsesZeroPaddedFields)
{
    EXPECT_EQ(TimecodeOverlayWidget::frameToTimecode(0, 30), QStringLiteral("00:00:00:00"));
    EXPECT_EQ(TimecodeOverlayWidget::frameToTimecode(1, 30), QStringLiteral("00:00:00:01"));
    EXPECT_EQ(TimecodeOverlayWidget::frameToTimecode(29, 30), QStringLiteral("00:00:00:29"));
    EXPECT_EQ(TimecodeOverlayWidget::frameToTimecode(30, 30), QStringLiteral("00:00:01:00"));
    EXPECT_EQ(TimecodeOverlayWidget::frameToTimecode(90, 30), QStringLiteral("00:00:03:00"));
    EXPECT_EQ(TimecodeOverlayWidget::frameToTimecode(30 * 3661, 30), QStringLiteral("01:01:01:00"));
}

TEST(ArtifactPrTimecodeTest, NonPositiveFrameRateFallsBackToThirty)
{
    EXPECT_EQ(TimecodeOverlayWidget::frameToTimecode(10, 0), QStringLiteral("00:00:00:10"));
    EXPECT_EQ(TimecodeOverlayWidget::frameToTimecode(10, -5), QStringLiteral("00:00:00:10"));
}

// =====================================================================
// Shortcut registry
// =====================================================================

TEST(ArtifactPrShortcutRegistryTest, RegistryExposesUniqueNamedShortcuts)
{
    ArtifactPr::PrShortcutRegistry registry;
    const QList<ArtifactPr::PrShortcut>& all = registry.all();
    const int countBeforeAdd = all.size();
    ASSERT_GT(countBeforeAdd, 0);

    QSet<QString> names;
    for (const ArtifactPr::PrShortcut& shortcut : all) {
        EXPECT_FALSE(shortcut.name.isEmpty());
        EXPECT_FALSE(shortcut.keys.isEmpty()) << shortcut.name.toStdString();
        EXPECT_FALSE(shortcut.description.isEmpty()) << shortcut.name.toStdString();
        EXPECT_FALSE(shortcut.category.isEmpty()) << shortcut.name.toStdString();
        EXPECT_FALSE(names.contains(shortcut.name)) << shortcut.name.toStdString();
        names.insert(shortcut.name);
    }

    EXPECT_TRUE(registry.has(all.first().name));
    EXPECT_FALSE(registry.has(QStringLiteral("no-such-action")));
}

TEST(ArtifactPrShortcutRegistryTest, CategoriesCoverEveryRegisteredShortcut)
{
    ArtifactPr::PrShortcutRegistry registry;

    const QStringList categories = registry.categories();
    ASSERT_FALSE(categories.isEmpty());

    QSet<QString> seenCategories;
    for (const QString& category : categories) {
        EXPECT_FALSE(seenCategories.contains(category));
        seenCategories.insert(category);
    }

    int categorised = 0;
    for (const QString& category : categories) {
        const QList<ArtifactPr::PrShortcut> entries = registry.byCategory(category);
        EXPECT_FALSE(entries.isEmpty());
        categorised += static_cast<int>(entries.size());
    }
    EXPECT_EQ(categorised, static_cast<int>(registry.all().size()));

    EXPECT_TRUE(registry.helpText().contains(QStringLiteral("Playback")));
}

TEST(ArtifactPrShortcutRegistryTest, AddRegistersANewNameAndKeepsExistingEntries)
{
    ArtifactPr::PrShortcutRegistry registry;
    const int countBeforeAdd = static_cast<int>(registry.all().size());

    registry.add(ArtifactPr::PrShortcut(QStringLiteral("contractAction"),
                                        QStringLiteral("Ctrl+Alt+T"),
                                        QStringLiteral("Contract test action"),
                                        QStringLiteral("Zoom")));

    EXPECT_EQ(static_cast<int>(registry.all().size()), countBeforeAdd + 1);
    EXPECT_TRUE(registry.has(QStringLiteral("contractAction")));
    EXPECT_EQ(static_cast<int>(registry.byCategory(QStringLiteral("Zoom")).size()), 4);
}
