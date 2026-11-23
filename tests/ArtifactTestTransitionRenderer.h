#pragma once

// Test-only image sink for the Video.Transitions.* contract lab.
//
// The transition implementations take a DecodedVideoFrame pair and write the
// result into the left frame in place. Nothing in the test suite could turn that
// into an image, so reviewing a transition meant opening the NLE editor and
// scrubbing. This header closes that gap the same way ArtifactTestFxCueRenderer
// closes it for FxStudio: a plain RGBA8 buffer, a deterministic A/B diagnostic
// pair, pixel statistics, contact sheets, and a QImage conversion at the export
// boundary only. No QPainter and no Qt composition mode is involved.
//
// Include order matters (same contract as ArtifactTestFxCueBridge.h):
//
//   #include <Qt and STL headers>
//   #include "ArtifactTestTransitionRenderer.h"
//   import NLE.Core;
//   import Video.VideoFrame;
//   ...
//   #include "ArtifactTestTransitionBridge.h"      <-- module adapters
//
// This header is Qt/STL only and must be included BEFORE the import
// declarations. The transition-facing adapters live in
// ArtifactTestTransitionBridge.h because they need the imported Video types.

#include <QImage>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace ArtifactTestTransitionRenderer {

// One tightly packed RGBA8 frame: the layout CpuFrameView reads, where the
// stride is exactly width * 4 and row y starts at y * width * 4.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;

    bool empty() const
    {
        return width <= 0 || height <= 0 ||
               pixels.size() != static_cast<std::size_t>(width) *
                                    static_cast<std::size_t>(height) * 4u;
    }

    std::uint8_t* at(const int x, const int y)
    {
        return pixels.data() +
               (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(x)) * 4u;
    }

    const std::uint8_t* at(const int x, const int y) const
    {
        return pixels.data() +
               (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(x)) * 4u;
    }
};

inline bool operator==(const Image& a, const Image& b)
{
    return a.width == b.width && a.height == b.height && a.pixels == b.pixels;
}

inline bool operator!=(const Image& a, const Image& b)
{
    return !(a == b);
}

inline std::uint8_t toByte(const float value)
{
    const float clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    return static_cast<std::uint8_t>(clamped * 255.0f + 0.5f);
}

inline Image makeImage(const int width, const int height)
{
    Image image;
    image.width = width;
    image.height = height;
    if (width > 0 && height > 0) {
        image.pixels.assign(static_cast<std::size_t>(width) *
                                static_cast<std::size_t>(height) * 4u,
                            0u);
    }
    return image;
}

// A deterministic A/B pair. Every pixel differs between the two inputs by at
// least ~40/255 in one channel, so "this output pixel comes from A" and "this
// output pixel comes from B" are unambiguous when compared byte for byte.
//
// A: warm ramp + checker, bright border and crosshair, red/green corner marks.
// B: cool ramp + checker, dark border and crosshair, magenta/cyan corner marks.
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
            const bool cell = (((x / 8) + (y / 8)) % 2) == 0;
            std::uint8_t* px = image.at(x, y);
            if (!secondInput) {
                px[0] = toByte(0.15f + 0.50f * u);
                px[1] = toByte(0.15f + 0.60f * v);
                px[2] = static_cast<std::uint8_t>(cell ? 40u : 90u);
            } else {
                // Deliberately not the complement of A: a 50/50 blend of two
                // complementary ramps collapses to a flat grey frame, which
                // would hide what a crossfade or zoom actually does halfway
                // through. Every pixel still differs from A in at least one
                // channel, so exact-match classification stays unambiguous.
                px[0] = toByte(0.95f - 0.20f * u);
                px[1] = toByte(0.95f - 0.30f * v);
                px[2] = static_cast<std::uint8_t>(cell ? 230u : 120u);
            }
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1 ||
                x == width / 2 || y == height / 2) {
                px[0] = static_cast<std::uint8_t>(secondInput ? 15u : 245u);
                px[1] = static_cast<std::uint8_t>(secondInput ? 20u : 240u);
                px[2] = static_cast<std::uint8_t>(secondInput ? 25u : 235u);
            }
            px[3] = 255u;
        }
    }

    const int marker = std::max(2, std::min(width, height) / 8);
    if (width > marker * 3 && height > marker * 3) {
        for (int y = marker; y < marker * 2; ++y) {
            for (int x = marker; x < marker * 2; ++x) {
                std::uint8_t* px = image.at(x, y);
                px[0] = 255u;
                px[1] = 0u;
                px[2] = secondInput ? 255u : 0u;
            }
        }
        for (int y = height - marker * 2; y < height - marker; ++y) {
            for (int x = width - marker * 2; x < width - marker; ++x) {
                std::uint8_t* px = image.at(x, y);
                px[0] = 0u;
                px[1] = 255u;
                px[2] = secondInput ? 255u : 0u;
            }
        }
    }
    return image;
}

