// ArtifactPr-style contract test for the FxStudio FX animation render path.
//
// FxStudio already samples cue animations (Sequence -> CueSample,
// EventTrack -> EventSample) but nothing turned them into pixels, so the only
// way to inspect an FX animation was the dialog's text list. This suite renders
// the animation to images instead: every cue kind is checked as a pixel
// contract, every preset is rendered frame by frame, and the frames plus a
// contact sheet are written to disk so a failure can be looked at.
//
// Pure CPU: no GPU device, no window, no media file.
#include <gtest/gtest.h>

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "../ArtifactTestFxCueRenderer.h"

import Artifact.FxStudio.EventTrack;
import Artifact.FxStudio.PresetCatalog;
import Artifact.FxStudio.Sequence;
import Artifact.FxStudio.Session;
import Artifact.FxStudio.ViewportModel;
import Core.ArtifactString;

namespace {

namespace Fx = Artifact::FxStudio;
using ArtifactTestFxCueRenderer::Background;
using ArtifactTestFxCueRenderer::Comparison;
using ArtifactTestFxCueRenderer::Cue;
using ArtifactTestFxCueRenderer::Effect;
using ArtifactTestFxCueRenderer::Image;

constexpr int kWidth = 96;
constexpr int kHeight = 64;

// ---------------------------------------------------------------------------
// FxStudio module -> renderer adapters
// ---------------------------------------------------------------------------

Effect toEffect(const Fx::CueKind kind)
{
    switch (kind) {
    case Fx::CueKind::Distort: return Effect::Distort;
    case Fx::CueKind::ChannelSplit: return Effect::ChannelSplit;
    case Fx::CueKind::BlockDisplace: return Effect::BlockDisplace;
    case Fx::CueKind::Freeze: return Effect::Freeze;
    case Fx::CueKind::SignalDrop: return Effect::SignalDrop;
    case Fx::CueKind::Flash: return Effect::Flash;
    case Fx::CueKind::Shake: return Effect::Shake;
    case Fx::CueKind::MaskProgress: return Effect::MaskProgress;
    case Fx::CueKind::GlowPulse: return Effect::GlowPulse;
    case Fx::CueKind::Recovery: return Effect::Recovery;
    }
    return Effect::Distort;
}

std::vector<Cue> cuesAt(const Fx::EventTrack& track, const std::int64_t frame)
{
    std::vector<Cue> cues;
    for (const Fx::EventSample& sample : track.sample(frame)) {
        Cue cue;
        cue.effect = toEffect(sample.cue.kind);
        cue.amplitude = sample.cue.strength;
        cue.progress = sample.cue.progress;
        cue.variation = sample.cue.variation;
        cue.seed = sample.cue.seed;
        cues.push_back(cue);
    }
    return cues;
}

// A one-frame sequence holding a single cue at the requested amplitude, so a
// cue can be exercised without the preset curve getting in the way.
Fx::Sequence singleCueSequence(const Fx::CueKind kind, const float strength,
                              const float variation = 0.5f,
                              const std::uint32_t seed = 7)
{
    Fx::Sequence sequence;
    sequence.durationFrames = 1;
    sequence.strength = 1.0f;
    sequence.cues = {
        Fx::Cue{kind, Fx::CueCurve::Step, 0.0f, 1.0f, strength, variation, seed, true}};
    return sequence;
}

Fx::EventTrack trackWith(const Fx::Sequence& sequence, const std::int64_t startFrame = 0)
{
    Fx::EventTrack track;
    Fx::Event event;
    event.id = 1;
    event.startFrame = startFrame;
    event.sequence = sequence;
    event.enabled = true;
    EXPECT_TRUE(track.add(event));
    return track;
}

std::vector<Cue> singleCue(const Fx::CueKind kind, const float strength,
                           const float variation = 0.5f)
{
    const Fx::EventTrack track = trackWith(singleCueSequence(kind, strength, variation));
    return cuesAt(track, 0);
}

// ---------------------------------------------------------------------------
// image inspection helpers
// ---------------------------------------------------------------------------

int clampTo(const int value, const int limit)
{
    return ArtifactTestFxCueRenderer::clampIndex(value, limit);
}

float meanLuminanceRow(const Image& image, const int y)
{
    double sum = 0.0;
    for (int x = 0; x < image.width; ++x) {
        const float* px = image.at(x, y);
        sum += (px[0] + px[1] + px[2]) / 3.0;
    }
    return static_cast<float>(sum / static_cast<double>(std::max(1, image.width)));
}

bool pixelEquals(const float* a, const float* b, const float tolerance = 1.0e-6f)
{
    for (int c = 0; c < 4; ++c) {
        if (std::fabs(a[c] - b[c]) > tolerance) return false;
    }
    return true;
}

struct Translation {
    int dx = 0;
    int dy = 0;
    float ratio = 0.0f;
};

// Best integer translation explaining `out` from `source`, measured on the
// interior so clamped borders cannot skew the match.
Translation findTranslation(const Image& source, const Image& out, const int reach)
{
    Translation best;
    const int first = reach;
    const int lastX = out.width - reach;
    const int lastY = out.height - reach;
    const float total = static_cast<float>(std::max(1, lastX - first) *
                                           std::max(1, lastY - first));
    for (int dy = -reach; dy <= reach; ++dy) {
        for (int dx = -reach; dx <= reach; ++dx) {
            int matches = 0;
            for (int y = first; y < lastY; ++y) {
                for (int x = first; x < lastX; ++x) {
                    if (pixelEquals(out.at(x, y),
                                    source.at(clampTo(x - dx, source.width),
                                              clampTo(y - dy, source.height)))) {
                        ++matches;
                    }
                }
            }
            const float ratio = static_cast<float>(matches) / total;
            if (ratio > best.ratio) {
                best.dx = dx;
                best.dy = dy;
                best.ratio = ratio;
            }
        }
    }
    return best;
}

int bestRowShift(const Image& source, const Image& out, const int y, const int reach)
{
    int bestShift = 0;
    int bestMatches = -1;
    for (int shift = -reach; shift <= reach; ++shift) {
        int matches = 0;
        for (int x = reach; x < out.width - reach; ++x) {
            if (pixelEquals(out.at(x, y),
                            source.at(clampTo(x - shift, source.width), y))) {
                ++matches;
            }
        }
        if (matches > bestMatches) {
            bestMatches = matches;
            bestShift = shift;
        }
    }
    return bestShift;
}

std::vector<int> rowShifts(const Image& source, const Image& out, const int reach)
{
    std::vector<int> shifts;
    shifts.reserve(static_cast<std::size_t>(out.height));
    for (int y = 0; y < out.height; ++y) {
        shifts.push_back(bestRowShift(source, out, y, reach));
    }
    return shifts;
}

Image brightenedAlphaGradedImage()
{
    Image image = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            image.at(x, y)[3] = 0.25f;
        }
    }
    return image;
}

