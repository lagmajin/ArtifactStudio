#pragma once

// Test-only FX cue renderer for Artifact::FxStudio.
//
// FxStudio's model already produces animation samples
// (Artifact.FxStudio.Sequence -> CueSample, Artifact.FxStudio.EventTrack ->
// EventSample, Artifact.FxStudio.ViewportModel -> visibleSamples), but nothing
// turns those samples into pixels: the only consumer today is
// ArtifactFxStudioDialog, which lists them as text. This helper closes that gap
// for tests, so an FX animation can be rendered to PNG frames, compared as
// images, and driven over MCP.
//
// Pixel math runs on a plain float RGBA buffer. QImage is only produced at the
// export boundary (toQImage -> PNG save / ArtifactTestImageArtifacts), so no Qt
// painter or Qt composition mode is involved.
//
// Include order matters: this header is Qt/STL only and must be included BEFORE
// the `import` declarations, matching the other tests/*.h helpers
// (ArtifactTestMcpServer.h, ArtifactTestImageArtifacts.h).
//
// Evaluation order is fixed and independent of cue order in the sequence:
//   signal/geometry (Distort, ChannelSplit, BlockDisplace, SignalDrop, Shake)
//   -> light (Flash, GlowPulse)
//   -> mask (MaskProgress)
//   -> recovery (Recovery)
// Freeze is temporal and is handled by renderSequence(), not by applyCues().

#include <QImage>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <vector>

namespace ArtifactTestFxCueRenderer {

// Mirrors Artifact::FxStudio::CueKind. The FX-render test maps the module enum
// onto this one; a new CueKind must be added here and mapped there.
enum class Effect : int {
    Distort = 0,
    ChannelSplit,
    BlockDisplace,
    Freeze,
    SignalDrop,
    Flash,
    Shake,
    MaskProgress,
    GlowPulse,
    Recovery
};

inline constexpr int kEffectCount = 10;

inline const char* effectName(const Effect effect)
{
    switch (effect) {
    case Effect::Distort: return "Distort";
    case Effect::ChannelSplit: return "ChannelSplit";
    case Effect::BlockDisplace: return "BlockDisplace";
    case Effect::Freeze: return "Freeze";
    case Effect::SignalDrop: return "SignalDrop";
    case Effect::Flash: return "Flash";
    case Effect::Shake: return "Shake";
    case Effect::MaskProgress: return "MaskProgress";
    case Effect::GlowPulse: return "GlowPulse";
    case Effect::Recovery: return "Recovery";
    }
    return "Unknown";
}

// One instantaneous cue, already sampled for a frame.
// `amplitude` corresponds to Artifact::FxStudio::CueSample::strength, which the
// Sequence sampler has already scaled by cue.strength * sequence.strength *
// curve(progress). `variation` widens the seeded jitter for the glitch cues.
struct Cue {
    Effect effect = Effect::Distort;
    float amplitude = 0.0f;
    float progress = 0.0f;
    float variation = 0.0f;
    std::uint32_t seed = 0;

    bool active() const { return amplitude > 0.0f && effect != Effect::Freeze; }
};

// Plain canonical logical RGBA32F image; 4 floats per pixel, row major.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<float> pixels;

    bool empty() const
    {
        return width <= 0 || height <= 0 ||
               pixels.size() != static_cast<std::size_t>(width) *
                                    static_cast<std::size_t>(height) * 4u;
    }

    float* at(const int x, const int y)
    {
        return pixels.data() +
               (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(x)) * 4u;
    }

    const float* at(const int x, const int y) const
    {
        return pixels.data() +
               (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(x)) * 4u;
    }
};

inline Image makeImage(const int width, const int height)
{
    Image image;
    image.width = width;
    image.height = height;
    if (width > 0 && height > 0) {
        image.pixels.assign(static_cast<std::size_t>(width) *
                                static_cast<std::size_t>(height) * 4u,
                            0.0f);
    }
    return image;
}

enum class Comparison : int { Processed = 0, Original, Split };
enum class Background : int { Transparent = 0, Checkerboard, Dark, Light };

struct FrameOptions {
    Comparison comparison = Comparison::Processed;
    float splitPosition = 0.5f;
    Background background = Background::Transparent;
};

struct ApplyContext {
    // MaskProgress reveals this image beyond the sweep edge. When null the
    // untouched input is used instead.
    const Image* revealTarget = nullptr;
};

struct Difference {
    Image mask;
    int changedPixels = 0;
    int minX = 0;
    int minY = 0;
    int maxX = -1;
    int maxY = -1;

