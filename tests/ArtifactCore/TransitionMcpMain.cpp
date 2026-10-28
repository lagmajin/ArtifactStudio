// Standalone MCP-hosted lab for the portable video transitions.
//
// This is the "drive it from outside" half of the transition test work. It uses
// the same render path as ArtifactTransitionRenderContractTest and exposes it as
// a stdio MCP server, so an agent or a script can list the registered kinds,
// render one at any progress, build strips and grids, diff two progresses, and
// write PNG captures for visual debugging.
//
//   ArtifactTransitionMcpTest --self-check
//       Deterministic headless smoke run (used by CTest).
//   ArtifactTransitionMcpTest --mcp-stdio [--width N --height N]
//       Serves tools over newline-delimited JSON-RPC on stdio. No window and no
//       Qt platform plugin are needed: the host only uses QtCore, QImage and the
//       transition factory.
//
// No GPU device and no media file are needed; the A/B inputs are synthetic
// diagnostic images produced by ArtifactTestTransitionRenderer.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <QCoreApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <variant>
#include <vector>

#include "../ArtifactTestImageArtifacts.h"
#include "../ArtifactTestMcpServer.h"
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
using ArtifactTestTransitionBridge::RenderOptions;
using ArtifactTestTransitionBridge::RenderResult;

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

struct State {
    int width = 96;
    int height = 64;
    int frameIndex = 0;
    double fps = 30.0;
    QString captureDirectory;
    Image inputA;
    Image inputB;
};

State* g_state = nullptr;

void ensureInputs(const int width, const int height)
{
    if (g_state->width == width && g_state->height == height &&
        !g_state->inputA.empty()) {
        return;
    }
    g_state->width = width;
    g_state->height = height;
    g_state->inputA = ArtifactTestTransitionRenderer::makeDiagnosticImage(width, height, false);
    g_state->inputB = ArtifactTestTransitionRenderer::makeDiagnosticImage(width, height, true);
}

// ---------------------------------------------------------------------------
// argument helpers
// ---------------------------------------------------------------------------

int argInt(const QJsonObject& args, const char* key, const int fallback)
{
    const QJsonValue value = args.value(QString::fromUtf8(key));
    if (!value.isDouble()) return fallback;
    return static_cast<int>(std::lround(value.toDouble()));
}

double argDouble(const QJsonObject& args, const char* key, const double fallback)
{
    const QJsonValue value = args.value(QString::fromUtf8(key));
    return value.isDouble() ? value.toDouble() : fallback;
}

QString argString(const QJsonObject& args, const char* key,
                  const QString& fallback = QString())
{
    const QJsonValue value = args.value(QString::fromUtf8(key));
    return value.isString() ? value.toString() : fallback;
}

QJsonObject errorResult(const QString& message)
{
    return QJsonObject{{QStringLiteral("error"), message}};
}

QJsonObject textContent(const QString& text)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                       {QStringLiteral("text"), text}};
}

QJsonObject imageContent(const QString& name, const Image& image)
{
    return ArtifactTestImageArtifacts::imageContent(
        name, ArtifactTestTransitionRenderer::toQImage(image));
}

bool resolveKind(const QString& text, TransitionKind& out)
{
    if (text.isEmpty()) return false;
    const QByteArray utf8 = text.toUtf8();
    if (ArtifactTestTransitionBridge::kindFromName(utf8.constData(), out)) return true;
    bool numeric = false;
    const int value = text.toInt(&numeric);
    if (!numeric) return false;
    out = static_cast<TransitionKind>(value);
    return true;
}

bool argKind(const QJsonObject& args, const char* key, TransitionKind& out)
{
    return resolveKind(argString(args, key), out);
}

