// ArtifactPr-style contract test for the portable video transition renders.
//
// Video.Transitions.* already turn a DecodedVideoFrame pair into a composited
// frame, but nothing in the test suite could look at the result: the only
// consumer was the NLE render path. This suite renders every registered
// transition over a deterministic A/B pair, checks the mechanical contract
// (factory registration, determinism, frame shape, alpha, left-frame-only
// mutation), measures how much of each output comes from which input, and
// writes PNG strips, per-progress grids and a behaviour report to disk so a
// reviewer can see the transition instead of inferring it from numbers.
//
// Pure CPU: no GPU device, no window, no media file, no NLE project.
#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QtGlobal>
#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <variant>
#include <vector>

#include "../ArtifactTestTransitionRenderer.h"

import NLE.Core;
import Video.VideoFrame;
import Video.AbstractTransition;
import Video.TransitionFactory;
import Video.Transitions.BlockDissolveTransition;
import Video.Transitions.CrossfadeTransition;
import Video.Transitions.CubeTransition;
import Video.Transitions.DissolveTransition;
import Video.Transitions.DoorsTransition;
import Video.Transitions.FlipTransition;
import Video.Transitions.GlitchDisplaceTransition;
import Video.Transitions.GradientWipeTransition;
import Video.Transitions.IrisWipeTransition;
import Video.Transitions.LightLeakTransition;
import Video.Transitions.LinearWipeTransition;
import Video.Transitions.RadialWipeTransition;
import Video.Transitions.SlideTransition;
import Video.Transitions.SpinTransition;
import Video.Transitions.WipeTransition;
import Video.Transitions.ZoomTransition;

#include "../ArtifactTestTransitionBridge.h"

