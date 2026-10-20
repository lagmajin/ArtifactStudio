// Standalone MCP-hosted FxStudio FX animation test driver.
//
// This is the "operate it over MCP" half of the FX animation test work: the
// same render path as ArtifactFxStudioRenderContractTest, exposed as a stdio
// MCP server so an agent or a script can build a session, sample frames, render
// them, diff them, and get the images back as MCP image content.
//
//   ArtifactFxStudioMcpTest --self-check
//       Deterministic headless smoke run (used by CTest).
//   ArtifactFxStudioMcpTest --mcp-stdio [--width N --height N]
//       Serves tools over newline-delimited JSON-RPC on stdio. No window or
//       platform plugin is required: the host only uses QtCore, QImage and the
//       FxStudio model.
//
// No GPU device and no media file are needed; the FX input is a synthetic
// diagnostic image produced by ArtifactTestFxCueRenderer.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../ArtifactTestFxCueRenderer.h"
#include "../ArtifactTestImageArtifacts.h"
#include "../ArtifactTestMcpServer.h"

import Artifact.FxStudio.EventTrack;
import Artifact.FxStudio.PresetCatalog;
import Artifact.FxStudio.Sequence;
import Artifact.FxStudio.Session;
import Artifact.FxStudio.ViewportModel;
import Core.ArtifactString;

#include "../ArtifactTestFxCueBridge.h"