// ---------------------------------------------------------------------------
// PNG artifact output
// ---------------------------------------------------------------------------

QString captureDirectory()
{
    QString directory = qEnvironmentVariable("ARTIFACT_FX_STUDIO_CAPTURE_DIR");
    if (directory.isEmpty()) {
        directory = QDir::tempPath() + QStringLiteral("/artifact-fx-studio");
    }
    QDir().mkpath(directory);
    return directory;
}

QString sanitize(const std::string& value)
{
    QString result = QString::fromStdString(value);
    for (const QChar character : QStringLiteral(".:/\\ ")) {
        result.replace(character, QChar('_'));
    }
    return result;
}

bool writePng(const Image& image, const QString& path)
{
    const QImage qimage = ArtifactTestFxCueRenderer::toQImage(image);
    return !qimage.isNull() && qimage.save(path, "PNG");
}

} // namespace

// ===========================================================================
// diagnostic input
// ===========================================================================

TEST(FxStudioRenderContractTest, DiagnosticImageIsDeterministicAndInputsDiffer)
{
    const Image first = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);
    const Image again = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);
    const Image second = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight, true);

    ASSERT_FALSE(first.empty());
    EXPECT_EQ(first.width, kWidth);
    EXPECT_EQ(first.height, kHeight);
    EXPECT_EQ(first.pixels, again.pixels);
    EXPECT_TRUE(ArtifactTestFxCueRenderer::difference(first, second).any());
}