namespace {

using ArtifactCore::NLE::TransitionKind;
using ArtifactTestTransitionRenderer::FrameStats;
using ArtifactTestTransitionRenderer::Image;
using ArtifactTestTransitionRenderer::Source;
using ArtifactTestTransitionBridge::RenderResult;

constexpr int kWidth = 96;
constexpr int kHeight = 64;
constexpr int kFrameIndex = 0;
constexpr double kFps = 30.0;
constexpr std::array<double, 5> kSteps{0.0, 0.25, 0.5, 0.75, 1.0};

Image inputA(const int width = kWidth, const int height = kHeight)
{
    return ArtifactTestTransitionRenderer::makeDiagnosticImage(width, height, false);
}

Image inputB(const int width = kWidth, const int height = kHeight)
{
    return ArtifactTestTransitionRenderer::makeDiagnosticImage(width, height, true);
}

ArtifactTestTransitionBridge::RenderOptions optionsAt(const double progress,
                                                      const int width = kWidth,
                                                      const int height = kHeight)
{
    ArtifactTestTransitionBridge::RenderOptions options;
    options.progress = progress;
    options.frameIndex = kFrameIndex;
    options.fps = kFps;
    options.width = width;
    options.height = height;
    return options;
}

RenderResult renderAt(const TransitionKind kind, const double progress)
{
    return ArtifactTestTransitionBridge::render(kind, inputA(), inputB(),
                                                optionsAt(progress));
}

FrameStats statsAt(const TransitionKind kind, const double progress)
{
    const RenderResult result = renderAt(kind, progress);
    return ArtifactTestTransitionRenderer::classifyFrame(result.image, inputA(),
                                                         inputB());
}

QString kindLabel(const TransitionKind kind)
{
    return QString::fromUtf8(ArtifactTestTransitionBridge::kindName(kind));
}

// ---------------------------------------------------------------------------
// PNG artifact output
// ---------------------------------------------------------------------------

QString captureDirectory()
{
    QString directory = qEnvironmentVariable("ARTIFACT_TRANSITION_CAPTURE_DIR");
    if (directory.isEmpty()) {
        directory = QDir::tempPath() + QStringLiteral("/artifact-transitions");
    }
    QDir().mkpath(directory);
    return directory;
}

bool writePng(const Image& image, const QString& path)
{
    const QImage qimage = ArtifactTestTransitionRenderer::toQImage(image);
    return !qimage.isNull() && qimage.save(path, "PNG");
}

QString progressStem(const double progress)
{
    const int percent = static_cast<int>(std::lround(progress * 100.0));
    return QString::number(percent).rightJustified(3, QChar('0'));
}

// ---------------------------------------------------------------------------
// behaviour report
// ---------------------------------------------------------------------------

const char* directionName(const Source start, const Source end)
{
    if (start == Source::Left && end == Source::Right) return "A->B";
    if (start == Source::Right && end == Source::Left) return "B->A";
    if (start == Source::Mixed && end == Source::Mixed) return "mixed";
    return "partial";
}

QJsonObject statsToJson(const FrameStats& stats)
{
    return QJsonObject{{QStringLiteral("leftPixels"), stats.leftPixels},
                       {QStringLiteral("rightPixels"), stats.rightPixels},
                       {QStringLiteral("mixedPixels"), stats.mixedPixels},
                       {QStringLiteral("leftFraction"), stats.leftFraction},
                       {QStringLiteral("rightFraction"), stats.rightFraction},
                       {QStringLiteral("mixedFraction"), stats.mixedFraction},
                       {QStringLiteral("seamColumn"), stats.seamColumn}};
}

// Renders every step for one kind, writes its strip, and returns the report
// entry. `directory` may be empty to skip the PNG write.
QJsonObject writeKindArtifacts(const TransitionKind kind, const QString& directory)
{
    const Image a = inputA();
    const Image b = inputB();
    const QString name = kindLabel(kind);

    std::vector<Image> frames;
    std::vector<FrameStats> statistics;
    QJsonArray steps;
    Image previous;
    for (const double progress : kSteps) {
        const RenderResult result = renderAt(kind, progress);
        const FrameStats stats =
            ArtifactTestTransitionRenderer::classifyFrame(result.image, a, b);
        const int changedFromPrevious =
            previous.empty()
                ? 0
                : ArtifactTestTransitionRenderer::difference(previous, result.image)
                      .changedPixels;
        steps.append(QJsonObject{{QStringLiteral("progress"), progress},
                                 {QStringLiteral("stats"), statsToJson(stats)},
                                 {QStringLiteral("changedFromPrevious"),
                                  changedFromPrevious}});
        if (!directory.isEmpty()) {
            writePng(result.image, directory + QStringLiteral("/transition_") +
                                       name + QStringLiteral("_p") +
                                       progressStem(progress) + QStringLiteral(".png"));
        }
        frames.push_back(result.image);
        statistics.push_back(stats);
        previous = result.image;
    }

    const Source firstSource =
        ArtifactTestTransitionRenderer::dominantSource(statistics.front());
    const Source lastSource =
        ArtifactTestTransitionRenderer::dominantSource(statistics.back());

    if (!directory.isEmpty()) {
        writePng(ArtifactTestTransitionRenderer::makeContactSheet(
                     frames, static_cast<int>(kSteps.size()), 2),
                 directory + QStringLiteral("/transition_") + name +
                     QStringLiteral("_sheet.png"));
    }

    std::printf("%-16s p0=%-6s p1=%-6s dir=%-8s seam=%4d  p0 B%%=%5.1f p1 B%%=%5.1f\n",
                qPrintable(name),
                ArtifactTestTransitionRenderer::sourceName(firstSource),
                ArtifactTestTransitionRenderer::sourceName(lastSource),
                directionName(firstSource, lastSource),
                statistics[2].seamColumn, statistics.front().rightFraction * 100.0,
                statistics.back().rightFraction * 100.0);

    return QJsonObject{
        {QStringLiteral("name"), name},
        {QStringLiteral("kind"), ArtifactTestTransitionBridge::kindValue(kind)},
        {QStringLiteral("startSource"),
         QString::fromUtf8(ArtifactTestTransitionRenderer::sourceName(firstSource))},
        {QStringLiteral("endSource"),
         QString::fromUtf8(ArtifactTestTransitionRenderer::sourceName(lastSource))},
        {QStringLiteral("direction"),
         QString::fromUtf8(directionName(firstSource, lastSource))},
        {QStringLiteral("steps"), steps}};
}

QJsonObject writeArtifacts(const QString& directory)
{
    QDir().mkpath(directory);
    std::printf("Transition render artifacts: %s\n", qPrintable(directory));

    writePng(ArtifactTestTransitionRenderer::makeContactSheet({inputA(), inputB()}, 2, 2),
             directory + QStringLiteral("/inputs.png"));

    QJsonArray transitions;
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        transitions.append(writeKindArtifacts(kind, directory));
    }