QString kindLabel(const TransitionKind kind)
{
    return QString::fromUtf8(ArtifactTestTransitionBridge::kindName(kind));
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

QString progressStem(const double progress)
{
    const int percent = static_cast<int>(std::lround(progress * 100.0));
    return QString::number(percent).rightJustified(3, QChar('0'));
}

std::vector<double> progressSteps(const int steps, const double first,
                                  const double last)
{
    const int count = std::max(2, steps);
    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        const double t = static_cast<double>(index) / static_cast<double>(count - 1);
        values.push_back(first + (last - first) * t);
    }
    return values;
}

// ---------------------------------------------------------------------------
// rendering
// ---------------------------------------------------------------------------

struct FrameRender {
    bool ok = false;
    RenderResult result;
    FrameStats stats;
    Image left;
    Image right;
};

FrameRender renderKind(const TransitionKind kind, const double progress,
                       const int width, const int height, const int frameIndex,
                       const double fps)
{
    FrameRender render;
    if (width <= 0 || height <= 0) return render;
    ensureInputs(width, height);

    RenderOptions options;
    options.progress = std::clamp(progress, 0.0, 1.0);
    options.frameIndex = frameIndex;
    options.fps = fps;
    options.width = width;
    options.height = height;

    render.left = g_state->inputA;
    render.right = g_state->inputB;
    render.result = ArtifactTestTransitionBridge::render(kind, render.left,
                                                         render.right, options);
    render.stats = ArtifactTestTransitionRenderer::classifyFrame(
        render.result.image, render.left, render.right);
    render.ok = render.result.validOutput;
    return render;
}

QJsonObject frameRenderJson(const TransitionKind kind, const double progress,
                            const FrameRender& render)
{
    return QJsonObject{
        {QStringLiteral("kind"), kindLabel(kind)},
        {QStringLiteral("kindValue"), ArtifactTestTransitionBridge::kindValue(kind)},
        {QStringLiteral("progress"), progress},
        {QStringLiteral("created"), render.result.hasCreator},
        {QStringLiteral("name"), QString::fromUtf8(render.result.name)},
        {QStringLiteral("width"), render.result.image.width},
        {QStringLiteral("height"), render.result.image.height},
        {QStringLiteral("stats"), statsToJson(render.stats)}};
}

// ---------------------------------------------------------------------------
// tool handlers
// ---------------------------------------------------------------------------

QJsonArray kindList()
{
    ArtifactTestTransitionBridge::ensureRegistered();
    const QVector<TransitionKind> available =
        ArtifactCore::TransitionFactory::instance().availableKinds();
    QJsonArray kinds;
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        kinds.append(QJsonObject{
            {QStringLiteral("name"), kindLabel(kind)},
            {QStringLiteral("value"), ArtifactTestTransitionBridge::kindValue(kind)},
            {QStringLiteral("registered"), available.contains(kind)}});
    }
    return kinds;
}

QJsonObject describeTool(const QJsonObject&)
{
    return QJsonObject{
        {QStringLiteral("server"), QStringLiteral("artifact-transition-mcp-test")},
        {QStringLiteral("kindCount"),
         static_cast<int>(ArtifactTestTransitionBridge::concreteKinds().size())},
        {QStringLiteral("kinds"), kindList()},
        {QStringLiteral("passthrough"),
         QStringLiteral("Cut has no creator; the harness and the NLE bridge both "
                        "return the left frame unchanged")},
        {QStringLiteral("input"),
         QStringLiteral("synthetic diagnostic A/B pair; a pixel is counted as A or "
                        "B only when it is byte-identical to that input")},
        {QStringLiteral("imageSize"),
         QJsonObject{{QStringLiteral("width"), g_state->width},
                     {QStringLiteral("height"), g_state->height}}},
        {QStringLiteral("captureDirectory"), g_state->captureDirectory}};
}

QJsonObject listTool(const QJsonObject&)
{
    QJsonObject result;
    result[QStringLiteral("kinds")] = kindList();
    return result;
}