namespace {

namespace Fx = Artifact::FxStudio;

using ArtifactTestFxCueRenderer::Background;
using ArtifactTestFxCueRenderer::Comparison;
using ArtifactTestFxCueRenderer::Cue;
using ArtifactTestFxCueRenderer::Effect;
using ArtifactTestFxCueRenderer::Image;

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

struct State {
    Fx::Session session;
    Image inputA;
    Image inputB;
    int width = 128;
    int height = 96;
    QString captureDirectory;
    QString lastRenderSummary;
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
    g_state->inputA = ArtifactTestFxCueRenderer::makeDiagnosticImage(width, height);
    g_state->inputB = ArtifactTestFxCueRenderer::makeDiagnosticImage(width, height, true);
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

bool argBool(const QJsonObject& args, const char* key, const bool fallback)
{
    const QJsonValue value = args.value(QString::fromUtf8(key));
    return value.isBool() ? value.toBool() : fallback;
}

QString argString(const QJsonObject& args, const char* key,
                  const QString& fallback = QString())
{
    const QJsonValue value = args.value(QString::fromUtf8(key));
    return value.isString() ? value.toString() : fallback;
}

bool hasArg(const QJsonObject& args, const char* key)
{
    return args.contains(QString::fromUtf8(key));
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
        name, ArtifactTestFxCueRenderer::toQImage(image));
}

QJsonArray effectNames(const std::vector<Effect>& effects)
{
    QJsonArray names;
    for (const Effect effect : effects) {
        names.append(QString::fromUtf8(ArtifactTestFxCueRenderer::effectName(effect)));
    }
    return names;
}

// ---------------------------------------------------------------------------
// rendering
// ---------------------------------------------------------------------------

struct FrameRender {
    bool ok = false;
    Image input;
    Image processed;
    Image output;
    std::vector<Cue> cues;
    std::vector<Effect> applied;
};

FrameRender renderAt(const int width, const int height, const std::int64_t frame)
{
    FrameRender render;
    if (width <= 0 || height <= 0) return render;
    ensureInputs(width, height);

    const Fx::ViewportModel& viewport = g_state->session.viewport();
    render.cues = ArtifactTestFxCueBridge::visibleCuesAt(
        g_state->session.eventTrack(), viewport, frame);

    render.input = g_state->inputA;
    render.processed = render.input;
    ArtifactTestFxCueRenderer::ApplyContext context;
    context.revealTarget = &g_state->inputB;
    ArtifactTestFxCueRenderer::applyCues(render.processed, render.input, render.cues,
                                         &render.applied, context);

    ArtifactTestFxCueRenderer::FrameOptions options;
    options.comparison = ArtifactTestFxCueBridge::toComparison(viewport.comparisonMode());
    options.splitPosition = viewport.splitPosition();
    options.background = ArtifactTestFxCueBridge::toBackground(viewport.background());
    render.output = ArtifactTestFxCueRenderer::renderFrame(render.processed, render.input,
                                                           options);
    render.ok = true;
    return render;
}

std::vector<Cue> visibleCuesForFrame(const std::int64_t frame)
{
    return ArtifactTestFxCueBridge::visibleCuesAt(
        g_state->session.eventTrack(), g_state->session.viewport(), frame);
}

// ---------------------------------------------------------------------------
// tool handlers
// ---------------------------------------------------------------------------

QJsonObject describeTool(const QJsonObject&)
{
    QJsonArray cueKinds;
    for (int index = 0; index < ArtifactTestFxCueRenderer::kEffectCount; ++index) {
        cueKinds.append(QString::fromUtf8(ArtifactTestFxCueRenderer::effectName(
            static_cast<Effect>(index))));
    }
    QJsonArray qualities;
    for (const Fx::PreviewQuality quality : {Fx::PreviewQuality::Draft,
                                             Fx::PreviewQuality::Interactive,
                                             Fx::PreviewQuality::Final}) {
        qualities.append(QString::fromUtf8(ArtifactTestFxCueBridge::qualityName(quality)));
    }
    return QJsonObject{
        {QStringLiteral("server"), QStringLiteral("artifact-fx-studio-mcp-test")},
        {QStringLiteral("schemaVersion"), g_state->session.schemaVersion()},
        {QStringLiteral("typeName"), g_state->session.typeName()},
        {QStringLiteral("cueKinds"), cueKinds},
        {QStringLiteral("comparisons"),
         QJsonArray{QStringLiteral("Processed"), QStringLiteral("Original"),
                    QStringLiteral("Split")}},
        {QStringLiteral("backgrounds"),
         QJsonArray{QStringLiteral("Transparent"), QStringLiteral("Checkerboard"),
                    QStringLiteral("Dark"), QStringLiteral("Light")}},
        {QStringLiteral("previewQualities"), qualities},
        {QStringLiteral("imageSize"),
         QJsonObject{{QStringLiteral("width"), g_state->width},
                     {QStringLiteral("height"), g_state->height}}},
        {QStringLiteral("input"),
         QStringLiteral("synthetic diagnostic image; secondInput controls the "
                        "A/B input used by Split comparison and MaskProgress reveal")},
        {QStringLiteral("freeze"),
         QStringLiteral("Freeze is temporal: fx.frame.strip holds the previously "
                        "emitted frame while the cue is active")}};
}

QJsonObject presetsListTool(const QJsonObject&)
{
    QJsonArray presets;
    for (const Fx::PresetDescriptor& descriptor : Fx::PresetCatalog::descriptors()) {
        Fx::Sequence sequence;
        bool haveSequence = false;
        const auto created = Fx::PresetCatalog::create(descriptor.id);
        if (created.has_value()) {
            sequence = *created;
            haveSequence = true;
        }
        QJsonArray cues;
        if (haveSequence) {
            for (const Fx::Cue& cue : sequence.cues) {
                cues.append(QJsonObject{
                    {QStringLiteral("kind"),
                     QString::fromUtf8(ArtifactTestFxCueBridge::cueKindName(cue.kind))},
                    {QStringLiteral("curve"),
                     QString::fromUtf8(ArtifactTestFxCueBridge::cueCurveName(cue.curve))},
                    {QStringLiteral("start"), cue.start},
                    {QStringLiteral("duration"), cue.duration},
                    {QStringLiteral("strength"), cue.strength},
                    {QStringLiteral("variation"), cue.variation},
                    {QStringLiteral("seed"), static_cast<double>(cue.seed)},
                    {QStringLiteral("enabled"), cue.enabled}});
            }
        }
        presets.append(QJsonObject{
            {QStringLiteral("id"),
             QString::fromStdString(ArtifactCore::toStdString(descriptor.id))},
            {QStringLiteral("name"),
             QString::fromStdString(ArtifactCore::toStdString(descriptor.name))},
            {QStringLiteral("family"),
             QString::fromUtf8(ArtifactTestFxCueBridge::familyName(descriptor.family))},
            {QStringLiteral("description"),
             QString::fromStdString(ArtifactCore::toStdString(descriptor.description))},
            {QStringLiteral("durationFrames"), static_cast<double>(sequence.durationFrames)},
            {QStringLiteral("cues"), cues}});
    }
    return QJsonObject{{QStringLiteral("presets"), presets},
                       {QStringLiteral("count"), presets.size()}};
}

QJsonArray eventList()
{
    QJsonArray events;
    const Fx::Event* selected = g_state->session.selectedEvent();
    for (const Fx::Event& event : g_state->session.eventTrack().events()) {
        events.append(QJsonObject{
            {QStringLiteral("id"), static_cast<double>(event.id)},
            {QStringLiteral("startFrame"), static_cast<double>(event.startFrame)},
            {QStringLiteral("family"),
             QString::fromUtf8(ArtifactTestFxCueBridge::familyName(event.sequence.family))},
            {QStringLiteral("durationFrames"),
             static_cast<double>(event.sequence.durationFrames)},
            {QStringLiteral("strength"), event.sequence.strength},
            {QStringLiteral("cueCount"), static_cast<int>(event.sequence.cues.size())},
            {QStringLiteral("enabled"), event.enabled},
            {QStringLiteral("selected"), selected != nullptr && selected->id == event.id}});
    }
    return events;
}

QJsonObject viewportState()
{
    const Fx::ViewportModel& viewport = g_state->session.viewport();
    QJsonArray bypassed;
    for (const Fx::CueKind cue : viewport.bypassedCues()) {
        bypassed.append(
            QString::fromUtf8(ArtifactTestFxCueBridge::cueKindName(cue)));
    }
    QJsonValue solo = QJsonValue(QJsonValue::Null);
    if (viewport.soloCue().has_value()) {
        solo = QString::fromUtf8(
            ArtifactTestFxCueBridge::cueKindName(*viewport.soloCue()));
    }
    return QJsonObject{
        {QStringLiteral("comparison"),
         QString::fromUtf8(ArtifactTestFxCueBridge::comparisonName(
             viewport.comparisonMode()))},
        {QStringLiteral("splitPosition"), viewport.splitPosition()},
        {QStringLiteral("quality"),
         QString::fromUtf8(ArtifactTestFxCueBridge::qualityName(viewport.quality()))},
        {QStringLiteral("background"),
         static_cast<int>(viewport.background())},
        {QStringLiteral("loopEnabled"), viewport.loopEnabled()},
        {QStringLiteral("loopFirstFrame"),
         static_cast<double>(viewport.loopFirstFrame())},
        {QStringLiteral("loopLastFrame"), static_cast<double>(viewport.loopLastFrame())},
        {QStringLiteral("soloCue"), solo},
        {QStringLiteral("bypassedCues"), bypassed}};
}

QJsonObject sessionStateTool(const QJsonObject&)
{
    return QJsonObject{
        {QStringLiteral("json"),
         QString::fromUtf8(QJsonDocument(g_state->session.toJson())
                               .toJson(QJsonDocument::Compact))},
        {QStringLiteral("events"), eventList()},
        {QStringLiteral("viewport"), viewportState()},
        {QStringLiteral("eventCount"),
         static_cast<int>(g_state->session.eventTrack().events().size())}};
}

QJsonObject sessionNewTool(const QJsonObject& args)
{
    g_state->session = Fx::Session{};
    if (hasArg(args, "presetId")) {
        const QString presetId = argString(args, "presetId");
        g_state->session.insertPreset(
            ArtifactCore::String(presetId.toStdString()), argInt(args, "startFrame", 0));
    }
    return sessionStateTool(args);
}

QJsonObject sessionLoadTool(const QJsonObject& args)
{
    const QString json = argString(args, "json");
    if (json.isEmpty()) return errorResult(QStringLiteral("json is required"));
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return errorResult(QStringLiteral("invalid session json: ") +
                           parseError.errorString());
    }
    Fx::Session candidate;
    if (!candidate.fromJson(document.object())) {
        return errorResult(QStringLiteral("session rejected the supplied json"));
    }
    g_state->session = std::move(candidate);
    QJsonObject result = sessionStateTool(args);
    result[QStringLiteral("loaded")] = true;
    return result;
}