// ===========================================================================
// per-cue pixel contracts
// ===========================================================================

TEST(FxStudioRenderContractTest, EveryCueKindChangesPixelsExceptTemporalFreeze)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);
    const Image secondInput =
        ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight, true);
    const QString directory = captureDirectory();

    for (int index = 0; index < ArtifactTestFxCueRenderer::kEffectCount; ++index) {
        const auto effect = static_cast<Effect>(index);
        const std::string name =
            std::string("cue_") + ArtifactTestFxCueRenderer::effectName(effect);

        // MaskProgress needs a reveal target and Recovery needs something to
        // undo, so both are exercised against a prepared working image.
        const float amplitude = (effect == Effect::MaskProgress) ? 0.5f : 1.0f;
        const std::vector<Cue> cues =
            singleCue(static_cast<Fx::CueKind>(index), amplitude);
        ASSERT_EQ(cues.size(), 1u) << name;
        ASSERT_NEAR(cues.front().amplitude, amplitude, 1.0e-6f) << name;

        Image working = input;
        if (effect == Effect::Recovery) {
            std::vector<Cue> flash;
            Cue flashCue;
            flashCue.effect = Effect::Flash;
            flashCue.amplitude = 0.7f;
            flash.push_back(flashCue);
            ArtifactTestFxCueRenderer::applyCues(working, input, flash);
        }
        const Image baseline = working;

        ArtifactTestFxCueRenderer::ApplyContext context;
        if (effect == Effect::MaskProgress) context.revealTarget = &secondInput;

        std::vector<Effect> applied;
        ArtifactTestFxCueRenderer::applyCues(working, input, cues, &applied, context);

        const auto difference = ArtifactTestFxCueRenderer::difference(baseline, working);
        const QString base = directory + QStringLiteral("/") + sanitize(name);
        EXPECT_TRUE(writePng(baseline, base + QStringLiteral("_before.png")));
        EXPECT_TRUE(writePng(working, base + QStringLiteral("_after.png")));
        EXPECT_TRUE(writePng(difference.mask, base + QStringLiteral("_diff.png")));

        if (effect == Effect::Freeze) {
            // Freeze is temporal and is honoured by renderSequence, not by a
            // single-frame pixel pass.
            EXPECT_EQ(difference.changedPixels, 0) << name;
            EXPECT_TRUE(applied.empty()) << name;
        } else {
            EXPECT_TRUE(difference.any()) << name;
            ASSERT_EQ(applied.size(), 1u) << name;
            EXPECT_EQ(applied.front(), effect) << name;
        }
    }
}

TEST(FxStudioRenderContractTest, EveryCueKindMapsToADistinctRenderEffect)
{
    std::vector<Effect> mapped;
    for (int index = 0; index < ArtifactTestFxCueRenderer::kEffectCount; ++index) {
        const auto kind = static_cast<Fx::CueKind>(index);
        mapped.push_back(toEffect(kind));
        EXPECT_EQ(static_cast<int>(toEffect(kind)), index)
            << ArtifactTestFxCueRenderer::effectName(toEffect(kind));
    }
    std::sort(mapped.begin(), mapped.end(),
              [](const Effect a, const Effect b) {
                  return static_cast<int>(a) < static_cast<int>(b);
              });
    mapped.erase(std::unique(mapped.begin(), mapped.end()), mapped.end());
    EXPECT_EQ(mapped.size(),
              static_cast<std::size_t>(ArtifactTestFxCueRenderer::kEffectCount));
}

TEST(FxStudioRenderContractTest, ZeroAmplitudeCueLeavesTheImageUntouched)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);

    for (int index = 0; index < ArtifactTestFxCueRenderer::kEffectCount; ++index) {
        const std::vector<Cue> cues = singleCue(static_cast<Fx::CueKind>(index), 0.0f);
        ASSERT_EQ(cues.size(), 1u);
        EXPECT_FLOAT_EQ(cues.front().amplitude, 0.0f);

        Image working = input;
        std::vector<Effect> applied;
        ArtifactTestFxCueRenderer::applyCues(working, input, cues, &applied);

        EXPECT_EQ(working.pixels, input.pixels)
            << ArtifactTestFxCueRenderer::effectName(static_cast<Effect>(index));
        EXPECT_TRUE(applied.empty())
            << ArtifactTestFxCueRenderer::effectName(static_cast<Effect>(index));
    }
}