QJsonObject renderTool(const QJsonObject& args)
{
    TransitionKind kind = TransitionKind::Crossfade;
    if (!argKind(args, "kind", kind)) {
        return errorResult(QStringLiteral("kind is required (name or TransitionKind value)"));
    }
    const double progress = argDouble(args, "progress", 0.5);
    const int width = argInt(args, "width", g_state->width);
    const int height = argInt(args, "height", g_state->height);
    const int frameIndex = argInt(args, "frameIndex", g_state->frameIndex);
    const double fps = argDouble(args, "fps", g_state->fps);

    const FrameRender render = renderKind(kind, progress, width, height, frameIndex, fps);
    if (!render.ok) return errorResult(QStringLiteral("render failed"));

    QJsonArray content;
    content.append(textContent(QStringLiteral("%1 progress %2  A %3pct  B %4pct  mixed %5pct  seam %6")
                                   .arg(kindLabel(kind))
                                   .arg(progress, 0, 'f', 3)
                                   .arg(render.stats.leftFraction * 100.0, 0, 'f', 1)
                                   .arg(render.stats.rightFraction * 100.0, 0, 'f', 1)
                                   .arg(render.stats.mixedFraction * 100.0, 0, 'f', 1)
                                   .arg(render.stats.seamColumn)));
    content.append(imageContent(QStringLiteral("output"), render.result.image));
    content.append(imageContent(QStringLiteral("inputA"), render.left));
    content.append(imageContent(QStringLiteral("inputB"), render.right));

    QJsonObject result = frameRenderJson(kind, progress, render);
    result[QStringLiteral("mcpContent")] = content;
    return result;
}

QJsonObject stripTool(const QJsonObject& args)
{
    TransitionKind kind = TransitionKind::Crossfade;
    if (!argKind(args, "kind", kind)) {
        return errorResult(QStringLiteral("kind is required (name or TransitionKind value)"));
    }
    const int width = argInt(args, "width", g_state->width);
    const int height = argInt(args, "height", g_state->height);
    const int frameIndex = argInt(args, "frameIndex", g_state->frameIndex);
    const double fps = argDouble(args, "fps", g_state->fps);
    const double first = argDouble(args, "firstProgress", 0.0);
    const double last = argDouble(args, "lastProgress", 1.0);
    const std::vector<double> steps =
        progressSteps(argInt(args, "steps", 5), first, last);

    std::vector<Image> frames;
    QJsonArray samples;
    for (const double progress : steps) {
        const FrameRender render =
            renderKind(kind, progress, width, height, frameIndex, fps);
        if (!render.ok) return errorResult(QStringLiteral("render failed"));
        frames.push_back(render.result.image);
        QJsonObject entry = frameRenderJson(kind, progress, render);
        entry.remove(QStringLiteral("name"));
        samples.append(entry);
    }

    const int columns = std::max(1, argInt(args, "columns", static_cast<int>(steps.size())));
    const int scale = std::max(1, argInt(args, "scale", 2));
    const Image sheet = ArtifactTestTransitionRenderer::makeContactSheet(frames, columns, scale);

    QJsonArray content;
    content.append(textContent(QStringLiteral("%1 strip %2 step(s) from %3 to %4")
                                   .arg(kindLabel(kind))
                                   .arg(static_cast<int>(steps.size()))
                                   .arg(first, 0, 'f', 3)
                                   .arg(last, 0, 'f', 3)));
    content.append(imageContent(QStringLiteral("contactSheet"), sheet));

    QJsonObject result;
    result[QStringLiteral("kind")] = kindLabel(kind);
    result[QStringLiteral("frameCount")] = static_cast<int>(frames.size());
    result[QStringLiteral("sheetWidth")] = sheet.width;
    result[QStringLiteral("sheetHeight")] = sheet.height;
    result[QStringLiteral("frames")] = samples;
    result[QStringLiteral("mcpContent")] = content;
    return result;
}