    // One grid per progress: all kinds side by side is the fastest way to spot
    // a transition that is inverted or never completes.
    for (const double progress : kSteps) {
        std::vector<Image> frames;
        for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
            frames.push_back(renderAt(kind, progress).image);
        }
        writePng(ArtifactTestTransitionRenderer::makeContactSheet(frames, 4, 2),
                 directory + QStringLiteral("/all_p") + progressStem(progress) +
                     QStringLiteral(".png"));
    }

    const QJsonObject report{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("width"), kWidth},
        {QStringLiteral("height"), kHeight},
        {QStringLiteral("frameIndex"), kFrameIndex},
        {QStringLiteral("fps"), kFps},
        {QStringLiteral("steps"),
         QJsonArray{kSteps[0], kSteps[1], kSteps[2], kSteps[3], kSteps[4]}},
        {QStringLiteral("transitions"), transitions}};

    QFile file(directory + QStringLiteral("/transition_report.json"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
        file.close();
    }
    return report;
}

} // namespace

// ===========================================================================
// registry
// ===========================================================================

TEST(TransitionRenderContractTest, FactoryRegistersEveryConcreteKind)
{
    ArtifactTestTransitionBridge::ensureRegistered();
    const std::vector<TransitionKind> kinds =
        ArtifactTestTransitionBridge::concreteKinds();
    ASSERT_EQ(kinds.size(), std::size_t{16});

    for (const TransitionKind kind : kinds) {
        const char* name = ArtifactTestTransitionBridge::kindName(kind);
        ArtifactCore::AbstractTransition* transition =
            ArtifactCore::TransitionFactory::instance().create(kind);
        ASSERT_NE(transition, nullptr) << name;
        EXPECT_EQ(transition->kind(), kind) << name;
        EXPECT_STREQ(transition->name(), name);
        delete transition;
    }

    const QVector<TransitionKind> available =
        ArtifactCore::TransitionFactory::instance().availableKinds();
    EXPECT_EQ(available.size(), static_cast<qsizetype>(kinds.size()));
    for (const TransitionKind kind : kinds) {
        EXPECT_TRUE(available.contains(kind))
            << ArtifactTestTransitionBridge::kindName(kind);
    }
    // Cut has no implementation; the NLE bridge treats a missing creator as a
    // passthrough, so the factory must keep reporting no creator for it.
    EXPECT_FALSE(available.contains(TransitionKind::Cut));
    EXPECT_EQ(ArtifactCore::TransitionFactory::instance().create(TransitionKind::Cut),
              nullptr);
}

TEST(TransitionRenderContractTest, CutIsAPassthroughOfTheLeftFrame)
{
    const RenderResult result = renderAt(TransitionKind::Cut, 0.37);
    EXPECT_FALSE(result.hasCreator);
    EXPECT_STREQ(result.name, "Cut");
    ASSERT_TRUE(result.validOutput);
    EXPECT_TRUE(result.image == inputA());
}

// ===========================================================================
// mechanical contract
// ===========================================================================

TEST(TransitionRenderContractTest, RendersAreDeterministic)
{
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        for (const double progress : kSteps) {
            const RenderResult first = renderAt(kind, progress);
            const RenderResult second = renderAt(kind, progress);
            ASSERT_TRUE(first.validOutput)
                << ArtifactTestTransitionBridge::kindName(kind);
            EXPECT_TRUE(first.image == second.image)
                << ArtifactTestTransitionBridge::kindName(kind) << " at progress "
                << progress;
        }
    }
}

TEST(TransitionRenderContractTest, OutputShapeAndAlphaMatchTheInputs)
{
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        for (const double progress : kSteps) {
            const RenderResult result = renderAt(kind, progress);
            ASSERT_TRUE(result.validOutput)
                << ArtifactTestTransitionBridge::kindName(kind);
            EXPECT_EQ(result.image.width, kWidth);
            EXPECT_EQ(result.image.height, kHeight);
            EXPECT_EQ(result.image.pixels.size(),
                      static_cast<std::size_t>(kWidth) * kHeight * 4u);
            // Both diagnostic inputs are opaque, so every transition must emit
            // opaque output regardless of which source it samples.
            EXPECT_TRUE(ArtifactTestTransitionRenderer::alphaEquals(result.image, 255))
                << ArtifactTestTransitionBridge::kindName(kind);
        }
    }
}