TEST(FxStudioRenderContractTest, FlashBrightensMonotonicallyWithStrength)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);

    double previous = -1.0;
    for (const float amplitude : {0.25f, 0.5f, 0.75f, 1.0f}) {
        std::vector<Cue> cues;
        Cue cue;
        cue.effect = Effect::Flash;
        cue.amplitude = amplitude;
        cues.push_back(cue);

        Image working = input;
        ArtifactTestFxCueRenderer::applyCues(working, input, cues);

        double sum = 0.0;
        for (int y = 0; y < working.height; ++y) {
            sum += meanLuminanceRow(working, y);
        }
        const double mean = sum / static_cast<double>(working.height);
        EXPECT_GT(mean, previous) << "amplitude " << amplitude;
        previous = mean;
    }
    // Full strength is a near-white flash.
    EXPECT_GT(previous, 0.9);
}

TEST(FxStudioRenderContractTest, ChannelSplitMovesRedAndBlueOppositely)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);
    const float amplitude = 0.5f;
    const int shift = std::max(1, static_cast<int>(std::lround(amplitude * 16.0f)));

    std::vector<Cue> cues;
    Cue cue;
    cue.effect = Effect::ChannelSplit;
    cue.amplitude = amplitude;
    cues.push_back(cue);

    Image working = input;
    ArtifactTestFxCueRenderer::applyCues(working, input, cues);

    for (int y = 0; y < working.height; ++y) {
        for (int x = 0; x < working.width; ++x) {
            const float* out = working.at(x, y);
            const float* in = input.at(x, y);
            EXPECT_FLOAT_EQ(out[0], input.at(clampTo(x - shift, input.width), y)[0]);
            EXPECT_FLOAT_EQ(out[1], in[1]);
            EXPECT_FLOAT_EQ(out[2], input.at(clampTo(x + shift, input.width), y)[2]);
            EXPECT_FLOAT_EQ(out[3], in[3]);
        }
    }
}

TEST(FxStudioRenderContractTest, ShakeTranslatesTheWholeImage)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);

    std::vector<Cue> cues;
    Cue cue;
    cue.effect = Effect::Shake;
    cue.amplitude = 1.0f;
    cue.variation = 0.5f;
    cue.seed = 11;
    cues.push_back(cue);

    Image working = input;
    ArtifactTestFxCueRenderer::applyCues(working, input, cues);

    const Translation translation = findTranslation(input, working, 16);
    EXPECT_GT(translation.ratio, 0.99f);
    EXPECT_TRUE(translation.dx != 0 || translation.dy != 0)
        << "translate " << translation.dx << "," << translation.dy;
}

TEST(FxStudioRenderContractTest, DistortShiftsRowsByDifferentAmounts)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);

    std::vector<Cue> cues;
    Cue cue;
    cue.effect = Effect::Distort;
    cue.amplitude = 1.0f;
    cue.variation = 0.6f;
    cue.seed = 23;
    cues.push_back(cue);

    Image working = input;
    ArtifactTestFxCueRenderer::applyCues(working, input, cues);

    EXPECT_EQ(working.width, input.width);
    EXPECT_EQ(working.height, input.height);
    EXPECT_TRUE(ArtifactTestFxCueRenderer::difference(input, working).any());

    const std::vector<int> shifts = rowShifts(input, working, 32);
    std::vector<int> distinct = shifts;
    std::sort(distinct.begin(), distinct.end());
    distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
    EXPECT_GE(distinct.size(), 2u) << "per-row displacement must vary";
}