QJsonObject sessionSaveTool(const QJsonObject&)
{
    return QJsonObject{
        {QStringLiteral("json"),
         QString::fromUtf8(QJsonDocument(g_state->session.toJson())
                               .toJson(QJsonDocument::Compact))}};
}

QJsonObject eventInsertTool(const QJsonObject& args)
{
    const QString presetId = argString(args, "presetId");
    if (presetId.isEmpty()) return errorResult(QStringLiteral("presetId is required"));
    if (Fx::PresetCatalog::find(ArtifactCore::String(presetId.toStdString())) == nullptr) {
        return errorResult(QStringLiteral("unknown presetId: ") + presetId);
    }
    const std::int64_t startFrame = argInt(args, "startFrame", 0);
    g_state->session.insertPreset(ArtifactCore::String(presetId.toStdString()),
                                  startFrame);
    QJsonObject result;
    result[QStringLiteral("events")] = eventList();
    result[QStringLiteral("eventCount")] =
        static_cast<int>(g_state->session.eventTrack().events().size());
    return result;
}

QJsonObject eventListTool(const QJsonObject&)
{
    return QJsonObject{{QStringLiteral("events"), eventList()},
                       {QStringLiteral("eventCount"),
                        static_cast<int>(g_state->session.eventTrack().events().size())}};
}

QJsonObject eventUpdateTool(const QJsonObject& args)
{
    const auto id = static_cast<Fx::EventId>(argInt(args, "id", 0));
    if (id == 0) return errorResult(QStringLiteral("id is required"));
    Fx::Event* event = g_state->session.eventTrack().find(id);
    if (event == nullptr) return errorResult(QStringLiteral("no such event"));

    Fx::Event updated = *event;
    if (hasArg(args, "startFrame"))
        updated.startFrame = argInt(args, "startFrame", 0);
    if (hasArg(args, "durationFrames"))
        updated.sequence.durationFrames = argInt(args, "durationFrames", 1);
    if (hasArg(args, "strength"))
        updated.sequence.strength = static_cast<float>(argDouble(args, "strength", 1.0));
    if (hasArg(args, "enabled"))
        updated.enabled = argBool(args, "enabled", true);
    if (!g_state->session.eventTrack().update(updated)) {
        return errorResult(QStringLiteral("event update rejected"));
    }
    return eventListTool(args);
}

QJsonObject eventRemoveTool(const QJsonObject& args)
{
    const auto id = static_cast<Fx::EventId>(argInt(args, "id", 0));
    if (id == 0) return errorResult(QStringLiteral("id is required"));
    if (!g_state->session.eventTrack().remove(id)) {
        return errorResult(QStringLiteral("no such event"));
    }
    return eventListTool(args);
}

QJsonObject eventSelectTool(const QJsonObject& args)
{
    const auto id = static_cast<Fx::EventId>(argInt(args, "id", 0));
    if (id == 0) {
        g_state->session.clearSelection();
        return eventListTool(args);
    }
    if (!g_state->session.selectEvent(id)) {
        return errorResult(QStringLiteral("no such event"));
    }
    return eventListTool(args);
}