QJsonObject gridTool(const QJsonObject& args)
{
    const double progress = argDouble(args, "progress", 0.5);
    const int width = argInt(args, "width", g_state->width);
    const int height = argInt(args, "height", g_state->height);
    const int columns = std::max(1, argInt(args, "columns", 4));
    const int scale = std::max(1, argInt(args, "scale", 2));

    std::vector<TransitionKind> kinds;
    const QJsonValue requested = args.value(QStringLiteral("kinds"));
    if (requested.isArray()) {
        for (const QJsonValue value : requested.toArray()) {
            TransitionKind kind = TransitionKind::Crossfade;
            if (!resolveKind(value.toString(), kind)) {
                return errorResult(QStringLiteral("unknown kind in kinds: ") +
                                   value.toString());
            }
            kinds.push_back(kind);
        }
    } else {
        kinds = ArtifactTestTransitionBridge::concreteKinds();
    }
    if (kinds.empty()) return errorResult(QStringLiteral("kinds must not be empty"));

    std::vector<Image> frames;
    QJsonArray samples;
    for (const TransitionKind kind : kinds) {
        const FrameRender render =
            renderKind(kind, progress, width, height, g_state->frameIndex, g_state->fps);
        if (!render.ok) return errorResult(QStringLiteral("render failed"));
        frames.push_back(render.result.image);
        samples.append(frameRenderJson(kind, progress, render));
    }

    const Image sheet = ArtifactTestTransitionRenderer::makeContactSheet(frames, columns, scale);

    QJsonArray content;
    content.append(textContent(QStringLiteral("%1 kind(s) at progress %2")
                                   .arg(static_cast<int>(kinds.size()))
                                   .arg(progress, 0, 'f', 3)));
    content.append(imageContent(QStringLiteral("contactSheet"), sheet));

    QJsonObject result;
    result[QStringLiteral("progress")] = progress;
    result[QStringLiteral("columns")] = columns;
    result[QStringLiteral("sheetWidth")] = sheet.width;
    result[QStringLiteral("sheetHeight")] = sheet.height;
    result[QStringLiteral("transitions")] = samples;
    result[QStringLiteral("mcpContent")] = content;
    return result;
}

QJsonObject diffTool(const QJsonObject& args)
{
    TransitionKind kind = TransitionKind::Crossfade;
    if (!argKind(args, "kind", kind)) {
        return errorResult(QStringLiteral("kind is required (name or TransitionKind value)"));
    }
    const int width = argInt(args, "width", g_state->width);
    const int height = argInt(args, "height", g_state->height);
    const double progressA = argDouble(args, "progressA", 0.0);
    const double progressB = argDouble(args, "progressB", 1.0);

    const FrameRender a =
        renderKind(kind, progressA, width, height, g_state->frameIndex, g_state->fps);
    const FrameRender b =
        renderKind(kind, progressB, width, height, g_state->frameIndex, g_state->fps);
    if (!a.ok || !b.ok) return errorResult(QStringLiteral("render failed"));

    const auto difference =
        ArtifactTestTransitionRenderer::difference(a.result.image, b.result.image);

    QJsonArray content;
    content.append(textContent(
        QStringLiteral("changed %1 pixels, bounds %2,%3 %4x%5")
            .arg(difference.changedPixels)
            .arg(difference.minX)
            .arg(difference.minY)
            .arg(difference.width())
            .arg(difference.height())));
    content.append(imageContent(QStringLiteral("mask"), difference.mask));

    QJsonObject result;
    result[QStringLiteral("kind")] = kindLabel(kind);
    result[QStringLiteral("progressA")] = progressA;
    result[QStringLiteral("progressB")] = progressB;
    result[QStringLiteral("changedPixels")] = difference.changedPixels;
    result[QStringLiteral("bounds")] =
        QJsonArray{difference.minX, difference.minY, difference.width(),
                   difference.height()};
    result[QStringLiteral("statsA")] = statsToJson(a.stats);
    result[QStringLiteral("statsB")] = statsToJson(b.stats);
    result[QStringLiteral("mcpContent")] = content;
    return result;
}