TEST(TransitionRenderContractTest, ProcessMutatesOnlyTheLeftFrame)
{
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        for (const double progress : kSteps) {
            const RenderResult result = renderAt(kind, progress);
            ASSERT_TRUE(result.validOutput)
                << ArtifactTestTransitionBridge::kindName(kind);
            EXPECT_TRUE(result.rightAfter == inputB())
                << ArtifactTestTransitionBridge::kindName(kind)
                << " modified the right frame";
        }
    }
}

TEST(TransitionRenderContractTest, EveryKindChangesPixelsAtThreeQuarterProgress)
{
    const Image a = inputA();
    const Image b = inputB();
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        const RenderResult result = renderAt(kind, 0.75);
        ASSERT_TRUE(result.validOutput)
            << ArtifactTestTransitionBridge::kindName(kind);
        const auto fromLeft = ArtifactTestTransitionRenderer::difference(result.image, a);
        const auto fromRight = ArtifactTestTransitionRenderer::difference(result.image, b);
        // A transition that reproduces either input exactly at 3/4 progress is
        // not transitioning at all.
        EXPECT_GT(fromLeft.changedPixels, 0)
            << ArtifactTestTransitionBridge::kindName(kind);
        EXPECT_GT(fromRight.changedPixels, 0)
            << ArtifactTestTransitionBridge::kindName(kind);
    }
}

TEST(TransitionRenderContractTest, WipeAndSlideSplitAtHalfWidth)
{
    const Image a = inputA();
    const Image b = inputB();
    const int half = kWidth / 2;

    // Wipe: the incoming frame is revealed from the left edge, so at half
    // progress the left half is B and the right half is A.
    const RenderResult wipe = renderAt(TransitionKind::Wipe, 0.5);
    ASSERT_TRUE(wipe.validOutput);
    int wipeLeftMatches = 0;
    int wipeRightMatches = 0;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < half; ++x) {
            if (ArtifactTestTransitionRenderer::pixelEquals(wipe.image.at(x, y),
                                                            b.at(x, y))) {
                ++wipeLeftMatches;
            }
        }
        for (int x = half; x < kWidth; ++x) {
            if (ArtifactTestTransitionRenderer::pixelEquals(wipe.image.at(x, y),
                                                            a.at(x, y))) {
                ++wipeRightMatches;
            }
        }
    }
    EXPECT_EQ(wipeLeftMatches, half * kHeight);
    EXPECT_EQ(wipeRightMatches, (kWidth - half) * kHeight);

    // Slide: the ordering is the opposite one. The right half keeps A, and the
    // left half shows B shifted by half a frame with a wrap around the edge.
    const RenderResult slide = renderAt(TransitionKind::Slide, 0.5);
    ASSERT_TRUE(slide.validOutput);
    int slideLeftMatches = 0;
    int slideRightMatches = 0;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < half; ++x) {
            if (ArtifactTestTransitionRenderer::pixelEquals(
                    slide.image.at(x, y), b.at((x + half) % kWidth, y))) {
                ++slideLeftMatches;
            }
        }
        for (int x = half; x < kWidth; ++x) {
            if (ArtifactTestTransitionRenderer::pixelEquals(slide.image.at(x, y),
                                                            a.at(x, y))) {
                ++slideRightMatches;
            }
        }
    }
    EXPECT_EQ(slideLeftMatches, half * kHeight);
    EXPECT_EQ(slideRightMatches, (kWidth - half) * kHeight);
}

TEST(TransitionRenderContractTest, PagedRevealsAdvanceMonotonically)
{
    // Ordered reveals may run A->B or B->A depending on the implementation, but
    // they must never un-reveal as progress advances.
    const std::array<TransitionKind, 7> paged{
        TransitionKind::Wipe,    TransitionKind::LinearWipe,
        TransitionKind::RadialWipe, TransitionKind::IrisWipe,
        TransitionKind::Dissolve,   TransitionKind::BlockDissolve,
        TransitionKind::Slide};

    for (const TransitionKind kind : paged) {
        std::vector<double> fractions;
        for (const double progress : kSteps) {
            fractions.push_back(statsAt(kind, progress).rightFraction);
        }
        constexpr double kEpsilon = 1.0e-9;
        bool nonDecreasing = true;
        bool nonIncreasing = true;
        for (std::size_t index = 1; index < fractions.size(); ++index) {
            if (fractions[index] + kEpsilon < fractions[index - 1]) {
                nonDecreasing = false;
            }
            if (fractions[index] > fractions[index - 1] + kEpsilon) {
                nonIncreasing = false;
            }
        }
        EXPECT_TRUE(nonDecreasing || nonIncreasing)
            << ArtifactTestTransitionBridge::kindName(kind)
            << " right-source fraction is not monotonic";
    }
}