    bool any() const { return changedPixels > 0; }
    int width() const { return maxX >= minX ? maxX - minX + 1 : 0; }
    int height() const { return maxY >= minY ? maxY - minY + 1 : 0; }
};

struct FrameReport {
    std::int64_t frame = 0;
    int activeCues = 0;
    bool held = false;
    std::vector<Effect> effects;  // effects that actually changed pixels
};

struct SequenceReport {
    std::vector<FrameReport> frames;
    int heldFrames = 0;
    int distinctFrames = 0;
};

struct SequenceRender {
    std::vector<Image> frames;
    SequenceReport report;
};

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

inline float clamp01(const float value)
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

inline float lerp(const float a, const float b, const float t)
{
    // Exact at the endpoints: a + (b - a) * 1.0f can round away from b, which
    // would stop Recovery at full amplitude from reproducing the original.
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    return a + (b - a) * t;
}

inline int clampIndex(const int value, const int limit)
{
    return value < 0 ? 0 : (value >= limit ? limit - 1 : value);
}

inline std::uint32_t hashSeed(const std::uint32_t seed, const std::uint32_t salt)
{
    std::uint32_t value = seed * 2654435761u + salt * 2246822519u + 0x9E3779B9u;
    value ^= value >> 15;
    value *= 2246822519u;
    value ^= value >> 13;
    value *= 3266489917u;
    value ^= value >> 16;
    return value;
}

inline float hashUnit(const std::uint32_t seed, const std::uint32_t salt)
{
    return static_cast<float>(hashSeed(seed, salt) & 0x00FFFFFFu) /
           static_cast<float>(0x00FFFFFFu);
}

// A deterministic reference image: horizontal/vertical ramps, a checker so
// block and scanline damage is visible, a white border and crosshair so a
// translation can be measured, and two corner markers so a shift is obvious.
inline Image makeDiagnosticImage(const int width, const int height,
                                 const bool secondInput = false)
{
    Image image = makeImage(width, height);
    if (image.empty()) return image;

    const float uSpan = static_cast<float>(std::max(1, width - 1));
    const float vSpan = static_cast<float>(std::max(1, height - 1));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float u = static_cast<float>(x) / uSpan;
            const float v = static_cast<float>(y) / vSpan;
            const bool cell = (((x / 6) + (y / 6)) % 2) == 0;
            float* px = image.at(x, y);
            if (!secondInput) {
                px[0] = u;
                px[1] = v;
                px[2] = cell ? 0.20f : 0.55f;
            } else {
                px[0] = cell ? 0.55f : 0.15f;
                px[1] = u;
                px[2] = v;
            }
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1 ||
                x == width / 2 || y == height / 2) {
                // Bright reference lines, graded along both axes so no row or
                // column is a constant run. A constant run would make a
                // horizontal displacement ambiguous to detect.
                px[0] = 1.0f;
                px[1] = 0.85f + 0.15f * u;
                px[2] = 0.85f + 0.15f * v;
            }
            px[3] = 1.0f;
        }
    }

    const int marker = std::max(2, std::min(width, height) / 8);
    if (width > marker * 3 && height > marker * 3) {
        for (int y = marker; y < marker * 2; ++y) {
            for (int x = marker; x < marker * 2; ++x) {
                float* px = image.at(x, y);
                px[0] = 1.0f; px[1] = 0.0f; px[2] = 0.0f;
            }
        }
        for (int y = height - marker * 2; y < height - marker; ++y) {
            for (int x = width - marker * 2; x < width - marker; ++x) {
                float* px = image.at(x, y);
                px[0] = 0.0f; px[1] = 1.0f; px[2] = 0.0f;
            }
        }
    }
    return image;
}

inline Image boxBlur(const Image& source, const int radius)
{
    Image result = makeImage(source.width, source.height);
    if (result.empty() || radius <= 0) return source;

    Image horizontal = makeImage(source.width, source.height);
    const float inverse = 1.0f / static_cast<float>(radius * 2 + 1);
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            float sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            for (int d = -radius; d <= radius; ++d) {
                const float* px = source.at(clampIndex(x + d, source.width), y);
                for (int c = 0; c < 4; ++c) sum[c] += px[c];
            }
            float* dst = horizontal.at(x, y);
            for (int c = 0; c < 4; ++c) dst[c] = sum[c] * inverse;
        }
    }
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            float sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            for (int d = -radius; d <= radius; ++d) {
                const float* px = horizontal.at(x, clampIndex(y + d, source.height));
                for (int c = 0; c < 4; ++c) sum[c] += px[c];
            }
            float* dst = result.at(x, y);
            for (int c = 0; c < 4; ++c) dst[c] = sum[c] * inverse;
        }
    }
    return result;
}