QJsonObject captureWriteTool(const QJsonObject& args)
{
    QString directory = argString(args, "directory", g_state->captureDirectory);
    if (directory.isEmpty()) directory = g_state->captureDirectory;
    if (directory.isEmpty()) return errorResult(QStringLiteral("directory is required"));
    QDir().mkpath(directory);

    const int width = argInt(args, "width", g_state->width);
    const int height = argInt(args, "height", g_state->height);
    const int steps = argInt(args, "steps", 5);

    std::vector<TransitionKind> kinds;
    const QString requested = argString(args, "kind", QStringLiteral("all"));
    if (requested.compare(QStringLiteral("all"), Qt::CaseInsensitive) == 0) {
        kinds = ArtifactTestTransitionBridge::concreteKinds();
    } else {
        TransitionKind kind = TransitionKind::Crossfade;
        if (!resolveKind(requested, kind)) {
            return errorResult(QStringLiteral("unknown kind: ") + requested);
        }
        kinds.push_back(kind);
    }

    const std::vector<double> progressValues = progressSteps(steps, 0.0, 1.0);
    QJsonArray written;
    QJsonArray sheets;
    int failures = 0;
    for (const TransitionKind kind : kinds) {
        std::vector<Image> frames;
        for (const double progress : progressValues) {
            const FrameRender render = renderKind(kind, progress, width, height,
                                                  g_state->frameIndex, g_state->fps);
            if (!render.ok) {
                ++failures;
                continue;
            }
            const QString path = directory + QStringLiteral("/transition_") +
                                 kindLabel(kind) + QStringLiteral("_p") +
                                 progressStem(progress) + QStringLiteral(".png");
            if (!ArtifactTestTransitionRenderer::toQImage(render.result.image)
                     .save(path, "PNG")) {
                ++failures;
            } else {
                written.append(path);
            }
            frames.push_back(render.result.image);
        }
        const QString sheetPath = directory + QStringLiteral("/transition_") +
                                  kindLabel(kind) + QStringLiteral("_sheet.png");
        if (!ArtifactTestTransitionRenderer::toQImage(
                 ArtifactTestTransitionRenderer::makeContactSheet(
                     frames, static_cast<int>(progressValues.size()), 2))
                 .save(sheetPath, "PNG")) {
            ++failures;
        } else {
            sheets.append(sheetPath);
        }
    }

    QJsonObject result;
    result[QStringLiteral("directory")] = directory;
    result[QStringLiteral("written")] = written;
    result[QStringLiteral("sheets")] = sheets;
    result[QStringLiteral("failures")] = failures;
    return result;
}

QJsonObject captureDirectoryTool(const QJsonObject& args)
{
    const QString directory = argString(args, "directory", g_state->captureDirectory);
    if (!directory.isEmpty()) g_state->captureDirectory = directory;
    QJsonObject result;
    result[QStringLiteral("captureDirectory")] = g_state->captureDirectory;
    return result;
}

// ---------------------------------------------------------------------------
// self check
// ---------------------------------------------------------------------------