inline bool pixelEquals(const std::uint8_t* a, const std::uint8_t* b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

enum class Source : int { Left = 0, Right, Mixed };

// Per-frame source accounting. A pixel is "left" or "right" only when it is
// byte-identical to that input at the same coordinate; everything else is
// "mixed". That makes order-only transitions (Wipe, Slide) and blend-only
// transitions (Crossfade, Zoom) measurable with the same numbers.
struct FrameStats {
    int totalPixels = 0;
    int leftPixels = 0;
    int rightPixels = 0;
    int mixedPixels = 0;
    double leftFraction = 0.0;
    double rightFraction = 0.0;
    double mixedFraction = 0.0;
    // First column whose majority equals the right input, or -1. Only
    // meaningful for column-ordered reveals; informational for the rest.
    int seamColumn = -1;
};

inline FrameStats classifyFrame(const Image& output, const Image& left,
                                const Image& right)
{
    FrameStats stats;
    if (output.empty() || left.empty() || right.empty() ||
        output.width != left.width || output.height != left.height ||
        output.width != right.width || output.height != right.height) {
        return stats;
    }

    std::vector<int> rightPerColumn(static_cast<std::size_t>(output.width), 0);
    stats.totalPixels = output.width * output.height;
    for (int y = 0; y < output.height; ++y) {
        for (int x = 0; x < output.width; ++x) {
            const std::uint8_t* out = output.at(x, y);
            const bool isLeft = pixelEquals(out, left.at(x, y));
            const bool isRight = pixelEquals(out, right.at(x, y));
            if (isRight && !isLeft) {
                ++stats.rightPixels;
                ++rightPerColumn[static_cast<std::size_t>(x)];
            } else if (isLeft) {
                ++stats.leftPixels;
            } else {
                ++stats.mixedPixels;
            }
        }
    }

    const double total = static_cast<double>(std::max(1, stats.totalPixels));
    stats.leftFraction = static_cast<double>(stats.leftPixels) / total;
    stats.rightFraction = static_cast<double>(stats.rightPixels) / total;
    stats.mixedFraction = static_cast<double>(stats.mixedPixels) / total;

    for (int x = 0; x < output.width; ++x) {
        if (rightPerColumn[static_cast<std::size_t>(x)] * 2 >= output.height) {
            stats.seamColumn = x;
            break;
        }
    }
    return stats;
}

inline double dominantFraction(const FrameStats& stats)
{
    return std::max(stats.leftFraction, stats.rightFraction);
}

inline Source dominantSource(const FrameStats& stats,
                             const double tolerance = 0.02)
{
    if (stats.leftFraction >= 1.0 - tolerance) return Source::Left;
    if (stats.rightFraction >= 1.0 - tolerance) return Source::Right;
    return Source::Mixed;
}

inline const char* sourceName(const Source source)
{
    switch (source) {
    case Source::Left: return "Left";
    case Source::Right: return "Right";
    case Source::Mixed: return "Mixed";
    }
    return "Mixed";
}

inline bool alphaEquals(const Image& image, const std::uint8_t value)
{
    if (image.empty()) return false;
    for (std::size_t index = 3; index < image.pixels.size(); index += 4) {
        if (image.pixels[index] != value) return false;
    }
    return true;
}

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

inline Difference difference(const Image& a, const Image& b,
                             const int channelThreshold = 0)
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
    const int threshold = std::clamp(channelThreshold, 0, 255);
    for (int y = 0; y < a.height; ++y) {
        for (int x = 0; x < a.width; ++x) {
            const std::uint8_t* pa = a.at(x, y);
            const std::uint8_t* pb = b.at(x, y);
            int delta = 0;
            for (int c = 0; c < 4; ++c) {
                delta = std::max(delta, std::abs(static_cast<int>(pa[c]) -
                                                 static_cast<int>(pb[c])));
            }
            if (delta <= threshold) continue;
            std::uint8_t* mask = result.mask.at(x, y);
            mask[0] = 255u;
            mask[1] = 255u;
            mask[2] = 255u;
            mask[3] = 255u;
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

// Block-average contact sheet. Frames keep their aspect ratio; `scale` is the
// integer downscale factor, matching ArtifactTestFxCueRenderer::makeContactSheet
// so the two labs produce sheets of the same weight.
inline Image makeContactSheet(const std::vector<Image>& frames,
                              const int columns, const int scale = 2)
{
    if (frames.empty() || columns < 1) return Image{};
    const int divisor = std::max(1, scale);
    const int cellWidth = std::max(1, frames.front().width / divisor);
    const int cellHeight = std::max(1, frames.front().height / divisor);
    const int rows = (static_cast<int>(frames.size()) + columns - 1) / columns;
    Image sheet = makeImage(cellWidth * columns, cellHeight * rows);
    if (sheet.empty()) return sheet;

    for (std::size_t index = 0; index < frames.size(); ++index) {
        const Image& frame = frames[index];
        if (frame.empty()) continue;
        const int originX = static_cast<int>(index) % columns * cellWidth;
        const int originY = static_cast<int>(index) / columns * cellHeight;
        for (int y = 0; y < cellHeight; ++y) {
            for (int x = 0; x < cellWidth; ++x) {
                int sum[4] = {0, 0, 0, 0};
                int samples = 0;
                for (int sy = 0; sy < divisor; ++sy) {
                    for (int sx = 0; sx < divisor; ++sx) {
                        const int sourceX = x * divisor + sx;
                        const int sourceY = y * divisor + sy;
                        if (sourceX >= frame.width || sourceY >= frame.height) {
                            continue;
                        }
                        const std::uint8_t* px = frame.at(sourceX, sourceY);
                        for (int c = 0; c < 4; ++c) sum[c] += px[c];
                        ++samples;
                    }
                }
                std::uint8_t* dst = sheet.at(originX + x, originY + y);
                const int count = std::max(1, samples);
                for (int c = 0; c < 4; ++c) {
                    dst[c] = static_cast<std::uint8_t>(sum[c] / count);
                }
            }
        }
    }
    return sheet;
}

inline QImage toQImage(const Image& image)
{
    if (image.empty()) return {};
    QImage result(image.width, image.height, QImage::Format_RGBA8888);
    if (result.isNull()) return {};
    for (int y = 0; y < image.height; ++y) {
        std::memcpy(result.scanLine(y), image.at(0, y),
                    static_cast<std::size_t>(image.width) * 4u);
    }
    return result;
}

} // namespace ArtifactTestTransitionRenderer