inline Difference difference(const Image& a, const Image& b,
                             const float threshold = 1.0f / 255.0f)
{
    Difference result;
    if (a.empty() || b.empty() || a.width != b.width || a.height != b.height) {
        return result;
    }
    result.mask = makeImage(a.width, a.height);
    int left = a.width;
    int top = a.height;
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < a.height; ++y) {
        for (int x = 0; x < a.width; ++x) {
            const float* pa = a.at(x, y);
            const float* pb = b.at(x, y);
            float delta = 0.0f;
            for (int c = 0; c < 4; ++c) {
                delta = std::max(delta, std::fabs(pa[c] - pb[c]));
            }
            if (delta <= threshold) continue;
            float* mask = result.mask.at(x, y);
            mask[0] = mask[1] = mask[2] = 1.0f;
            mask[3] = 1.0f;
            ++result.changedPixels;
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
        }
    }
    if (right >= left && bottom >= top) {
        result.minX = left;
        result.minY = top;
        result.maxX = right;
        result.maxY = bottom;
    }
    return result;
}

// ---------------------------------------------------------------------------
// cue evaluation
// ---------------------------------------------------------------------------

namespace detail {

inline bool shiftRowsHorizontal(Image& working, const Image& source,
                               const std::vector<int>& offsets)
{
    bool changed = false;
    for (int y = 0; y < source.height; ++y) {
        const int offset = offsets[static_cast<std::size_t>(y)];
        if (offset == 0) continue;
        changed = true;
        for (int x = 0; x < source.width; ++x) {
            const float* src = source.at(clampIndex(x - offset, source.width), y);
            float* dst = working.at(x, y);
            for (int c = 0; c < 4; ++c) dst[c] = src[c];
        }
    }
    return changed;
}

inline bool applySignalDrop(Image& working, const Image&, const Cue& cue)
{
    const float amount = clamp01(cue.amplitude);
    const int strips = 1 + static_cast<int>(hashUnit(cue.seed, 1u) * 3.0f);
    const int stripHeight =
        std::max(1, static_cast<int>(std::lround(amount *
                                                 static_cast<float>(working.height) * 0.25f)));
    bool changed = false;
    for (int i = 0; i < strips; ++i) {
        const int top = clampIndex(
            static_cast<int>(hashUnit(cue.seed, 17u + static_cast<std::uint32_t>(i)) *
                             static_cast<float>(working.height)),
            working.height);
        for (int y = top; y < std::min(working.height, top + stripHeight); ++y) {
            for (int x = 0; x < working.width; ++x) {
                float* px = working.at(x, y);
                px[0] = 0.06f;
                px[1] = 0.06f;
                px[2] = 0.08f;
                px[3] = 1.0f;
            }
            changed = true;
        }
        // A single bright torn line marks the drop edge.
        for (int x = 0; x < working.width; ++x) {
            float* px = working.at(x, top);
            px[0] = 0.95f;
            px[1] = 0.95f;
            px[2] = 1.0f;
            px[3] = 1.0f;
        }
    }
    return changed;
}

inline bool applyFlash(Image& working, const Image&, const Cue& cue)
{
    const float amount = clamp01(cue.amplitude) * 0.9f;
    if (amount <= 0.0f) return false;
    for (int y = 0; y < working.height; ++y) {
        for (int x = 0; x < working.width; ++x) {
            float* px = working.at(x, y);
            px[0] = clamp01(lerp(px[0], 1.0f, amount));
            px[1] = clamp01(lerp(px[1], 1.0f, amount));
            px[2] = clamp01(lerp(px[2], 1.0f, amount));
        }
    }
    return true;
}

inline bool applyGlowPulse(Image& working, const Image&, const Cue& cue)
{
    const float amount = clamp01(cue.amplitude);
    if (amount <= 0.0f) return false;
    const int radius = std::max(1, static_cast<int>(std::lround(amount * 6.0f)));
    const Image blurred = boxBlur(working, radius);
    for (int y = 0; y < working.height; ++y) {
        for (int x = 0; x < working.width; ++x) {
            const float* src = blurred.at(x, y);
            float* px = working.at(x, y);
            for (int c = 0; c < 3; ++c) {
                px[c] = clamp01(px[c] + src[c] * amount * 0.8f);
            }
        }
    }
    return true;
}

} // namespace detail