QJsonObject viewportSetTool(const QJsonObject& args)
{
    Fx::ViewportModel& viewport = g_state->session.viewport();
    const QString comparison = argString(args, "comparison");
    if (!comparison.isEmpty()) {
        if (comparison == QStringLiteral("Processed"))
            viewport.setComparisonMode(Fx::ComparisonMode::Processed);
        else if (comparison == QStringLiteral("Original"))
            viewport.setComparisonMode(Fx::ComparisonMode::Original);
        else if (comparison == QStringLiteral("Split"))
            viewport.setComparisonMode(Fx::ComparisonMode::Split);
        else
            return errorResult(QStringLiteral("unknown comparison: ") + comparison);
    }
    if (hasArg(args, "splitPosition")) {
        viewport.setSplitPosition(
            std::clamp(static_cast<float>(argDouble(args, "splitPosition", 0.5)), 0.0f,
                       1.0f));
    }
    const QString quality = argString(args, "quality");
    if (!quality.isEmpty()) {
        if (quality == QStringLiteral("Draft"))
            viewport.setQuality(Fx::PreviewQuality::Draft);
        else if (quality == QStringLiteral("Interactive"))
            viewport.setQuality(Fx::PreviewQuality::Interactive);
        else if (quality == QStringLiteral("Final"))
            viewport.setQuality(Fx::PreviewQuality::Final);
        else
            return errorResult(QStringLiteral("unknown quality: ") + quality);
    }
    const QString background = argString(args, "background");
    if (!background.isEmpty()) {
        if (background == QStringLiteral("Transparent"))
            viewport.setBackground(Fx::PreviewBackground::Transparent);
        else if (background == QStringLiteral("Checkerboard"))
            viewport.setBackground(Fx::PreviewBackground::Checkerboard);
        else if (background == QStringLiteral("Dark"))
            viewport.setBackground(Fx::PreviewBackground::Dark);
        else if (background == QStringLiteral("Light"))
            viewport.setBackground(Fx::PreviewBackground::Light);
        else
            return errorResult(QStringLiteral("unknown background: ") + background);
    }
    if (hasArg(args, "loopEnabled"))
        viewport.setLoopEnabled(argBool(args, "loopEnabled", true));
    if (hasArg(args, "loopFirstFrame") && hasArg(args, "loopLastFrame")) {
        if (!viewport.setLoopRange(argInt(args, "loopFirstFrame", 0),
                                   argInt(args, "loopLastFrame", 0))) {
            return errorResult(QStringLiteral("loopLastFrame must be >= loopFirstFrame"));
        }
    }
    if (hasArg(args, "useEventLoop")) {
        const auto id = static_cast<Fx::EventId>(argInt(args, "useEventLoop", 0));
        if (!viewport.useEventLoop(g_state->session.eventTrack(), id)) {
            return errorResult(QStringLiteral("no such event for useEventLoop"));
        }
    }
    QJsonObject result;
    result[QStringLiteral("viewport")] = viewportState();
    return result;
}

QJsonObject cueBypassTool(const QJsonObject& args)
{
    const int index = argInt(args, "cue", -1);
    if (index < 0 || index >= ArtifactTestFxCueRenderer::kEffectCount) {
        return errorResult(QStringLiteral("cue index out of range"));
    }
    const auto kind = static_cast<Fx::CueKind>(index);
    if (!argBool(args, "bypassed", true)) {
        g_state->session.viewport().setCueBypassed(kind, false);
    } else {
        g_state->session.viewport().setCueBypassed(kind, true);
    }
    QJsonObject result;
    result[QStringLiteral("viewport")] = viewportState();
    return result;
}

QJsonObject cueSoloTool(const QJsonObject& args)
{
    const int index = argInt(args, "cue", -1);
    if (index < 0) {
        g_state->session.viewport().setSoloCue(ArtifactCore::Optional<Fx::CueKind>{});
    } else if (index >= ArtifactTestFxCueRenderer::kEffectCount) {
        return errorResult(QStringLiteral("cue index out of range"));
    } else {
        g_state->session.viewport().setSoloCue(static_cast<Fx::CueKind>(index));
    }
    QJsonObject result;
    result[QStringLiteral("viewport")] = viewportState();
    return result;
}

QJsonObject frameCuesTool(const QJsonObject& args)
{
    const std::int64_t frame = argInt(args, "frame", 0);
    const std::vector<Cue> cues = visibleCuesForFrame(frame);
    QJsonArray samples;
    QStringList lines;
    for (const Cue& cue : cues) {
        const QString name =
            QString::fromUtf8(ArtifactTestFxCueRenderer::effectName(cue.effect));
        samples.append(QJsonObject{
            {QStringLiteral("cue"), name},
            {QStringLiteral("amplitude"), cue.amplitude},
            {QStringLiteral("progress"), cue.progress},
            {QStringLiteral("variation"), cue.variation},
            {QStringLiteral("seed"), static_cast<double>(cue.seed)}});
        lines << QStringLiteral("%1 amplitude %2 progress %3 seed %4")
                     .arg(name)
                     .arg(cue.amplitude, 0, 'f', 3)
                     .arg(cue.progress, 0, 'f', 3)
                     .arg(cue.seed);
    }
    const QString text = lines.isEmpty()
                             ? QStringLiteral("no visible cue at frame %1").arg(frame)
                             : lines.join(QChar('\n'));
    QJsonObject result{
        {QStringLiteral("frame"), static_cast<double>(frame)},
        {QStringLiteral("cues"), samples},
        {QStringLiteral("cueCount"), static_cast<int>(cues.size())},
        {QStringLiteral("text"), text},
        {QStringLiteral("mcpContent"),
         QJsonArray{textContent(QStringLiteral("frame %1\n%2").arg(frame).arg(text))}}};
    return result;
}