int runSelfCheck()
{
    ensureInputs(g_state->width, g_state->height);
    const QString directory = g_state->captureDirectory;
    QDir().mkpath(directory);
    const std::vector<double> progressValues = progressSteps(5, 0.0, 1.0);

    int failures = 0;
    int kindCount = 0;
    QJsonArray reportTransitions;
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        const QString name = kindLabel(kind);
        std::vector<Image> frames;
        QJsonArray steps;
        Image previous;
        bool kindFailed = false;
        for (const double progress : progressValues) {
            const FrameRender first =
                renderKind(kind, progress, g_state->width, g_state->height,
                           g_state->frameIndex, g_state->fps);
            const FrameRender second =
                renderKind(kind, progress, g_state->width, g_state->height,
                           g_state->frameIndex, g_state->fps);
            if (!first.ok) {
                std::printf("FAIL %s: no output at progress %.2f\n", qPrintable(name),
                            progress);
                kindFailed = true;
                break;
            }
            if (!(first.result.image == second.result.image)) {
                std::printf("FAIL %s: non-deterministic at progress %.2f\n",
                            qPrintable(name), progress);
                kindFailed = true;
                break;
            }
            if (first.result.image.width != g_state->width ||
                first.result.image.height != g_state->height) {
                std::printf("FAIL %s: output size %dx%d, expected %dx%d\n",
                            qPrintable(name), first.result.image.width,
                            first.result.image.height, g_state->width,
                            g_state->height);
                kindFailed = true;
                break;
            }
            if (!ArtifactTestTransitionRenderer::alphaEquals(first.result.image, 255)) {
                std::printf("FAIL %s: alpha is not preserved at progress %.2f\n",
                            qPrintable(name), progress);
                kindFailed = true;
                break;
            }
            if (progress == 0.75) {
                const auto fromLeft = ArtifactTestTransitionRenderer::difference(
                    first.result.image, g_state->inputA);
                const auto fromRight = ArtifactTestTransitionRenderer::difference(
                    first.result.image, g_state->inputB);
                if (fromLeft.changedPixels <= 0 || fromRight.changedPixels <= 0) {
                    std::printf("FAIL %s: output at 0.75 equals an input\n",
                                qPrintable(name));
                    kindFailed = true;
                    break;
                }
            }
            const int changedFromPrevious =
                previous.empty()
                    ? 0
                    : ArtifactTestTransitionRenderer::difference(previous,
                                                                 first.result.image)
                          .changedPixels;
            steps.append(QJsonObject{
                {QStringLiteral("progress"), progress},
                {QStringLiteral("stats"), statsToJson(first.stats)},
                {QStringLiteral("changedFromPrevious"), changedFromPrevious}});
            frames.push_back(first.result.image);
            previous = first.result.image;
        }
        if (kindFailed) {
            ++failures;
            continue;
        }

        const QString sheetPath = directory + QStringLiteral("/selfcheck_") + name +
                                  QStringLiteral("_sheet.png");
        if (!ArtifactTestTransitionRenderer::toQImage(
                 ArtifactTestTransitionRenderer::makeContactSheet(
                     frames, static_cast<int>(progressValues.size()), 2))
                 .save(sheetPath, "PNG")) {
            std::printf("FAIL %s: could not write %s\n", qPrintable(name),
                        qPrintable(sheetPath));
            ++failures;
            continue;
        }

        reportTransitions.append(QJsonObject{
            {QStringLiteral("name"), name},
            {QStringLiteral("kind"), ArtifactTestTransitionBridge::kindValue(kind)},
            {QStringLiteral("steps"), steps}});
        std::printf("OK   %-16s steps %d  %s\n", qPrintable(name),
                    static_cast<int>(frames.size()), qPrintable(sheetPath));
        ++kindCount;
    }

    std::vector<Image> gridFrames;
    for (const TransitionKind kind : ArtifactTestTransitionBridge::concreteKinds()) {
        const FrameRender render =
            renderKind(kind, 0.5, g_state->width, g_state->height,
                       g_state->frameIndex, g_state->fps);
        if (render.ok) gridFrames.push_back(render.result.image);
    }
    const QString gridPath = directory + QStringLiteral("/selfcheck_grid_p050.png");
    if (!ArtifactTestTransitionRenderer::toQImage(
             ArtifactTestTransitionRenderer::makeContactSheet(gridFrames, 4, 2))
             .save(gridPath, "PNG")) {
        std::printf("FAIL could not write %s\n", qPrintable(gridPath));
        ++failures;
    }

    const QJsonObject report{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("width"), g_state->width},
        {QStringLiteral("height"), g_state->height},
        {QStringLiteral("transitions"), reportTransitions}};
    QFile reportFile(directory + QStringLiteral("/transition_report.json"));
    if (reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        reportFile.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
        reportFile.close();
    } else {
        std::printf("FAIL could not write the behaviour report\n");
        ++failures;
    }

    std::printf("Transition MCP self-check: %d kind(s), %d failure(s), artifacts in %s\n",
                kindCount, failures, qPrintable(directory));
    return failures == 0 &&
                   kindCount == static_cast<int>(ArtifactTestTransitionBridge::concreteKinds().size())
               ? 0
               : 1;
}