// Applies a single cue in place. Freeze is a no-op here (see renderSequence).
inline bool applyCue(Image& working, const Image& original, const Cue& cue,
                     const ApplyContext& context = {})
{
    if (working.empty() || working.width != original.width ||
        working.height != original.height) {
        return false;
    }
    const float amount = clamp01(cue.amplitude);
    const int width = working.width;
    const int height = working.height;

    switch (cue.effect) {
    case Effect::Freeze:
        return false;

    case Effect::SignalDrop:
        return amount <= 0.0f ? false : detail::applySignalDrop(working, original, cue);

    case Effect::Flash:
        return detail::applyFlash(working, original, cue);

    case Effect::GlowPulse:
        return detail::applyGlowPulse(working, original, cue);

    case Effect::Recovery: {
        if (amount <= 0.0f) return false;
        bool changed = false;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float* src = original.at(x, y);
                float* px = working.at(x, y);
                for (int c = 0; c < 4; ++c) {
                    if (std::fabs(px[c] - src[c]) > 0.0f) changed = true;
                    px[c] = clamp01(lerp(px[c], src[c], amount));
                }
            }
        }
        return changed;
    }

    case Effect::MaskProgress: {
        if (amount >= 1.0f) return false;
        const Image* reveal = context.revealTarget ? context.revealTarget : &original;
        if (reveal->empty() || reveal->width != width || reveal->height != height) {
            reveal = &original;
        }
        const int edge = std::clamp(
            static_cast<int>(std::lround(amount * static_cast<float>(width))), 0, width);
        bool changed = false;
        for (int y = 0; y < height; ++y) {
            for (int x = edge; x < width; ++x) {
                const float* src = reveal->at(x, y);
                float* px = working.at(x, y);
                bool differs = false;
                for (int c = 0; c < 4; ++c) {
                    if (std::fabs(px[c] - src[c]) > 0.0f) differs = true;
                    px[c] = src[c];
                }
                if (differs) changed = true;
            }
        }
        return changed;
    }

    case Effect::ChannelSplit: {
        if (amount <= 0.0f) return false;
        const int shift = std::max(1, static_cast<int>(std::lround(amount * 16.0f)));
        const Image source = working;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* dst = working.at(x, y);
                dst[0] = source.at(clampIndex(x - shift, width), y)[0];
                dst[1] = source.at(x, y)[1];
                dst[2] = source.at(clampIndex(x + shift, width), y)[2];
            }
        }
        return true;
    }

    case Effect::Shake: {
        if (amount <= 0.0f) return false;
        const int reach = std::max(
            1, static_cast<int>(std::lround(amount * (8.0f + 16.0f * cue.variation))));
        const int dx = static_cast<int>(std::lround(
            (hashUnit(cue.seed, 101u) * 2.0f - 1.0f) * static_cast<float>(reach)));
        const int dy = static_cast<int>(std::lround(
            (hashUnit(cue.seed, 103u) * 2.0f - 1.0f) * static_cast<float>(reach)));
        if (dx == 0 && dy == 0) return false;
        const Image source = working;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float* src = source.at(clampIndex(x - dx, width),
                                             clampIndex(y - dy, height));
                float* dst = working.at(x, y);
                for (int c = 0; c < 4; ++c) dst[c] = src[c];
            }
        }
        return true;
    }

    case Effect::Distort: {
        if (amount <= 0.0f) return false;
        const int reach = std::max(1, static_cast<int>(std::lround(amount * 24.0f)));
        const float phase = hashUnit(cue.seed, 211u) * 6.28318530717958647692f;
        std::vector<int> offsets(static_cast<std::size_t>(height), 0);
        for (int y = 0; y < height; ++y) {
            const float rowPhase =
                6.28318530717958647692f * 3.0f *
                (static_cast<float>(y) / static_cast<float>(std::max(1, height - 1)));
            const float wave = std::sin(rowPhase + phase);
            const float jitter =
                (hashUnit(cue.seed, 307u + static_cast<std::uint32_t>(y)) * 2.0f - 1.0f) *
                (0.2f + 0.8f * clamp01(cue.variation));
            offsets[static_cast<std::size_t>(y)] = static_cast<int>(
                std::lround(static_cast<float>(reach) * (0.65f * wave + 0.35f * jitter)));
        }
        // Displacement must read an unchanged snapshot, so the shift helper
        // never reads a row it has already overwritten.
        const Image source = working;
        return detail::shiftRowsHorizontal(working, source, offsets);
    }

    case Effect::BlockDisplace: {
        if (amount <= 0.0f) return false;
        constexpr int kBlocks = 6;
        const int blockHeight = std::max(1, height / kBlocks);
        std::vector<int> offsets(static_cast<std::size_t>(height), 0);
        bool anyShift = false;
        for (int block = 0; block < kBlocks; ++block) {
            const float unit =
                hashUnit(cue.seed, 401u + static_cast<std::uint32_t>(block) * 7919u);
            const int offset = static_cast<int>(std::lround(
                (unit * 2.0f - 1.0f) * amount * 32.0f));
            if (offset != 0) anyShift = true;
            for (int y = block * blockHeight;
                 y < std::min(height, (block + 1) * blockHeight); ++y) {
                offsets[static_cast<std::size_t>(y)] = offset;
            }
        }
        // Any rows left over by integer division follow the last block.
        for (int y = kBlocks * blockHeight; y < height; ++y) {
            offsets[static_cast<std::size_t>(y)] =
                offsets[static_cast<std::size_t>(std::max(0, kBlocks * blockHeight - 1))];
        }
        if (!anyShift) return false;
        const Image source = working;
        return detail::shiftRowsHorizontal(working, source, offsets);
    }
    }
    return false;
}