TEST(TransitionRenderContractTest, InvalidOrMismatchedFramesDoNotCrash)
{
    const Image empty;
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        const RenderResult result = ArtifactTestTransitionBridge::render(
            kind, empty, empty, optionsAt(0.5, 0, 0));
        EXPECT_TRUE(result.image.empty())
            << ArtifactTestTransitionBridge::kindName(kind);
    }

    // A right frame smaller than the left one must be handled by the shared
    // min(width) / min(height) rule, not by reading past the right buffer.
    const Image a = inputA();
    const Image small = inputB(32, 32);
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        const RenderResult result =
            ArtifactTestTransitionBridge::render(kind, a, small, optionsAt(0.5, 0, 0));
        ASSERT_TRUE(result.validOutput)
            << ArtifactTestTransitionBridge::kindName(kind);
        EXPECT_EQ(result.image.width, kWidth);
        EXPECT_EQ(result.image.height, kHeight);
    }
}

// ===========================================================================
// endpoint and per-step contract
// ===========================================================================

TEST(TransitionRenderContractTest, EveryKindMatchesTheDiagnosticEndpoints)
{
    const Image a = inputA();
    const Image b = inputB();
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        const RenderResult start = renderAt(kind, 0.0);
        const RenderResult end = renderAt(kind, 1.0);
        ASSERT_TRUE(start.validOutput)
            << ArtifactTestTransitionBridge::kindName(kind);
        ASSERT_TRUE(end.validOutput)
            << ArtifactTestTransitionBridge::kindName(kind);
        EXPECT_TRUE(start.image == a)
            << ArtifactTestTransitionBridge::kindName(kind)
            << " does not start on the left frame";
        EXPECT_TRUE(end.image == b)
            << ArtifactTestTransitionBridge::kindName(kind)
            << " does not end on the right frame";
    }
}

TEST(TransitionRenderContractTest, EveryKindChangesOnEveryProgressStep)
{
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        Image previous = renderAt(kind, 0.0).image;
        for (const double progress : {0.25, 0.5, 0.75, 1.0}) {
            const Image current = renderAt(kind, progress).image;
            EXPECT_GT(ArtifactTestTransitionRenderer::difference(previous, current)
                          .changedPixels,
                      0)
                << ArtifactTestTransitionBridge::kindName(kind)
                << " did not change at progress " << progress;
            previous = current;
        }
    }
}

TEST(TransitionRenderContractTest, NonOpaqueAlphaIsPreserved)
{
    Image a = inputA();
    Image b = inputB();
    for (std::size_t index = 3; index < a.pixels.size(); index += 4) {
        a.pixels[index] = 77u;
        b.pixels[index] = 77u;
    }
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        const RenderResult result = ArtifactTestTransitionBridge::render(
            kind, a, b, optionsAt(0.5));
        ASSERT_TRUE(result.validOutput)
            << ArtifactTestTransitionBridge::kindName(kind);
        // Every transition either copies a pixel or blends the two inputs; with
        // both inputs at alpha 77 the result must stay 77 instead of being
        // forced opaque.
        EXPECT_TRUE(ArtifactTestTransitionRenderer::alphaEquals(result.image, 77))
            << ArtifactTestTransitionBridge::kindName(kind)
            << " changed a uniform non-opaque alpha";
    }
}

TEST(TransitionRenderContractTest, FrameStatsFractionsSumToOne)
{
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        for (const double progress : kSteps) {
            const FrameStats stats = statsAt(kind, progress);
            EXPECT_NEAR(stats.leftFraction + stats.rightFraction +
                            stats.mixedFraction,
                        1.0, 1.0e-9)
                << ArtifactTestTransitionBridge::kindName(kind);
        }
    }
}