void printUsage()
{
    std::printf(
        "ArtifactTransitionMcpTest\n"
        "  --self-check                 headless render smoke run\n"
        "  --mcp-stdio                  serve the transition.* tools over stdio MCP\n"
        "  --width N --height N         diagnostic image size (default 96x64)\n"
        "  --capture-dir DIR            where PNG artifacts are written\n"
        "  --list-tools                 print the registered tool names\n");
}

} // namespace

int main(int argc, char** argv)
{
    // QCoreApplication, not QApplication: the host never creates a widget, so it
    // must not depend on a Qt platform plugin (a missing plugin would block on a
    // modal error dialog instead of failing fast).
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactTransitionMcpTest"));
    QCoreApplication::setOrganizationName(QStringLiteral("ArtifactStudio"));

    State state;
    state.captureDirectory =
        qEnvironmentVariable("ARTIFACT_TRANSITION_CAPTURE_DIR", QString());
    g_state = &state;

    bool mcpStdio = false;
    bool selfCheck = false;
    bool listTools = false;
    bool showHelp = false;
    for (int index = 1; index < argc; ++index) {
        QString argument = QString::fromLocal8Bit(argv[index]);
        QString inlineValue;
        const int equals = argument.indexOf(QChar('='));
        if (equals > 0) {
            inlineValue = argument.mid(equals + 1);
            argument = argument.left(equals);
        }
        const auto value = [&](QString* out) {
            if (!inlineValue.isEmpty()) {
                *out = inlineValue;
                return true;
            }
            if (index + 1 < argc) {
                *out = QString::fromLocal8Bit(argv[++index]);
                return true;
            }
            return false;
        };
        QString raw;
        if (argument == QStringLiteral("--mcp-stdio")) {
            mcpStdio = true;
        } else if (argument == QStringLiteral("--self-check")) {
            selfCheck = true;
        } else if (argument == QStringLiteral("--list-tools")) {
            listTools = true;
        } else if (argument == QStringLiteral("--help") ||
                   argument == QStringLiteral("-h")) {
            showHelp = true;
        } else if (argument == QStringLiteral("--width")) {
            if (value(&raw)) state.width = raw.toInt();
        } else if (argument == QStringLiteral("--height")) {
            if (value(&raw)) state.height = raw.toInt();
        } else if (argument == QStringLiteral("--capture-dir")) {
            if (value(&raw)) state.captureDirectory = raw;
        }
    }
    if (state.captureDirectory.isEmpty()) {
        state.captureDirectory = QDir::tempPath() + QStringLiteral("/artifact-transitions");
    }
    if (state.width <= 0) state.width = 96;
    if (state.height <= 0) state.height = 64;

    ensureInputs(state.width, state.height);

    if (showHelp) {
        printUsage();
        return 0;
    }

    if (listTools) {
        for (const char* name : {"transition.describe", "transition.list",
                                 "transition.render", "transition.strip",
                                 "transition.grid", "transition.diff",
                                 "transition.capture.write",
                                 "transition.capture.directory"}) {
            std::printf("%s\n", name);
        }
        return 0;
    }

    if (selfCheck || !mcpStdio) {
        return runSelfCheck();
    }

    ArtifactTestMcpServer server(QStringLiteral("artifact-transition-mcp-test"),
                                 QStringLiteral("1.0.0"));

    const auto objectSchema = [](const char* properties, const char* required) {
        return QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"),
             QJsonDocument::fromJson(properties).object()},
            {QStringLiteral("required"), QJsonDocument::fromJson(required).array()}};
    };

    bool registered = true;
    registered &= server.registerTool(
        QStringLiteral("transition.describe"),
        QStringLiteral("Report the transition lab capabilities: kinds, image size, "
                       "capture directory and the A/B input contract."),
        objectSchema("{}", "[]"),
        [](const QJsonObject& args) { return describeTool(args); });
    registered &= server.registerTool(
        QStringLiteral("transition.list"),
        QStringLiteral("List every concrete TransitionKind with its value and "
                       "factory registration state."),
        objectSchema("{}", "[]"),
        [](const QJsonObject& args) { return listTool(args); });
    registered &= server.registerTool(
        QStringLiteral("transition.render"),
        QStringLiteral("Render one transition at a progress and return the output, "
                       "both inputs and per-frame source statistics."),
        objectSchema(
            R"({"kind":{"type":"string"},"progress":{"type":"number"},"width":{"type":"integer"},"height":{"type":"integer"},"frameIndex":{"type":"integer"},"fps":{"type":"number"}})",
            R"(["kind"])"),
        [](const QJsonObject& args) { return renderTool(args); });
    registered &= server.registerTool(
        QStringLiteral("transition.strip"),
        QStringLiteral("Render a progress run of one transition and return a contact "
                       "sheet plus per-step statistics."),
        objectSchema(
            R"({"kind":{"type":"string"},"steps":{"type":"integer"},"firstProgress":{"type":"number"},"lastProgress":{"type":"number"},"columns":{"type":"integer"},"scale":{"type":"integer"},"width":{"type":"integer"},"height":{"type":"integer"}})",
            R"(["kind"])"),
        [](const QJsonObject& args) { return stripTool(args); });
    registered &= server.registerTool(
        QStringLiteral("transition.grid"),
        QStringLiteral("Render several kinds at one progress into a single contact "
                       "sheet for side-by-side comparison."),
        objectSchema(
            R"({"progress":{"type":"number"},"kinds":{"type":"array","items":{"type":"string"}},"columns":{"type":"integer"},"scale":{"type":"integer"},"width":{"type":"integer"},"height":{"type":"integer"}})",
            "[]"),
        [](const QJsonObject& args) { return gridTool(args); });
    registered &= server.registerTool(
        QStringLiteral("transition.diff"),
        QStringLiteral("Difference mask and changed pixel count between two "
                       "progresses of one transition."),
        objectSchema(
            R"({"kind":{"type":"string"},"progressA":{"type":"number"},"progressB":{"type":"number"},"width":{"type":"integer"},"height":{"type":"integer"}})",
            R"(["kind"])"),
        [](const QJsonObject& args) { return diffTool(args); });
    registered &= server.registerTool(
        QStringLiteral("transition.capture.write"),
        QStringLiteral("Render a progress run and write PNG frames plus a contact "
                       "sheet to disk (kind = \"all\" for every transition)."),
        objectSchema(
            R"({"kind":{"type":"string"},"directory":{"type":"string"},"steps":{"type":"integer"},"width":{"type":"integer"},"height":{"type":"integer"}})",
            "[]"),
        [](const QJsonObject& args) { return captureWriteTool(args); });
    registered &= server.registerTool(
        QStringLiteral("transition.capture.directory"),
        QStringLiteral("Get or set the default directory for capture.write."),
        objectSchema(R"({"directory":{"type":"string"}})", "[]"),
        [](const QJsonObject& args) { return captureDirectoryTool(args); });

    if (!registered) {
        std::printf("failed to register the transition test tools\n");
        return 1;
    }
    if (!server.start()) {
        std::printf("failed to start the MCP stdio server\n");
        return 1;
    }
    return app.exec();
}