TEST(FxStudioRenderContractTest, BlockDisplaceMovesWholeBands)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);

    std::vector<Cue> cues;
    Cue cue;
    cue.effect = Effect::BlockDisplace;
    cue.amplitude = 1.0f;
    cue.variation = 0.6f;
    cue.seed = 37;
    cues.push_back(cue);

    Image working = input;
    ArtifactTestFxCueRenderer::applyCues(working, input, cues);
    EXPECT_TRUE(ArtifactTestFxCueRenderer::difference(input, working).any());

    constexpr int kBlocks = 6;
    const int blockHeight = std::max(1, input.height / kBlocks);
    const std::vector<int> shifts = rowShifts(input, working, 32);

    bool anyShift = false;
    for (int block = 0; block < kBlocks; ++block) {
        const int first = block * blockHeight;
        const int last = std::min(input.height, first + blockHeight);
        for (int y = first + 1; y < last; ++y) {
            EXPECT_EQ(shifts[static_cast<std::size_t>(y)],
                      shifts[static_cast<std::size_t>(first)])
                << "band " << block << " row " << y;
        }
        if (shifts[static_cast<std::size_t>(first)] != 0) anyShift = true;
    }
    EXPECT_TRUE(anyShift);
}

TEST(FxStudioRenderContractTest, SignalDropDarkensStripsAndAddsATornLine)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);

    std::vector<Cue> cues;
    Cue cue;
    cue.effect = Effect::SignalDrop;
    cue.amplitude = 1.0f;
    cue.variation = 0.35f;
    cue.seed = 41;
    cues.push_back(cue);

    Image working = input;
    ArtifactTestFxCueRenderer::applyCues(working, input, cues);

    int darkRows = 0;
    int brightRows = 0;
    for (int y = 0; y < working.height; ++y) {
        const float mean = meanLuminanceRow(working, y);
        if (mean < 0.2f) ++darkRows;
        if (mean > 0.9f) ++brightRows;
    }
    EXPECT_GE(darkRows, 1);
    EXPECT_GE(brightRows, 1) << "the torn drop line must be present";
}

TEST(FxStudioRenderContractTest, MaskProgressRevealsLeftToRight)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);

    std::vector<Cue> cues;
    Cue glow;
    glow.effect = Effect::GlowPulse;
    glow.amplitude = 1.0f;
    cues.push_back(glow);
    Cue mask;
    mask.effect = Effect::MaskProgress;
    mask.amplitude = 0.5f;
    cues.push_back(mask);

    Image working = input;
    ArtifactTestFxCueRenderer::applyCues(working, input, cues);

    const int edge = kWidth / 2;
    // Revealed side (left) shows the processed frame, masked side the original.
    EXPECT_FALSE(pixelEquals(working.at(edge / 2, kHeight / 4),
                             input.at(edge / 2, kHeight / 4), 1.0e-3f));
    for (int y = 0; y < working.height; ++y) {
        for (int x = edge; x < working.width; ++x) {
            EXPECT_TRUE(pixelEquals(working.at(x, y), input.at(x, y)))
                << "masked pixel " << x << "," << y;
        }
    }
}

TEST(FxStudioRenderContractTest, RecoveryAtFullStrengthRestoresTheOriginal)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);

    std::vector<Cue> cues;
    Cue glow;
    glow.effect = Effect::GlowPulse;
    glow.amplitude = 1.0f;
    cues.push_back(glow);

    Image transformed = input;
    ArtifactTestFxCueRenderer::applyCues(transformed, input, cues);
    EXPECT_TRUE(ArtifactTestFxCueRenderer::difference(input, transformed).any());

    Cue recovery;
    recovery.effect = Effect::Recovery;
    recovery.amplitude = 1.0f;
    cues.push_back(recovery);

    Image recovered = input;
    ArtifactTestFxCueRenderer::applyCues(recovered, input, cues);
    EXPECT_EQ(recovered.pixels, input.pixels);
}

TEST(FxStudioRenderContractTest, GlowPulseLiftsMeanLuminance)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);

    std::vector<Cue> cues;
    Cue cue;
    cue.effect = Effect::GlowPulse;
    cue.amplitude = 1.0f;
    cues.push_back(cue);

    Image working = input;
    ArtifactTestFxCueRenderer::applyCues(working, input, cues);

    double before = 0.0;
    double after = 0.0;
    for (int y = 0; y < input.height; ++y) {
        before += meanLuminanceRow(input, y);
        after += meanLuminanceRow(working, y);
    }
    EXPECT_GT(after, before);
}

