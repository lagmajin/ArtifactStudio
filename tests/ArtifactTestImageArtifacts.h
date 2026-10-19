#pragma once

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>
#include <QRect>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <cmath>

// Test-only image artifacts for visual recognition. Captures from independent
// states can be compared to isolate a semantic layer without changing the
// production renderer or requiring desktop-window pixels.
namespace ArtifactTestImageArtifacts {

inline QByteArray encodePng(const QImage& image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (image.isNull() || !buffer.open(QIODevice::WriteOnly) ||
        !image.save(&buffer, "PNG")) return {};
    return bytes;
}

inline QJsonObject imageContent(const QString& name, const QImage& image)
{
    const QByteArray png = encodePng(image);
    if (png.isEmpty()) return {};
    return {{QStringLiteral("type"), QStringLiteral("image")},
            {QStringLiteral("mimeType"), QStringLiteral("image/png")},
            {QStringLiteral("name"), name},
            {QStringLiteral("data"), QString::fromLatin1(png.toBase64())}};
}

struct DifferenceMaskResult {
    QImage mask;
    qsizetype changedPixels = 0;
    QRect bounds;
};

inline DifferenceMaskResult differenceMask(const QImage& baseline,
                                           const QImage& variant,
                                           int channelThreshold = 4)
{
    DifferenceMaskResult result;
    if (baseline.isNull() || variant.isNull() ||
        baseline.size() != variant.size()) return result;
    const QImage a = baseline.convertToFormat(QImage::Format_RGBA8888);
    const QImage b = variant.convertToFormat(QImage::Format_RGBA8888);
    result.mask = QImage(a.size(), QImage::Format_RGBA8888);
    result.mask.fill(Qt::transparent);
    int left = a.width(), top = a.height(), right = -1, bottom = -1;
    const int threshold = std::clamp(channelThreshold, 0, 255);
    for (int y = 0; y < a.height(); ++y) {
        const auto* ar = a.constScanLine(y);
        const auto* br = b.constScanLine(y);
        auto* mr = result.mask.scanLine(y);
        for (int x = 0; x < a.width(); ++x) {
            const int offset = x * 4;
            const int dr = std::abs(int(ar[offset]) - int(br[offset]));
            const int dg = std::abs(int(ar[offset + 1]) - int(br[offset + 1]));
            const int db = std::abs(int(ar[offset + 2]) - int(br[offset + 2]));
            if (std::max({dr, dg, db}) <= threshold) continue;
            mr[offset] = mr[offset + 1] = mr[offset + 2] = 255;
            mr[offset + 3] = 255;
            ++result.changedPixels;
            left = std::min(left, x); top = std::min(top, y);
            right = std::max(right, x); bottom = std::max(bottom, y);
        }
    }
    if (right >= left && bottom >= top)
        result.bounds = QRect(left, top, right - left + 1, bottom - top + 1);
    return result;
}

inline QJsonArray regionMetadata(const QString& name, const QRect& rect)
{
    return QJsonArray{name, rect.x(), rect.y(), rect.width(), rect.height()};
}

} // namespace ArtifactTestImageArtifacts