TEST(TransitionRenderContractTest, WipeSeamFollowsProgress)
{
    const Image a = inputA();
    const Image b = inputB();
    struct SeamCase {
        double progress;
        int columns;
    };
    const std::array<SeamCase, 3> cases{
        SeamCase{0.25, 24}, SeamCase{0.5, 48}, SeamCase{0.75, 72}};
    for (const SeamCase& expected : cases) {
        const RenderResult result = renderAt(TransitionKind::Wipe, expected.progress);
        ASSERT_TRUE(result.validOutput);
        const FrameStats stats =
            ArtifactTestTransitionRenderer::classifyFrame(result.image, a, b);
        EXPECT_EQ(stats.rightPixels, expected.columns * kHeight)
            << "progress " << expected.progress;

        // The wipe reveals the incoming frame from the left edge, so the last
        // revealed column must already be B and the next column still A.
        int lastRightMatches = 0;
        int firstLeftMatches = 0;
        for (int y = 0; y < kHeight; ++y) {
            if (ArtifactTestTransitionRenderer::pixelEquals(
                    result.image.at(expected.columns - 1, y),
                    b.at(expected.columns - 1, y))) {
                ++lastRightMatches;
            }
            if (ArtifactTestTransitionRenderer::pixelEquals(
                    result.image.at(expected.columns, y),
                    a.at(expected.columns, y))) {
                ++firstLeftMatches;
            }
        }
        EXPECT_EQ(lastRightMatches, kHeight)
            << "progress " << expected.progress;
        EXPECT_EQ(firstLeftMatches, kHeight)
            << "progress " << expected.progress;
    }
}

TEST(TransitionRenderContractTest, OrderedRevealsPassThroughPartialCoverage)
{
    const std::array<TransitionKind, 6> reveals{
        TransitionKind::Wipe,      TransitionKind::LinearWipe,
        TransitionKind::RadialWipe, TransitionKind::IrisWipe,
        TransitionKind::Dissolve,  TransitionKind::BlockDissolve};
    for (const TransitionKind kind : reveals) {
        const double coverage = statsAt(kind, 0.5).rightFraction;
        EXPECT_GT(coverage, 0.05)
            << ArtifactTestTransitionBridge::kindName(kind);
        EXPECT_LT(coverage, 0.95)
            << ArtifactTestTransitionBridge::kindName(kind);
    }
}

TEST(TransitionRenderContractTest, RevealGeometryOpensFromTheExpectedOrigin)
{
    const Image a = inputA();
    const Image b = inputB();
    const auto isLeft = [](const RenderResult& result, const int x, const int y,
                           const Image& reference) {
        return ArtifactTestTransitionRenderer::pixelEquals(
            result.image.at(x, y), reference.at(x, y));
    };

    // RadialWipe expands from the centre: the centre is already B at half
    // progress while the corner still shows A.
    const RenderResult radial = renderAt(TransitionKind::RadialWipe, 0.5);
    ASSERT_TRUE(radial.validOutput);
    EXPECT_TRUE(isLeft(radial, kWidth / 2, kHeight / 2, b));
    EXPECT_TRUE(isLeft(radial, 0, 0, a));

    // IrisWipe is the inverse: A survives in the centre, B owns the edges.
    const RenderResult iris = renderAt(TransitionKind::IrisWipe, 0.5);
    ASSERT_TRUE(iris.validOutput);
    EXPECT_TRUE(isLeft(iris, kWidth / 2, kHeight / 2, a));
    EXPECT_TRUE(isLeft(iris, 0, 0, b));

    // Doors opens from the centre outward.
    const RenderResult doors = renderAt(TransitionKind::Doors, 0.5);
    ASSERT_TRUE(doors.validOutput);
    EXPECT_TRUE(isLeft(doors, kWidth / 2, kHeight / 2, b));
    EXPECT_TRUE(isLeft(doors, 0, kHeight / 2, a));

    // GradientWipe sweeps from the top-left corner toward the bottom-right.
    const RenderResult gradient = renderAt(TransitionKind::GradientWipe, 0.5);
    ASSERT_TRUE(gradient.validOutput);
    EXPECT_TRUE(isLeft(gradient, 0, 0, b));
    EXPECT_TRUE(isLeft(gradient, kWidth - 1, kHeight - 1, a));
}

