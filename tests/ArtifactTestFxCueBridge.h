#pragma once

// Adapters between the Artifact::FxStudio model and ArtifactTestFxCueRenderer.
//
// REQUIRED INCLUDE ORDER (the renderer is Qt/STL only and must be included
// before the `import` declarations; this header needs the imported FxStudio
// types and so must come after them):
//
//   #include <Qt and STL headers>
//   #include "../ArtifactTestFxCueRenderer.h"
//   import Artifact.FxStudio.EventTrack;
//   import Artifact.FxStudio.Sequence;
//   import Artifact.FxStudio.ViewportModel;
//   #include "../ArtifactTestFxCueBridge.h"      <-- here
//
// This header deliberately adds no includes of its own: pulling an STL header
// in after the import declarations is what the ArtifactCore module rules warn
// about. It relies on ArtifactTestFxCueRenderer.h having been included first.
//
// toEffect() is an explicit switch rather than a static_cast so that a reorder
// of Artifact::FxStudio::CueKind or of the renderer's Effect list fails the
// EveryCueKindMapsToADistinctRenderEffect contract test instead of silently
// swapping two effects.

#include "ArtifactTestFxCueRenderer.h"

namespace ArtifactTestFxCueBridge {

using ArtifactTestFxCueRenderer::Comparison;
using ArtifactTestFxCueRenderer::Cue;
using ArtifactTestFxCueRenderer::Effect;
using ArtifactTestFxCueRenderer::Image;

inline Effect toEffect(const Artifact::FxStudio::CueKind kind)
{
    switch (kind) {
    case Artifact::FxStudio::CueKind::Distort: return Effect::Distort;
    case Artifact::FxStudio::CueKind::ChannelSplit: return Effect::ChannelSplit;
    case Artifact::FxStudio::CueKind::BlockDisplace: return Effect::BlockDisplace;
    case Artifact::FxStudio::CueKind::Freeze: return Effect::Freeze;
    case Artifact::FxStudio::CueKind::SignalDrop: return Effect::SignalDrop;
    case Artifact::FxStudio::CueKind::Flash: return Effect::Flash;
    case Artifact::FxStudio::CueKind::Shake: return Effect::Shake;
    case Artifact::FxStudio::CueKind::MaskProgress: return Effect::MaskProgress;
    case Artifact::FxStudio::CueKind::GlowPulse: return Effect::GlowPulse;
    case Artifact::FxStudio::CueKind::Recovery: return Effect::Recovery;
    }
    return Effect::Distort;
}

inline Cue toCue(const Artifact::FxStudio::CueSample& sample)
{
    Cue cue;
    cue.effect = toEffect(sample.kind);
    // Sequence::sample() already scaled strength by cue.strength *
    // sequence.strength * curve(progress).
    cue.amplitude = sample.strength;
    cue.progress = sample.progress;
    cue.variation = sample.variation;
    cue.seed = sample.seed;
    return cue;
}

inline std::vector<Cue> toCues(
    const std::vector<Artifact::FxStudio::EventSample>& samples)
{
    std::vector<Cue> cues;
    cues.reserve(samples.size());
    for (const Artifact::FxStudio::EventSample& sample : samples) {
        cues.push_back(toCue(sample.cue));
    }
    return cues;
}

inline std::vector<Cue> cuesAt(const Artifact::FxStudio::EventTrack& track,
                               const std::int64_t frame)
{
    return toCues(track.sample(frame));
}

// Same as cuesAt() but respects Cue bypass and Cue solo, matching what the
// FxStudio viewport would actually show.
inline std::vector<Cue> visibleCuesAt(const Artifact::FxStudio::EventTrack& track,
                                      const Artifact::FxStudio::ViewportModel& viewport,
                                      const std::int64_t frame)
{
    return toCues(viewport.visibleSamples(track, frame));
}

inline Comparison toComparison(const Artifact::FxStudio::ComparisonMode mode)
{
    switch (mode) {
    case Artifact::FxStudio::ComparisonMode::Processed: return Comparison::Processed;
    case Artifact::FxStudio::ComparisonMode::Original: return Comparison::Original;
    case Artifact::FxStudio::ComparisonMode::Split: return Comparison::Split;
    }
    return Comparison::Processed;
}

inline ArtifactTestFxCueRenderer::Background toBackground(
    const Artifact::FxStudio::PreviewBackground background)
{
    switch (background) {
    case Artifact::FxStudio::PreviewBackground::Checkerboard:
        return ArtifactTestFxCueRenderer::Background::Checkerboard;
    case Artifact::FxStudio::PreviewBackground::Dark:
        return ArtifactTestFxCueRenderer::Background::Dark;
    case Artifact::FxStudio::PreviewBackground::Light:
        return ArtifactTestFxCueRenderer::Background::Light;
    case Artifact::FxStudio::PreviewBackground::Transparent:
        return ArtifactTestFxCueRenderer::Background::Transparent;
    }
    return ArtifactTestFxCueRenderer::Background::Transparent;
}

inline const char* cueKindName(const Artifact::FxStudio::CueKind kind)
{
    return ArtifactTestFxCueRenderer::effectName(toEffect(kind));
}

inline const char* cueCurveName(const Artifact::FxStudio::CueCurve curve)
{
    switch (curve) {
    case Artifact::FxStudio::CueCurve::Step: return "Step";
    case Artifact::FxStudio::CueCurve::Linear: return "Linear";
    case Artifact::FxStudio::CueCurve::EaseIn: return "EaseIn";
    case Artifact::FxStudio::CueCurve::EaseOut: return "EaseOut";
    case Artifact::FxStudio::CueCurve::EaseInOut: return "EaseInOut";
    case Artifact::FxStudio::CueCurve::Pulse: return "Pulse";
    }
    return "Unknown";
}

inline const char* familyName(const Artifact::FxStudio::Family family)
{
    switch (family) {
    case Artifact::FxStudio::Family::Glitch: return "Glitch";
    case Artifact::FxStudio::Family::Impact: return "Impact";
    case Artifact::FxStudio::Family::Reveal: return "Reveal";
    case Artifact::FxStudio::Family::Energy: return "Energy";
    case Artifact::FxStudio::Family::Transition: return "Transition";
    }
    return "Unknown";
}

inline const char* comparisonName(const Artifact::FxStudio::ComparisonMode mode)
{
    switch (mode) {
    case Artifact::FxStudio::ComparisonMode::Processed: return "Processed";
    case Artifact::FxStudio::ComparisonMode::Original: return "Original";
    case Artifact::FxStudio::ComparisonMode::Split: return "Split";
    }
    return "Unknown";
}

inline const char* qualityName(const Artifact::FxStudio::PreviewQuality quality)
{
    switch (quality) {
    case Artifact::FxStudio::PreviewQuality::Draft: return "Draft";
    case Artifact::FxStudio::PreviewQuality::Interactive: return "Interactive";
    case Artifact::FxStudio::PreviewQuality::Final: return "Final";
    }
    return "Unknown";
}

} // namespace ArtifactTestFxCueBridge