TEST(FxStudioRenderContractTest, CueEvaluationOrderIsIndependentOfInputOrder)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);

    std::vector<Cue> forward;
    for (const Effect effect : {Effect::BlockDisplace, Effect::Flash, Effect::Recovery}) {
        Cue cue;
        cue.effect = effect;
        cue.amplitude = 0.8f;
        cue.variation = 0.5f;
        cue.seed = 53;
        forward.push_back(cue);
    }
    std::vector<Cue> reversed(forward.rbegin(), forward.rend());

    Image first = input;
    Image second = input;
    ArtifactTestFxCueRenderer::applyCues(first, input, forward);
    ArtifactTestFxCueRenderer::applyCues(second, input, reversed);

    EXPECT_EQ(first.pixels, second.pixels);
}

// ===========================================================================
// temporal Freeze
// ===========================================================================

TEST(FxStudioRenderContractTest, FreezeHoldsThePreviouslyEmittedFrame)
{
    Fx::Sequence sequence;
    sequence.durationFrames = 5;
    sequence.strength = 1.0f;
    sequence.cues = {
        Fx::Cue{Fx::CueKind::Flash, Fx::CueCurve::Linear, 0.0f, 1.0f, 1.0f, 0.0f, 61, true},
        Fx::Cue{Fx::CueKind::Freeze, Fx::CueCurve::Step, 0.6f, 0.4f, 1.0f, 0.0f, 67, true},
    };
    const Fx::EventTrack track = trackWith(sequence);

    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);
    const auto rendered = ArtifactTestFxCueRenderer::renderSequence(
        input, 0, 5, [&track](const std::int64_t frame) { return cuesAt(track, frame); });

    ASSERT_EQ(rendered.frames.size(), 6u);
    ASSERT_EQ(rendered.report.frames.size(), 6u);
    // CueCurve::Step flips at localProgress == 1.0, which is float sensitive, so
    // the exact count is derived from the report instead of hard-coded.
    EXPECT_GE(rendered.report.heldFrames, 2);
    EXPECT_FALSE(rendered.report.frames.front().held);

    int heldCount = 0;
    Image lastEmitted;
    bool hasEmitted = false;
    for (std::size_t index = 0; index < rendered.frames.size(); ++index) {
        if (rendered.report.frames[index].held) {
            ++heldCount;
            ASSERT_TRUE(hasEmitted)
                << "frame " << index << " was held before any frame was emitted";
            EXPECT_EQ(rendered.frames[index].pixels, lastEmitted.pixels)
                << "held frame " << index;
        } else {
            lastEmitted = rendered.frames[index];
            hasEmitted = true;
        }
    }
    EXPECT_EQ(heldCount, rendered.report.heldFrames);
}

// ===========================================================================
// presets end to end
// ===========================================================================