TEST(TransitionRenderContractTest, DissolvePatternDependsOnFrameIndex)
{
    ArtifactTestTransitionBridge::RenderOptions first = optionsAt(0.5);
    first.frameIndex = 0;
    ArtifactTestTransitionBridge::RenderOptions second = optionsAt(0.5);
    second.frameIndex = 1;

    const Image a = inputA();
    const Image b = inputB();
    const RenderResult one = ArtifactTestTransitionBridge::render(
        TransitionKind::Dissolve, a, b, first);
    const RenderResult two = ArtifactTestTransitionBridge::render(
        TransitionKind::Dissolve, a, b, second);
    ASSERT_TRUE(one.validOutput);
    ASSERT_TRUE(two.validOutput);
    // Dissolve seeds its noise with frameIndex + fps, so a different frame must
    // produce a different reveal pattern (and still a valid, opaque frame).
    EXPECT_TRUE(one.image != two.image);
    EXPECT_TRUE(ArtifactTestTransitionRenderer::alphaEquals(one.image, 255));
    EXPECT_TRUE(ArtifactTestTransitionRenderer::alphaEquals(two.image, 255));
}

TEST(TransitionRenderContractTest, DegenerateFrameSizesRender)
{
    struct SizeCase {
        int width;
        int height;
    };
    const std::array<SizeCase, 4> sizes{
        SizeCase{1, 1}, SizeCase{3, 7}, SizeCase{1, 64}, SizeCase{64, 1}};
    for (const SizeCase& size : sizes) {
        for (const TransitionKind kind :
             ArtifactTestTransitionBridge::concreteKinds()) {
            const RenderResult result = ArtifactTestTransitionBridge::render(
                kind, inputA(size.width, size.height),
                inputB(size.width, size.height),
                optionsAt(0.5, size.width, size.height));
            ASSERT_TRUE(result.validOutput)
                << ArtifactTestTransitionBridge::kindName(kind) << " at "
                << size.width << "x" << size.height;
            EXPECT_EQ(result.image.width, size.width);
            EXPECT_EQ(result.image.height, size.height);
            EXPECT_TRUE(
                ArtifactTestTransitionRenderer::alphaEquals(result.image, 255))
                << ArtifactTestTransitionBridge::kindName(kind) << " at "
                << size.width << "x" << size.height;
        }
    }
}

TEST(TransitionRenderContractTest, ContactSheetGeometryMatchesTheRequest)
{
    std::vector<Image> frames;
    for (int index = 0; index < 5; ++index) {
        frames.push_back(inputA());
    }

    const Image wide = ArtifactTestTransitionRenderer::makeContactSheet(frames, 3, 2);
    EXPECT_EQ(wide.width, (kWidth / 2) * 3);
    EXPECT_EQ(wide.height, (kHeight / 2) * 2);

    const Image full = ArtifactTestTransitionRenderer::makeContactSheet(frames, 5, 1);
    EXPECT_EQ(full.width, kWidth * 5);
    EXPECT_EQ(full.height, kHeight);
}

// ===========================================================================
// visual artifacts
// ===========================================================================

TEST(TransitionRenderContractTest, WritesFrameStripsContactSheetsAndBehaviourReport)
{
    const QString directory = captureDirectory();
    const QJsonObject report = writeArtifacts(directory);

    EXPECT_EQ(report.value(QStringLiteral("transitions")).toArray().size(),
              static_cast<qsizetype>(
                  ArtifactTestTransitionBridge::concreteKinds().size()));

    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        const QString name = kindLabel(kind);
        for (const double progress : kSteps) {
            const QString framePath =
                directory + QStringLiteral("/transition_") + name +
                QStringLiteral("_p") + progressStem(progress) + QStringLiteral(".png");
            EXPECT_GT(QFileInfo(framePath).size(), 0) << qPrintable(framePath);
        }
        const QString sheetPath = directory + QStringLiteral("/transition_") + name +
                                  QStringLiteral("_sheet.png");
        EXPECT_GT(QFileInfo(sheetPath).size(), 0) << qPrintable(sheetPath);
    }

    for (const double progress : kSteps) {
        const QString gridPath = directory + QStringLiteral("/all_p") +
                                 progressStem(progress) + QStringLiteral(".png");
        EXPECT_GT(QFileInfo(gridPath).size(), 0) << qPrintable(gridPath);
    }

    const QString reportPath =
        directory + QStringLiteral("/transition_report.json");
    EXPECT_GT(QFileInfo(reportPath).size(), 0) << qPrintable(reportPath);
}