QJsonObject frameRenderTool(const QJsonObject& args)
{
    const std::int64_t frame = argInt(args, "frame", 0);
    const int width = argInt(args, "width", g_state->width);
    const int height = argInt(args, "height", g_state->height);
    const FrameRender render = renderAt(width, height, frame);
    if (!render.ok) return errorResult(QStringLiteral("render failed"));

    QJsonArray content;
    content.append(textContent(QStringLiteral("frame %1  cues %2  applied %3")
                                   .arg(frame)
                                   .arg(render.cues.size())
                                   .arg(effectNames(render.applied).size())));
    content.append(imageContent(QStringLiteral("output"), render.output));
    content.append(imageContent(QStringLiteral("processed"), render.processed));
    content.append(imageContent(QStringLiteral("input"), render.input));

    QJsonObject result{
        {QStringLiteral("frame"), static_cast<double>(frame)},
        {QStringLiteral("width"), render.output.width},
        {QStringLiteral("height"), render.output.height},
        {QStringLiteral("applied"), effectNames(render.applied)},
        {QStringLiteral("cueCount"), static_cast<int>(render.cues.size())},
        {QStringLiteral("viewport"), viewportState()},
        {QStringLiteral("mcpContent"), content}};
    return result;
}

QJsonObject frameStripTool(const QJsonObject& args)
{
    const int first = argInt(args, "firstFrame", 0);
    const int last = argInt(args, "lastFrame", first);
    if (last < first) return errorResult(QStringLiteral("lastFrame must be >= firstFrame"));
    const int width = argInt(args, "width", g_state->width);
    const int height = argInt(args, "height", g_state->height);
    ensureInputs(width, height);

    const Fx::ViewportModel& viewport = g_state->session.viewport();
    ArtifactTestFxCueRenderer::FrameOptions options;
    options.comparison = ArtifactTestFxCueBridge::toComparison(viewport.comparisonMode());
    options.splitPosition = viewport.splitPosition();
    options.background = ArtifactTestFxCueBridge::toBackground(viewport.background());

    const auto rendered = ArtifactTestFxCueRenderer::renderSequence(
        g_state->inputA, first, last,
        [](const std::int64_t frame) { return visibleCuesForFrame(frame); }, options,
        &g_state->inputB);

    QJsonArray frames;
    for (std::size_t index = 0; index < rendered.report.frames.size(); ++index) {
        const auto& report = rendered.report.frames[index];
        const int changed = index == 0
                                ? 0
                                : ArtifactTestFxCueRenderer::difference(
                                      rendered.frames[index - 1], rendered.frames[index])
                                      .changedPixels;
        frames.append(QJsonObject{
            {QStringLiteral("frame"), static_cast<double>(report.frame)},
            {QStringLiteral("held"), report.held},
            {QStringLiteral("activeCues"), report.activeCues},
            {QStringLiteral("applied"), effectNames(report.effects)},
            {QStringLiteral("changedFromPrevious"), changed}});
    }

    const int columns = std::max(1, argInt(args, "columns", 6));
    const int scale = std::max(1, argInt(args, "scale", 2));
    const Image sheet =
        ArtifactTestFxCueRenderer::makeContactSheet(rendered.frames, columns, scale);

    QJsonArray content;
    content.append(textContent(
        QStringLiteral("frames %1..%2  held %3  distinct %4")
            .arg(first)
            .arg(last)
            .arg(rendered.report.heldFrames)
            .arg(rendered.report.distinctFrames)));
    content.append(imageContent(QStringLiteral("contactSheet"), sheet));

    QJsonObject result{
        {QStringLiteral("firstFrame"), first},
        {QStringLiteral("lastFrame"), last},
        {QStringLiteral("frameCount"), static_cast<int>(rendered.frames.size())},
        {QStringLiteral("heldFrames"), rendered.report.heldFrames},
        {QStringLiteral("distinctFrames"), rendered.report.distinctFrames},
        {QStringLiteral("sheetWidth"), sheet.width},
        {QStringLiteral("sheetHeight"), sheet.height},
        {QStringLiteral("frames"), frames},
        {QStringLiteral("mcpContent"), content}};
    return result;
}