// Applies every cue using the fixed evaluation order documented at the top.
inline void applyCues(Image& working, const Image& original,
                      const std::vector<Cue>& cues,
                      std::vector<Effect>* appliedEffects = nullptr,
                      const ApplyContext& context = {})
{
    static constexpr Effect kOrder[] = {
        Effect::Distort,      Effect::ChannelSplit, Effect::BlockDisplace,
        Effect::SignalDrop,   Effect::Shake,        Effect::Flash,
        Effect::GlowPulse,    Effect::MaskProgress, Effect::Recovery,
    };

    for (const Effect effect : kOrder) {
        for (const Cue& cue : cues) {
            if (cue.effect != effect) continue;
            if (applyCue(working, original, cue, context) && appliedEffects) {
                if (std::find(appliedEffects->begin(), appliedEffects->end(), effect) ==
                    appliedEffects->end()) {
                    appliedEffects->push_back(effect);
                }
            }
        }
    }
}

// Applies the viewport comparison mode and background to a cue-applied frame.
inline Image renderFrame(const Image& processed, const Image& original,
                         const FrameOptions& options)
{
    if (processed.empty()) return processed;
    Image out = processed;

    if (options.comparison == Comparison::Original) {
        if (!original.empty() && original.width == processed.width &&
            original.height == processed.height) {
            out = original;
        }
    } else if (options.comparison == Comparison::Split && !original.empty() &&
               original.width == processed.width &&
               original.height == processed.height) {
        const int split = std::clamp(
            static_cast<int>(std::lround(clamp01(options.splitPosition) *
                                         static_cast<float>(processed.width))),
            0, processed.width);
        for (int y = 0; y < out.height; ++y) {
            for (int x = split; x < out.width; ++x) {
                const float* src = original.at(x, y);
                float* dst = out.at(x, y);
                for (int c = 0; c < 4; ++c) dst[c] = src[c];
            }
        }
    }

    if (options.background != Background::Transparent) {
        for (int y = 0; y < out.height; ++y) {
            for (int x = 0; x < out.width; ++x) {
                float background[3] = {0.0f, 0.0f, 0.0f};
                switch (options.background) {
                case Background::Dark:
                    break;
                case Background::Light:
                    background[0] = background[1] = background[2] = 1.0f;
                    break;
                case Background::Checkerboard: {
                    const float tone = (((x / 8) + (y / 8)) % 2) == 0 ? 0.36f : 0.18f;
                    background[0] = background[1] = background[2] = tone;
                    break;
                }
                case Background::Transparent:
                    break;
                }
                float* px = out.at(x, y);
                const float alpha = clamp01(px[3]);
                for (int c = 0; c < 3; ++c) {
                    px[c] = clamp01(px[c] * alpha + background[c] * (1.0f - alpha));
                }
                px[3] = 1.0f;
            }
        }
    }
    return out;
}

// Renders a frame range. Freeze holds the previously emitted frame while its
// cue is active, which is what the cue means in the FxStudio model; every other
// effect is applied per frame by applyCues().
using CueProvider = std::function<std::vector<Cue>(std::int64_t frame)>;