TEST(FxStudioRenderContractTest, EveryPresetRendersDistinctFramesAndWritesPng)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);
    const QString directory = captureDirectory();
    std::printf("FxStudio render artifacts: %s\n", qPrintable(directory));

    int presetIndex = 0;
    for (const Fx::PresetDescriptor& descriptor : Fx::PresetCatalog::descriptors()) {
        Fx::Session session;
        session.insertPreset(descriptor.id, 0);
        ASSERT_EQ(session.eventTrack().events().size(), 1u)
            << ArtifactCore::toStdString(descriptor.id);
        const Fx::Event& event = session.eventTrack().events().front();
        EXPECT_EQ(event.sequence.family, descriptor.family);
        ASSERT_GT(event.sequence.durationFrames, 0);

        const auto rendered = ArtifactTestFxCueRenderer::renderSequence(
            input, 0, event.sequence.durationFrames,
            [&session](const std::int64_t frame) {
                return cuesAt(session.eventTrack(), frame);
            });

        const std::size_t expectedFrames =
            static_cast<std::size_t>(event.sequence.durationFrames + 1);
        ASSERT_EQ(rendered.frames.size(), expectedFrames);
        EXPECT_GT(rendered.report.distinctFrames, 2)
            << ArtifactCore::toStdString(descriptor.id);

        const QString stem = QStringLiteral("%1_%2")
                                 .arg(presetIndex, 1, 10, QChar('0'))
                                 .arg(sanitize(ArtifactCore::toStdString(descriptor.id)));
        for (std::size_t index = 0; index < rendered.frames.size(); ++index) {
            const QString path = directory + QStringLiteral("/preset_") + stem +
                                 QStringLiteral("_f%1.png")
                                     .arg(static_cast<int>(index), 3, 10, QChar('0'));
            EXPECT_TRUE(writePng(rendered.frames[index], path));
            EXPECT_GT(QFileInfo(path).size(), 100) << qPrintable(path);
        }

        const Image sheet = ArtifactTestFxCueRenderer::makeContactSheet(
            rendered.frames, 6, 2);
        ASSERT_FALSE(sheet.empty());
        const QString sheetPath =
            directory + QStringLiteral("/preset_") + stem + QStringLiteral("_sheet.png");
        EXPECT_TRUE(writePng(sheet, sheetPath));
        EXPECT_GT(QFileInfo(sheetPath).size(), 100) << qPrintable(sheetPath);

        // The loudest frame against the first frame, for a quick visual diff.
        std::size_t peak = 0;
        int peakDifference = 0;
        for (std::size_t index = 1; index < rendered.frames.size(); ++index) {
            const int changed = ArtifactTestFxCueRenderer::difference(
                                    rendered.frames.front(), rendered.frames[index])
                                    .changedPixels;
            if (changed > peakDifference) {
                peakDifference = changed;
                peak = index;
            }
        }
        EXPECT_GT(peakDifference, 0);
        EXPECT_TRUE(writePng(ArtifactTestFxCueRenderer::difference(
                                 rendered.frames.front(), rendered.frames[peak])
                                 .mask,
                             directory + QStringLiteral("/preset_") + stem +
                                 QStringLiteral("_peakdiff.png")));

        ++presetIndex;
    }
    EXPECT_EQ(presetIndex, 5);
}

TEST(FxStudioRenderContractTest, RenderIsDeterministicAcrossRuns)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);

    Fx::Session first;
    Fx::Session second;
    first.insertPreset(ArtifactCore::String("fx.glitch.burst"), 0);
    second.insertPreset(ArtifactCore::String("fx.glitch.burst"), 0);
    ASSERT_EQ(first.eventTrack().events().size(), 1u);
    ASSERT_EQ(second.eventTrack().events().size(), 1u);

    const int duration = static_cast<int>(first.eventTrack().events().front()
                                              .sequence.durationFrames);
    const auto a = ArtifactTestFxCueRenderer::renderSequence(
        input, 0, duration,
        [&first](const std::int64_t frame) { return cuesAt(first.eventTrack(), frame); });
    const auto b = ArtifactTestFxCueRenderer::renderSequence(
        input, 0, duration,
        [&second](const std::int64_t frame) { return cuesAt(second.eventTrack(), frame); });

    ASSERT_EQ(a.frames.size(), b.frames.size());
    for (std::size_t index = 0; index < a.frames.size(); ++index) {
        EXPECT_EQ(a.frames[index].pixels, b.frames[index].pixels) << "frame " << index;
    }
    EXPECT_EQ(a.report.distinctFrames, b.report.distinctFrames);
}

// ===========================================================================
// viewport comparison and background
// ===========================================================================

TEST(FxStudioRenderContractTest, ComparisonModesSelectProcessedOriginalOrSplit)
{
    const Image original = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);
    const Image second = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight, true);

    std::vector<Cue> cues;
    Cue cue;
    cue.effect = Effect::Flash;
    cue.amplitude = 0.8f;
    cues.push_back(cue);

    Image processed = original;
    ArtifactTestFxCueRenderer::applyCues(processed, original, cues);
    ASSERT_TRUE(ArtifactTestFxCueRenderer::difference(original, processed).any());

    ArtifactTestFxCueRenderer::FrameOptions options;
    options.comparison = Comparison::Processed;
    EXPECT_EQ(ArtifactTestFxCueRenderer::renderFrame(processed, original, options).pixels,
              processed.pixels);

    options.comparison = Comparison::Original;
    EXPECT_EQ(ArtifactTestFxCueRenderer::renderFrame(processed, original, options).pixels,
              original.pixels);

    options.comparison = Comparison::Split;
    options.splitPosition = 0.5f;
    const Image split = ArtifactTestFxCueRenderer::renderFrame(processed, original, options);
    const int edge = kWidth / 2;
    for (int y = 0; y < split.height; ++y) {
        for (int x = 0; x < edge; ++x) {
            EXPECT_TRUE(pixelEquals(split.at(x, y), processed.at(x, y)))
                << "processed half " << x << "," << y;
        }
        for (int x = edge; x < split.width; ++x) {
            EXPECT_TRUE(pixelEquals(split.at(x, y), original.at(x, y)))
                << "original half " << x << "," << y;
        }
    }

    // The same split against a second input is a real A/B wipe.
    options.splitPosition = 0.5f;
    const Image ab = ArtifactTestFxCueRenderer::renderFrame(second, original, options);
    EXPECT_TRUE(pixelEquals(ab.at(edge / 2, kHeight / 2), second.at(edge / 2, kHeight / 2)));
    EXPECT_TRUE(pixelEquals(ab.at(edge + edge / 2, kHeight / 2),
                            original.at(edge + edge / 2, kHeight / 2)));
}