QJsonObject frameDiffTool(const QJsonObject& args)
{
    const int width = argInt(args, "width", g_state->width);
    const int height = argInt(args, "height", g_state->height);
    const FrameRender a = renderAt(width, height, argInt(args, "frameA", 0));
    const FrameRender b = renderAt(width, height, argInt(args, "frameB", 1));
    if (!a.ok || !b.ok) return errorResult(QStringLiteral("render failed"));

    const auto difference = ArtifactTestFxCueRenderer::difference(a.output, b.output);
    QJsonArray content;
    content.append(textContent(QStringLiteral("changed %1 pixels, bounds %2,%3 %4x%5")
                                   .arg(difference.changedPixels)
                                   .arg(difference.minX)
                                   .arg(difference.minY)
                                   .arg(difference.width())
                                   .arg(difference.height())));
    content.append(imageContent(QStringLiteral("mask"), difference.mask));

    QJsonObject result{
        {QStringLiteral("frameA"), argInt(args, "frameA", 0)},
        {QStringLiteral("frameB"), argInt(args, "frameB", 1)},
        {QStringLiteral("changedPixels"), difference.changedPixels},
        {QStringLiteral("bounds"),
         QJsonObject{{QStringLiteral("x"), difference.minX},
                     {QStringLiteral("y"), difference.minY},
                     {QStringLiteral("width"), difference.width()},
                     {QStringLiteral("height"), difference.height()}}},
        {QStringLiteral("mcpContent"), content}};
    return result;
}

QJsonObject captureWriteTool(const QJsonObject& args)
{
    QString directory = argString(args, "directory", g_state->captureDirectory);
    if (directory.isEmpty()) {
        directory = QDir::tempPath() + QStringLiteral("/artifact-fx-studio-mcp");
    }
    if (!QDir().mkpath(directory)) {
        return errorResult(QStringLiteral("cannot create ") + directory);
    }
    const int width = argInt(args, "width", g_state->width);
    const int height = argInt(args, "height", g_state->height);
    ensureInputs(width, height);

    const Fx::ViewportModel& viewport = g_state->session.viewport();
    ArtifactTestFxCueRenderer::FrameOptions options;
    options.comparison = ArtifactTestFxCueBridge::toComparison(viewport.comparisonMode());
    options.splitPosition = viewport.splitPosition();
    options.background = ArtifactTestFxCueBridge::toBackground(viewport.background());

    const int first = argInt(args, "firstFrame", 0);
    const int last = argInt(args, "lastFrame", first);
    if (last < first) return errorResult(QStringLiteral("lastFrame must be >= firstFrame"));

    const auto rendered = ArtifactTestFxCueRenderer::renderSequence(
        g_state->inputA, first, last,
        [](const std::int64_t frame) { return visibleCuesForFrame(frame); }, options,
        &g_state->inputB);

    QJsonArray written;
    int failures = 0;
    for (std::size_t index = 0; index < rendered.frames.size(); ++index) {
        const QString path = directory + QStringLiteral("/fx_%1.png")
                                 .arg(static_cast<int>(index), 4, 10, QChar('0'));
        const QImage image = ArtifactTestFxCueRenderer::toQImage(rendered.frames[index]);
        if (image.isNull() || !image.save(path, "PNG")) {
            ++failures;
        } else {
            written.append(path);
        }
    }
    const Image sheet =
        ArtifactTestFxCueRenderer::makeContactSheet(rendered.frames, 6, 2);
    const QString sheetPath = directory + QStringLiteral("/fx_sheet.png");
    if (!ArtifactTestFxCueRenderer::toQImage(sheet).save(sheetPath, "PNG")) ++failures;

    return QJsonObject{
        {QStringLiteral("directory"), directory},
        {QStringLiteral("written"), written},
        {QStringLiteral("sheet"), sheetPath},
        {QStringLiteral("heldFrames"), rendered.report.heldFrames},
        {QStringLiteral("distinctFrames"), rendered.report.distinctFrames},
        {QStringLiteral("failures"), failures}};
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

    int failures = 0;
    int presetIndex = 0;
    for (const Fx::PresetDescriptor& descriptor : Fx::PresetCatalog::descriptors()) {
        const QString id =
            QString::fromStdString(ArtifactCore::toStdString(descriptor.id));
        g_state->session = Fx::Session{};
        g_state->session.insertPreset(descriptor.id, 0);
        if (g_state->session.eventTrack().events().size() != 1) {
            std::printf("FAIL %s: insertPreset produced %d events\n", qPrintable(id),
                        static_cast<int>(g_state->session.eventTrack().events().size()));
            ++failures;
            continue;
        }
        const Fx::Event& event = g_state->session.eventTrack().events().front();
        const int last = static_cast<int>(event.sequence.durationFrames);

        const auto rendered = ArtifactTestFxCueRenderer::renderSequence(
            g_state->inputA, 0, last,
            [](const std::int64_t frame) { return visibleCuesForFrame(frame); },
            ArtifactTestFxCueRenderer::FrameOptions{}, &g_state->inputB);

        if (rendered.frames.size() != static_cast<std::size_t>(last + 1)) {
            std::printf("FAIL %s: %d frames for range 0..%d\n", qPrintable(id),
                        static_cast<int>(rendered.frames.size()), last);
            ++failures;
            continue;
        }
        if (rendered.report.distinctFrames <= 2) {
            std::printf("FAIL %s: only %d distinct frames\n", qPrintable(id),
                        rendered.report.distinctFrames);
            ++failures;
            continue;
        }
        // Every preset decays back to the untouched input by its last frame, so
        // the animation is validated by its loudest frame, not by the last one.
        int peakDifference = 0;
        for (std::size_t index = 1; index < rendered.frames.size(); ++index) {
            peakDifference = std::max(
                peakDifference,
                ArtifactTestFxCueRenderer::difference(rendered.frames.front(),
                                                      rendered.frames[index])
                    .changedPixels);
        }
        if (peakDifference <= 0) {
            std::printf("FAIL %s: no frame differs from the first\n", qPrintable(id));
            ++failures;
            continue;
        }

        const QString stem = QStringLiteral("%1_%2")
                                 .arg(presetIndex, 1, 10, QChar('0'))
                                 .arg(QString(id).replace(QChar('.'), QChar('_')));
        const QString sheetPath = directory + QStringLiteral("/selfcheck_") + stem +
                                  QStringLiteral("_sheet.png");
        const Image sheet = ArtifactTestFxCueRenderer::makeContactSheet(
            rendered.frames, 6, 2);
        if (!ArtifactTestFxCueRenderer::toQImage(sheet).save(sheetPath, "PNG")) {
            std::printf("FAIL %s: could not write %s\n", qPrintable(id),
                        qPrintable(sheetPath));
            ++failures;
            continue;
        }
        std::printf("OK   %-24s frames %2d  held %d  distinct %2d  peakdiff %5d  %s\n",
                    qPrintable(id), last + 1, rendered.report.heldFrames,
                    rendered.report.distinctFrames, peakDifference, qPrintable(sheetPath));
        ++presetIndex;
    }

    std::printf("FxStudio MCP self-check: %d preset(s), %d failure(s), artifacts in %s\n",
                presetIndex, failures, qPrintable(directory));
    return failures == 0 && presetIndex == 5 ? 0 : 1;
}