inline SequenceRender renderSequence(const Image& input,
                                     const std::int64_t firstFrame,
                                     const std::int64_t lastFrame,
                                     const CueProvider& cuesForFrame,
                                     const FrameOptions& options = {},
                                     const Image* revealTarget = nullptr)
{
    SequenceRender result;
    if (input.empty() || lastFrame < firstFrame) return result;

    Image lastEmitted;
    bool hasLast = false;
    for (std::int64_t frame = firstFrame; frame <= lastFrame; ++frame) {
        const std::vector<Cue> cues = cuesForFrame(frame);

        bool frozen = false;
        for (const Cue& cue : cues) {
            if (cue.effect == Effect::Freeze && cue.amplitude > 0.0f) frozen = true;
        }

        FrameReport report;
        report.frame = frame;
        report.activeCues = static_cast<int>(cues.size());

        if (frozen && hasLast) {
            report.held = true;
            ++result.report.heldFrames;
            result.report.frames.push_back(report);
            result.frames.push_back(lastEmitted);
            continue;
        }

        ApplyContext context;
        context.revealTarget = revealTarget;
        Image working = input;
        applyCues(working, input, cues, &report.effects, context);
        const Image out = renderFrame(working, input, options);
        result.report.frames.push_back(report);
        result.frames.push_back(out);
        lastEmitted = out;
        hasLast = true;
    }

    // Count frames that differ from the previously emitted frame.
    int distinct = 0;
    for (std::size_t i = 1; i < result.frames.size(); ++i) {
        if (difference(result.frames[i - 1], result.frames[i]).any()) ++distinct;
    }
    if (!result.frames.empty()) ++distinct;
    result.report.distinctFrames = distinct;
    return result;
}

// Tiles frames into one image so a whole animation can be eyeballed at once.
// `scale` is an integer downsample factor (>= 1).
inline Image makeContactSheet(const std::vector<Image>& frames, const int columns,
                              const int scale = 2)
{
    Image sheet;
    if (frames.empty() || frames.front().empty()) return sheet;
    const int step = std::max(1, scale);
    const int columnsUsed = std::max(1, std::min(columns, static_cast<int>(frames.size())));
    const int rows =
        (static_cast<int>(frames.size()) + columnsUsed - 1) / columnsUsed;
    const int cellWidth = std::max(1, frames.front().width / step);
    const int cellHeight = std::max(1, frames.front().height / step);

    sheet = makeImage(cellWidth * columnsUsed, cellHeight * rows);
    for (std::size_t index = 0; index < frames.size(); ++index) {
        const Image& frame = frames[index];
        if (frame.empty()) continue;
        const int column = static_cast<int>(index) % columnsUsed;
        const int row = static_cast<int>(index) / columnsUsed;
        for (int y = 0; y < cellHeight; ++y) {
            for (int x = 0; x < cellWidth; ++x) {
                const float* src = frame.at(std::min(frame.width - 1, x * step),
                                            std::min(frame.height - 1, y * step));
                float* dst = sheet.at(column * cellWidth + x, row * cellHeight + y);
                for (int c = 0; c < 4; ++c) dst[c] = src[c];
            }
        }
    }
    return sheet;
}

// Export boundary: the only place a QImage is produced.
inline QImage toQImage(const Image& image)
{
    if (image.empty()) return {};
    QImage result(image.width, image.height, QImage::Format_RGBA8888);
    if (result.isNull()) return {};
    for (int y = 0; y < image.height; ++y) {
        uchar* row = result.scanLine(y);
        for (int x = 0; x < image.width; ++x) {
            const float* px = image.at(x, y);
            uchar* dst = row + x * 4;
            for (int c = 0; c < 4; ++c) {
                dst[c] = static_cast<uchar>(
                    std::lround(clamp01(px[c]) * 255.0f));
            }
        }
    }
    return result;
}

inline Image fromQImage(const QImage& source)
{
    if (source.isNull()) return {};
    const QImage rgba = source.convertToFormat(QImage::Format_RGBA8888);
    Image image = makeImage(rgba.width(), rgba.height());
    for (int y = 0; y < rgba.height(); ++y) {
        const uchar* row = rgba.constScanLine(y);
        for (int x = 0; x < rgba.width(); ++x) {
            float* px = image.at(x, y);
            for (int c = 0; c < 4; ++c) {
                px[c] = static_cast<float>(row[x * 4 + c]) / 255.0f;
            }
        }
    }
    return image;
}

} // namespace ArtifactTestFxCueRenderer