TEST(FxStudioRenderContractTest, BackgroundModesCompositeOntoAlpha)
{
    const Image graded = brightenedAlphaGradedImage();
    ASSERT_FLOAT_EQ(graded.at(0, 0)[3], 0.25f);

    ArtifactTestFxCueRenderer::FrameOptions options;
    options.comparison = Comparison::Processed;

    options.background = Background::Transparent;
    const Image transparent = ArtifactTestFxCueRenderer::renderFrame(graded, graded, options);
    EXPECT_FLOAT_EQ(transparent.at(10, 10)[3], 0.25f);

    options.background = Background::Dark;
    const Image dark = ArtifactTestFxCueRenderer::renderFrame(graded, graded, options);
    EXPECT_FLOAT_EQ(dark.at(10, 10)[3], 1.0f);

    options.background = Background::Light;
    const Image light = ArtifactTestFxCueRenderer::renderFrame(graded, graded, options);

    options.background = Background::Checkerboard;
    const Image checker = ArtifactTestFxCueRenderer::renderFrame(graded, graded, options);

    EXPECT_TRUE(ArtifactTestFxCueRenderer::difference(dark, light).any());
    EXPECT_TRUE(ArtifactTestFxCueRenderer::difference(dark, checker).any());
    EXPECT_FALSE(ArtifactTestFxCueRenderer::difference(dark, checker).mask.empty());
}

TEST(FxStudioRenderContractTest, ContactSheetTilesEveryFrame)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);
    std::vector<Image> frames;
    for (int index = 0; index < 7; ++index) {
        Image frame = input;
        frame.at(index, index)[0] = 1.0f;
        frames.push_back(frame);
    }

    const Image sheet = ArtifactTestFxCueRenderer::makeContactSheet(frames, 3, 2);
    ASSERT_FALSE(sheet.empty());
    EXPECT_EQ(sheet.width, (kWidth / 2) * 3);
    EXPECT_EQ(sheet.height, (kHeight / 2) * 3);

    EXPECT_TRUE(writePng(sheet, captureDirectory() +
                                    QStringLiteral("/contact_sheet_contract.png")));
}

// ===========================================================================
// difference reporting
// ===========================================================================

TEST(FxStudioRenderContractTest, DifferenceReportsCountAndBounds)
{
    const Image input = ArtifactTestFxCueRenderer::makeDiagnosticImage(kWidth, kHeight);
    Image changed = input;
    for (int y = 20; y < 26; ++y) {
        for (int x = 30; x < 40; ++x) {
            changed.at(x, y)[1] = 0.0f;
        }
    }

    const auto difference = ArtifactTestFxCueRenderer::difference(input, changed);
    EXPECT_EQ(difference.changedPixels, 60);
    EXPECT_EQ(difference.minX, 30);
    EXPECT_EQ(difference.minY, 20);
    EXPECT_EQ(difference.maxX, 39);
    EXPECT_EQ(difference.maxY, 25);
    EXPECT_EQ(difference.width(), 10);
    EXPECT_EQ(difference.height(), 6);

    // Identical images report nothing.
    EXPECT_FALSE(ArtifactTestFxCueRenderer::difference(input, input).any());
}