void printUsage()
{
    std::printf(
        "ArtifactFxStudioMcpTest\n"
        "  --self-check                 headless render smoke run\n"
        "  --mcp-stdio                  serve the fx.* tools over stdio MCP\n"
        "  --width N --height N         diagnostic image size (default 128x96)\n"
        "  --capture-dir DIR            where PNG artifacts are written\n"
        "  --list-tools                 print the registered tool names and exit\n");
}

} // namespace

int main(int argc, char** argv)
{
    // QCoreApplication, not QApplication: the host never creates a widget, so it
    // must not depend on a Qt platform plugin (a missing plugin would block on a
    // modal error dialog instead of failing fast).
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactFxStudioMcpTest"));
    QCoreApplication::setOrganizationName(QStringLiteral("ArtifactStudio"));

    State state;
    state.captureDirectory =
        qEnvironmentVariable("ARTIFACT_FX_STUDIO_CAPTURE_DIR", QString());
    g_state = &state;

    bool mcpStdio = false;
    bool selfCheck = false;
    bool listTools = false;
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
        } else if (argument == QStringLiteral("--width")) {
            if (value(&raw)) state.width = raw.toInt();
        } else if (argument == QStringLiteral("--height")) {
            if (value(&raw)) state.height = raw.toInt();
        } else if (argument == QStringLiteral("--capture-dir")) {
            if (value(&raw)) state.captureDirectory = raw;
        }
    }
    if (state.captureDirectory.isEmpty()) {
        state.captureDirectory = QDir::tempPath() + QStringLiteral("/artifact-fx-studio");
    }
    if (state.width <= 0) state.width = 128;
    if (state.height <= 0) state.height = 96;

    ensureInputs(state.width, state.height);
    state.session.insertPreset(ArtifactCore::String("fx.glitch.burst"), 0);

    if (listTools) {
        for (const char* name : {"fx.describe", "fx.presets.list", "fx.session.new",
                                 "fx.session.state", "fx.session.load", "fx.session.save",
                                 "fx.event.insert", "fx.event.list", "fx.event.update",
                                 "fx.event.remove", "fx.event.select", "fx.viewport.set",
                                 "fx.cue.bypass", "fx.cue.solo", "fx.frame.cues",
                                 "fx.frame.render", "fx.frame.strip", "fx.frame.diff",
                                 "fx.capture.write", "fx.capture.directory"}) {
            std::printf("%s\n", name);
        }
        return 0;
    }

    if (selfCheck || !mcpStdio) {
        return runSelfCheck();
    }

    ArtifactTestMcpServer server(QStringLiteral("artifact-fx-studio-mcp-test"),
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
        QStringLiteral("fx.describe"),
        QStringLiteral("Report the FxStudio FX animation capabilities exposed by this "
                       "test server: cue kinds, comparisons, backgrounds, image size."),
        objectSchema("{}", "[]"),
        [](const QJsonObject& args) { return describeTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.presets.list"),
        QStringLiteral("List the built-in FX animation presets with their cues."),
        objectSchema("{}", "[]"),
        [](const QJsonObject& args) { return presetsListTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.session.new"),
        QStringLiteral("Reset the session, optionally inserting one preset."),
        objectSchema(
            R"({"presetId":{"type":"string"},"startFrame":{"type":"integer"}})", "[]"),
        [](const QJsonObject& args) { return sessionNewTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.session.state"),
        QStringLiteral("Current session JSON, event list and viewport state."),
        objectSchema("{}", "[]"),
        [](const QJsonObject& args) { return sessionStateTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.session.load"),
        QStringLiteral("Replace the session from serialized session JSON."),
        objectSchema(R"({"json":{"type":"string"}})", R"(["json"])"),
        [](const QJsonObject& args) { return sessionLoadTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.session.save"),
        QStringLiteral("Serialize the current session to JSON."),
        objectSchema("{}", "[]"),
        [](const QJsonObject& args) { return sessionSaveTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.event.insert"),
        QStringLiteral("Insert a preset as an event at startFrame."),
        objectSchema(
            R"({"presetId":{"type":"string"},"startFrame":{"type":"integer"}})",
            R"(["presetId"])"),
        [](const QJsonObject& args) { return eventInsertTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.event.list"),
        QStringLiteral("List events with id, range, family and cue count."),
        objectSchema("{}", "[]"),
        [](const QJsonObject& args) { return eventListTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.event.update"),
        QStringLiteral("Update startFrame, durationFrames, strength or enabled."),
        objectSchema(
            R"({"id":{"type":"integer"},"startFrame":{"type":"integer"},"durationFrames":{"type":"integer"},"strength":{"type":"number"},"enabled":{"type":"boolean"}})",
            R"(["id"])"),
        [](const QJsonObject& args) { return eventUpdateTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.event.remove"),
        QStringLiteral("Remove an event by id."),
        objectSchema(R"({"id":{"type":"integer"}})", R"(["id"])"),
        [](const QJsonObject& args) { return eventRemoveTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.event.select"),
        QStringLiteral("Select an event by id, or clear the selection with id 0."),
        objectSchema(R"({"id":{"type":"integer"}})", "[]"),
        [](const QJsonObject& args) { return eventSelectTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.viewport.set"),
        QStringLiteral("Set comparison, split position, quality, background and loop "
                       "range."),
        objectSchema(
            R"({"comparison":{"type":"string"},"splitPosition":{"type":"number"},"quality":{"type":"string"},"background":{"type":"string"},"loopEnabled":{"type":"boolean"},"loopFirstFrame":{"type":"integer"},"loopLastFrame":{"type":"integer"},"useEventLoop":{"type":"integer"}})",
            "[]"),
        [](const QJsonObject& args) { return viewportSetTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.cue.bypass"),
        QStringLiteral("Bypass or re-enable a cue kind (cue = CueKind index)."),
        objectSchema(
            R"({"cue":{"type":"integer"},"bypassed":{"type":"boolean"}})", R"(["cue"])"),
        [](const QJsonObject& args) { return cueBypassTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.cue.solo"),
        QStringLiteral("Solo one cue kind, or clear solo with cue < 0."),
        objectSchema(R"({"cue":{"type":"integer"}})", "[]"),
        [](const QJsonObject& args) { return cueSoloTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.frame.cues"),
        QStringLiteral("Sampled visible cues at a frame (solo and bypass applied)."),
        objectSchema(R"({"frame":{"type":"integer"}})", R"(["frame"])"),
        [](const QJsonObject& args) { return frameCuesTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.frame.render"),
        QStringLiteral("Render one frame and return the output, processed and input "
                       "images as MCP image content."),
        objectSchema(
            R"({"frame":{"type":"integer"},"width":{"type":"integer"},"height":{"type":"integer"}})",
            R"(["frame"])"),
        [](const QJsonObject& args) { return frameRenderTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.frame.strip"),
        QStringLiteral("Render a frame range and return a contact sheet plus per-frame "
                       "metadata (held frames included)."),
        objectSchema(
            R"({"firstFrame":{"type":"integer"},"lastFrame":{"type":"integer"},"columns":{"type":"integer"},"scale":{"type":"integer"},"width":{"type":"integer"},"height":{"type":"integer"}})",
            R"(["firstFrame","lastFrame"])"),
        [](const QJsonObject& args) { return frameStripTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.frame.diff"),
        QStringLiteral("Difference mask and changed pixel count between two frames."),
        objectSchema(
            R"({"frameA":{"type":"integer"},"frameB":{"type":"integer"},"width":{"type":"integer"},"height":{"type":"integer"}})",
            R"(["frameA","frameB"])"),
        [](const QJsonObject& args) { return frameDiffTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.capture.write"),
        QStringLiteral("Render a frame range and write PNG frames plus a contact sheet "
                       "to disk for visual debugging."),
        objectSchema(
            R"({"directory":{"type":"string"},"firstFrame":{"type":"integer"},"lastFrame":{"type":"integer"},"width":{"type":"integer"},"height":{"type":"integer"}})",
            R"(["directory","firstFrame","lastFrame"])"),
        [](const QJsonObject& args) { return captureWriteTool(args); });
    registered &= server.registerTool(
        QStringLiteral("fx.capture.directory"),
        QStringLiteral("Get or set the default directory for fx.capture.write."),
        objectSchema(R"({"directory":{"type":"string"}})", "[]"),
        [](const QJsonObject& args) { return captureDirectoryTool(args); });

    if (!registered) {
        std::fprintf(stderr, "ArtifactFxStudioMcpTest: tool registration failed\n");
        return 2;
    }
    if (!server.start()) {
        std::fprintf(stderr, "ArtifactFxStudioMcpTest: could not start the MCP server\n");
        return 2;
    }
    return app.exec();
}
