#include "ExternalFrameRenderer.h"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QSvgRenderer>
#include <QTextCharFormat>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <optional>

namespace ArtifactRenderer {
namespace {

QString normalizeBackend(const QString& backend)
{
    const QString value = backend.trimmed().toLower();
    if (value.isEmpty() || value == QStringLiteral("auto")) {
        return QStringLiteral("diagnostic");
    }
    if (value == QStringLiteral("cpu") || value == QStringLiteral("software")) {
        return QStringLiteral("diagnostic");
    }
    return value;
}

QString layerKindName(const QJsonObject& layer)
{
    const QString explicitKind = layer.value(QStringLiteral("layerType")).toString().trimmed();
    if (!explicitKind.isEmpty()) {
        return explicitKind;
    }

    switch (layer.value(QStringLiteral("type")).toInt(-1)) {
    case 2:
        return QStringLiteral("Null");
    case 3:
        return QStringLiteral("Solid");
    case 4:
        return QStringLiteral("Image");
    case 5:
        return QStringLiteral("Adjustment");
    case 6:
        return QStringLiteral("Text");
    case 7:
        return QStringLiteral("Shape");
    case 8:
        return QStringLiteral("Precomp");
    case 9:
        return QStringLiteral("Audio");
    case 10:
        return QStringLiteral("Video");
    case 11:
        return QStringLiteral("Camera");
    case 12:
        return QStringLiteral("Light");
    case 13:
        return QStringLiteral("Group");
    case 14:
        return QStringLiteral("Folder");
    case 15:
        return QStringLiteral("Particle");
    case 16:
        return QStringLiteral("Clone");
    case 17:
        return QStringLiteral("SDF");
    case 18:
        return QStringLiteral("Model3D");
    default:
        return QStringLiteral("Unknown");
    }
}

QColor deriveJobColor(const RenderJobSummary& summary)
{
    const size_t hash = qHash(summary.jobId + summary.compositionId);
    const int red = 48 + static_cast<int>(hash & 0x7f);
    const int green = 64 + static_cast<int>((hash >> 7) & 0x7f);
    const int blue = 80 + static_cast<int>((hash >> 14) & 0x7f);
    return QColor(red, green, blue, 255);
}

void paintDiagnosticPattern(QImage* image, const QColor& base)
{
    if (!image || image->isNull()) {
        return;
    }

    image->fill(base.rgba());

    const int width = image->width();
    const int height = image->height();
    const QColor stripe(std::min(base.red() + 64, 255),
                        std::min(base.green() + 64, 255),
                        std::min(base.blue() + 64, 255),
                        255);

    const int stripeWidth = std::max(8, width / 48);
    for (int y = 0; y < height; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(image->scanLine(y));
        for (int x = 0; x < width; ++x) {
            if (((x + y) / stripeWidth) % 9 == 0) {
                row[x] = stripe.rgba();
            }
        }
    }

    const QColor border(255, 255, 255, 255);
    const QRgb borderRgb = border.rgba();
    const int borderWidth = std::max(2, std::min(width, height) / 160);
    for (int y = 0; y < height; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(image->scanLine(y));
        for (int x = 0; x < width; ++x) {
            if (x < borderWidth || y < borderWidth ||
                x >= width - borderWidth || y >= height - borderWidth) {
                row[x] = borderRgb;
            }
        }
    }
}

QColor jsonColor(const QJsonObject& object, const QColor& fallback)
{
    if (object.isEmpty()) {
        return fallback;
    }
    return QColor::fromRgbF(
        std::clamp(object.value(QStringLiteral("r")).toDouble(fallback.redF()), 0.0, 1.0),
        std::clamp(object.value(QStringLiteral("g")).toDouble(fallback.greenF()), 0.0, 1.0),
        std::clamp(object.value(QStringLiteral("b")).toDouble(fallback.blueF()), 0.0, 1.0),
        std::clamp(object.value(QStringLiteral("a")).toDouble(fallback.alphaF()), 0.0, 1.0));
}

QColor jsonColorValue(const QJsonValue& value, const QColor& fallback)
{
    if (value.isObject()) {
        return jsonColor(value.toObject(), fallback);
    }
    if (value.isString()) {
        const QColor color(value.toString());
        if (color.isValid()) {
            return color;
        }
    }
    return fallback;
}

QSizeF compositionSizeForSummary(const RenderJobSummary& summary)
{
    const double width = summary.compositionSnapshot.value(QStringLiteral("width")).toDouble(summary.outputWidth);
    const double height = summary.compositionSnapshot.value(QStringLiteral("height")).toDouble(summary.outputHeight);
    return QSizeF(std::max(1.0, width), std::max(1.0, height));
}

QTextOption::WrapMode textWrapModeFromJson(int value)
{
    switch (value) {
    case 0:
        return QTextOption::NoWrap;
    case 2:
        return QTextOption::WrapAnywhere;
    case 3:
        return QTextOption::ManualWrap;
    case 1:
    default:
        return QTextOption::WordWrap;
    }
}

Qt::Alignment textAlignmentFromJson(int value)
{
    switch (value) {
    case 1:
        return Qt::AlignHCenter;
    case 2:
        return Qt::AlignRight;
    case 3:
        return Qt::AlignJustify;
    case 0:
    default:
        return Qt::AlignLeft;
    }
}

QImage rasterizeTextLayer(const QJsonObject& layer, const QColor& textColor, int margin, QString* errorOut)
{
    const QString rawText = layer.value(QStringLiteral("text.value")).toString();
    const bool isRichText = Qt::mightBeRichText(rawText);
    QString displayText = rawText;
    if (!isRichText && layer.value(QStringLiteral("text.allCaps")).toBool(false)) {
        displayText = displayText.toUpper();
    }

    QFont font(layer.value(QStringLiteral("text.fontFamily")).toString(QStringLiteral("Arial")));
    const double fontSize = layer.value(QStringLiteral("text.fontSize")).toDouble(60.0);
    if (fontSize > 0.0) {
        font.setPointSizeF(fontSize);
    }
    font.setBold(layer.value(QStringLiteral("text.bold")).toBool(false));
    font.setItalic(layer.value(QStringLiteral("text.italic")).toBool(false));
    font.setUnderline(layer.value(QStringLiteral("text.underline")).toBool(false));
    font.setStrikeOut(layer.value(QStringLiteral("text.strikethrough")).toBool(false));
    font.setLetterSpacing(QFont::AbsoluteSpacing,
                          layer.value(QStringLiteral("text.tracking")).toDouble(0.0));

    QTextDocument doc;
    doc.setUndoRedoEnabled(false);
    doc.setDocumentMargin(0.0);
    doc.setDefaultFont(font);

    QTextOption option = doc.defaultTextOption();
    option.setWrapMode(textWrapModeFromJson(layer.value(QStringLiteral("text.wrapMode")).toInt(1)));
    option.setAlignment(textAlignmentFromJson(layer.value(QStringLiteral("text.alignment")).toInt(0)));
    doc.setDefaultTextOption(option);

    if (isRichText) {
        doc.setHtml(displayText);
    } else {
        doc.setPlainText(displayText);
    }

    QTextCursor cursor(&doc);
    cursor.select(QTextCursor::Document);
    QTextCharFormat format;
    format.setFont(font);
    format.setForeground(textColor);
    cursor.mergeCharFormat(format);

    const double paragraphSpacing = layer.value(QStringLiteral("text.paragraphSpacing")).toDouble(0.0);
    const double leading = layer.value(QStringLiteral("text.leading")).toDouble(-1.0);
    if (paragraphSpacing > 0.0 || leading > 0.0) {
        for (QTextBlock block = doc.begin(); block.isValid(); block = block.next()) {
            QTextCursor blockCursor(block);
            QTextBlockFormat blockFormat = block.blockFormat();
            if (paragraphSpacing > 0.0) {
                blockFormat.setBottomMargin(paragraphSpacing);
            }
            if (leading > 0.0) {
                blockFormat.setLineHeight(static_cast<qreal>(leading * 100.0),
                                          QTextBlockFormat::ProportionalHeight);
            }
            blockCursor.setBlockFormat(blockFormat);
        }
    }

    const double maxWidth = layer.value(QStringLiteral("text.maxWidth")).toDouble(0.0);
    if (maxWidth > 0.0) {
        doc.setTextWidth(maxWidth);
    }

    doc.adjustSize();
    const QSizeF docSize = doc.size();
    const double boxWidth = maxWidth > 0.0 ? maxWidth : docSize.width();
    const double boxHeight = std::max(0.0, layer.value(QStringLiteral("text.boxHeight")).toDouble(0.0));
    const int canvasWidth = std::max(1, static_cast<int>(std::ceil(boxWidth + margin * 2.0)));
    const int canvasHeight = std::max(1, static_cast<int>(std::ceil(std::max(boxHeight, docSize.height()) + margin * 2.0)));
    QImage image(canvasWidth, canvasHeight, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to allocate text image");
        }
        return {};
    }
    image.fill(Qt::transparent);

    const int verticalAlignment = layer.value(QStringLiteral("text.verticalAlignment")).toInt(0);
    double verticalOffset = 0.0;
    if (boxHeight > docSize.height()) {
        switch (verticalAlignment) {
        case 1:
            verticalOffset = (boxHeight - docSize.height()) * 0.5;
            break;
        case 2:
            verticalOffset = boxHeight - docSize.height();
            break;
        case 0:
        default:
            verticalOffset = 0.0;
            break;
        }
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.translate(margin, margin + verticalOffset);
    doc.drawContents(&painter);
    painter.end();

    return image;
}

QImage rasterizeSvgLayer(const QJsonObject& layer, const RenderExecutionOptions& options, QString* errorOut)
{
    const QString sourcePath = layer.value(QStringLiteral("svg.sourcePath")).toString().trimmed();
    if (sourcePath.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Missing svg.sourcePath");
        }
        return {};
    }

    QString resolvedPath = sourcePath;
    if (!QFileInfo(sourcePath).isAbsolute()) {
        const QDir baseDir(options.sourceBaseDirectory.isEmpty()
                               ? QDir::currentPath()
                               : options.sourceBaseDirectory);
        resolvedPath = baseDir.absoluteFilePath(sourcePath);
    }

    QSvgRenderer renderer(resolvedPath);
    if (!renderer.isValid()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invalid SVG source: %1").arg(sourcePath);
        }
        return {};
    }

    QSize renderSize = renderer.defaultSize();
    if (!renderSize.isValid() || renderSize.isEmpty()) {
        const QRectF viewBox = renderer.viewBoxF();
        if (viewBox.isValid() && viewBox.width() > 0.0 && viewBox.height() > 0.0) {
            renderSize = viewBox.size().toSize();
        }
    }
    if (!renderSize.isValid() || renderSize.isEmpty()) {
        renderSize = QSize(512, 512);
    }

    QImage image(renderSize, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to allocate SVG image");
        }
        return {};
    }
    image.fill(Qt::transparent);

    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();
    return image;
}

QImage rasterizeVideoLayer(const QJsonObject& layer, const RenderExecutionOptions& options, QString* errorOut)
{
    const QString proxyPath = layer.value(QStringLiteral("video.proxyPath")).toString().trimmed();
    const QString sourcePath = proxyPath.isEmpty()
        ? layer.value(QStringLiteral("video.sourcePath")).toString().trimmed()
        : proxyPath;
    if (sourcePath.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Missing video.sourcePath");
        }
        return {};
    }

    QString resolvedPath = sourcePath;
    if (!QFileInfo(sourcePath).isAbsolute()) {
        const QDir baseDir(options.sourceBaseDirectory.isEmpty()
                               ? QDir::currentPath()
                               : options.sourceBaseDirectory);
        resolvedPath = baseDir.absoluteFilePath(sourcePath);
    }

    QImageReader reader(resolvedPath);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (!image.isNull()) {
        return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    const int fallbackWidth = std::max(1, layer.value(QStringLiteral("video.width")).toInt(320));
    const int fallbackHeight = std::max(1, layer.value(QStringLiteral("video.height")).toInt(180));
    QImage placeholder(fallbackWidth, fallbackHeight, QImage::Format_ARGB32_Premultiplied);
    placeholder.fill(QColor(16, 18, 22, 255));

    QPainter painter(&placeholder);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(52, 58, 68, 255));
    painter.drawRoundedRect(QRectF(0, 0, placeholder.width(), placeholder.height()), 18.0, 18.0);
    painter.setBrush(QColor(210, 215, 225, 255));
    QPainterPath play;
    const double cx = placeholder.width() * 0.42;
    const double cy = placeholder.height() * 0.5;
    const double radius = std::min(placeholder.width(), placeholder.height()) * 0.18;
    play.moveTo(cx - radius * 0.55, cy - radius);
    play.lineTo(cx + radius * 0.9, cy);
    play.lineTo(cx - radius * 0.55, cy + radius);
    play.closeSubpath();
    painter.drawPath(play);
    painter.end();

    if (errorOut) {
        *errorOut = QStringLiteral("Video source could not be decoded as an image: %1").arg(resolvedPath);
    }
    return placeholder;
}

QImage rasterizeImageLayer(const QJsonObject& layer, const RenderExecutionOptions& options, QString* errorOut)
{
    const QString sourcePath = layer.value(QStringLiteral("image.sourcePath")).toString().trimmed();
    if (sourcePath.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Missing image.sourcePath");
        }
        return {};
    }

    QString resolvedPath = sourcePath;
    if (!QFileInfo(sourcePath).isAbsolute()) {
        const QDir baseDir(options.sourceBaseDirectory.isEmpty()
                               ? QDir::currentPath()
                               : options.sourceBaseDirectory);
        resolvedPath = baseDir.absoluteFilePath(sourcePath);
    }

    QImage image(resolvedPath);
    if (image.isNull()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invalid image source: %1").arg(sourcePath);
        }
        return {};
    }

    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

bool paintTextLayer(QPainter* painter, const QJsonObject& layer)
{
    if (!painter) {
        return false;
    }

    const double opacity = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0);
    const QJsonObject transform = layer.value(QStringLiteral("transform")).toObject();
    const double x = transform.value(QStringLiteral("px")).toDouble(0.0);
    const double y = transform.value(QStringLiteral("py")).toDouble(0.0);
    const double anchorX = transform.value(QStringLiteral("ax")).toDouble(0.0);
    const double anchorY = transform.value(QStringLiteral("ay")).toDouble(0.0);
    const double rotation = transform.value(QStringLiteral("rx")).toDouble(0.0);
    const double scaleX = transform.value(QStringLiteral("sx")).toDouble(1.0);
    const double scaleY = transform.value(QStringLiteral("sy")).toDouble(1.0);

    const QColor fillColor = jsonColorValue(layer.value(QStringLiteral("text.color")), QColor(255, 255, 255, 255));
    const QColor strokeColor = jsonColorValue(layer.value(QStringLiteral("text.strokeColor")), QColor(0, 0, 0, 255));
    const QColor shadowColor = jsonColorValue(layer.value(QStringLiteral("text.shadowColor")), QColor(0, 0, 0, 128));
    const bool strokeEnabled = layer.value(QStringLiteral("text.strokeEnabled")).toBool(false);
    const bool shadowEnabled = layer.value(QStringLiteral("text.shadowEnabled")).toBool(false);
    const double strokeWidth = std::max(0.0, layer.value(QStringLiteral("text.strokeWidth")).toDouble(0.0));
    const double shadowOffsetX = layer.value(QStringLiteral("text.shadowOffsetX")).toDouble(0.0);
    const double shadowOffsetY = layer.value(QStringLiteral("text.shadowOffsetY")).toDouble(0.0);
    const double shadowBlur = layer.value(QStringLiteral("text.shadowBlur")).toDouble(0.0);
    const int margin = std::max(4, static_cast<int>(std::ceil(std::max({strokeWidth, std::abs(shadowOffsetX), std::abs(shadowOffsetY), shadowBlur}) + 8.0)));

    QImage fillImage = rasterizeTextLayer(layer, fillColor, margin, nullptr);
    if (fillImage.isNull()) {
        return false;
    }

    painter->save();
    painter->translate(x, y);
    painter->rotate(rotation);
    painter->scale(scaleX, scaleY);
    painter->translate(-anchorX, -anchorY);
    painter->setOpacity(opacity);

    if (shadowEnabled) {
        QImage shadowImage = rasterizeTextLayer(layer, shadowColor, margin, nullptr);
        if (!shadowImage.isNull()) {
            painter->drawImage(QPointF(shadowOffsetX, shadowOffsetY), shadowImage);
        }
    }

    if (strokeEnabled && strokeWidth > 0.0) {
        QImage strokeImage = rasterizeTextLayer(layer, strokeColor, margin, nullptr);
        if (!strokeImage.isNull()) {
            const int radius = std::max(1, static_cast<int>(std::ceil(strokeWidth)));
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    if ((dx * dx) + (dy * dy) > radius * radius) {
                        continue;
                    }
                    painter->drawImage(QPointF(dx, dy), strokeImage);
                }
            }
        }
    }

    painter->drawImage(QPointF(0.0, 0.0), fillImage);
    painter->restore();
    return true;
}

bool paintSvgLayer(QPainter* painter, const QJsonObject& layer, const RenderExecutionOptions& options)
{
    if (!painter) {
        return false;
    }

    QString error;
    QImage svgImage = rasterizeSvgLayer(layer, options, &error);
    if (svgImage.isNull()) {
        return false;
    }

    const double opacity = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0);
    const QJsonObject transform = layer.value(QStringLiteral("transform")).toObject();
    const double x = transform.value(QStringLiteral("px")).toDouble(0.0);
    const double y = transform.value(QStringLiteral("py")).toDouble(0.0);
    const double anchorX = transform.value(QStringLiteral("ax")).toDouble(0.0);
    const double anchorY = transform.value(QStringLiteral("ay")).toDouble(0.0);
    const double rotation = transform.value(QStringLiteral("rx")).toDouble(0.0);
    const double scaleX = transform.value(QStringLiteral("sx")).toDouble(1.0);
    const double scaleY = transform.value(QStringLiteral("sy")).toDouble(1.0);

    painter->save();
    painter->translate(x, y);
    painter->rotate(rotation);
    painter->scale(scaleX, scaleY);
    painter->translate(-anchorX, -anchorY);
    painter->setOpacity(opacity);
    painter->drawImage(QPointF(0.0, 0.0), svgImage);
    painter->restore();
    return true;
}

bool paintImageLayer(QPainter* painter, const QJsonObject& layer, const RenderExecutionOptions& options)
{
    if (!painter) {
        return false;
    }

    QString error;
    QImage image = rasterizeImageLayer(layer, options, &error);
    if (image.isNull()) {
        return false;
    }

    const bool fitToLayer = layer.value(QStringLiteral("image.fitToLayer")).toBool(true);
    const int sourceWidth = layer.value(QStringLiteral("image.width")).toInt(image.width());
    const int sourceHeight = layer.value(QStringLiteral("image.height")).toInt(image.height());
    const double opacity = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0);
    const QJsonObject transform = layer.value(QStringLiteral("transform")).toObject();
    const double x = transform.value(QStringLiteral("px")).toDouble(0.0);
    const double y = transform.value(QStringLiteral("py")).toDouble(0.0);
    const double anchorX = transform.value(QStringLiteral("ax")).toDouble(0.0);
    const double anchorY = transform.value(QStringLiteral("ay")).toDouble(0.0);
    const double rotation = transform.value(QStringLiteral("rx")).toDouble(0.0);
    const double scaleX = transform.value(QStringLiteral("sx")).toDouble(1.0);
    const double scaleY = transform.value(QStringLiteral("sy")).toDouble(1.0);

    painter->save();
    painter->translate(x, y);
    painter->rotate(rotation);
    painter->scale(scaleX, scaleY);
    painter->translate(-anchorX, -anchorY);
    painter->setOpacity(opacity);
    painter->drawImage(QRectF(0.0, 0.0, fitToLayer ? image.width() : sourceWidth,
                              fitToLayer ? image.height() : sourceHeight),
                       image);
    painter->restore();
    return true;
}

bool paintVideoLayer(QPainter* painter, const QJsonObject& layer, const RenderExecutionOptions& options)
{
    if (!painter) {
        return false;
    }

    QString error;
    QImage image = rasterizeVideoLayer(layer, options, &error);
    if (image.isNull()) {
        return false;
    }

    const double opacity = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0);
    const QJsonObject transform = layer.value(QStringLiteral("transform")).toObject();
    const double x = transform.value(QStringLiteral("px")).toDouble(0.0);
    const double y = transform.value(QStringLiteral("py")).toDouble(0.0);
    const double anchorX = transform.value(QStringLiteral("ax")).toDouble(0.0);
    const double anchorY = transform.value(QStringLiteral("ay")).toDouble(0.0);
    const double rotation = transform.value(QStringLiteral("rx")).toDouble(0.0);
    const double scaleX = transform.value(QStringLiteral("sx")).toDouble(1.0);
    const double scaleY = transform.value(QStringLiteral("sy")).toDouble(1.0);

    painter->save();
    painter->translate(x, y);
    painter->rotate(rotation);
    painter->scale(scaleX, scaleY);
    painter->translate(-anchorX, -anchorY);
    painter->setOpacity(opacity);
    painter->drawImage(QRectF(0.0, 0.0, image.width(), image.height()), image);
    painter->restore();
    return true;
}

QImage rasterizeAudioLayer(const QJsonObject& layer, const RenderExecutionOptions& options, QString* errorOut);

bool paintAudioLayer(QPainter* painter, const QJsonObject& layer, const RenderExecutionOptions& options)
{
    if (!painter) {
        return false;
    }

    QString error;
    QImage image = rasterizeAudioLayer(layer, options, &error);
    if (image.isNull()) {
        return false;
    }

    const double opacity = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0);
    const QJsonObject transform = layer.value(QStringLiteral("transform")).toObject();
    const double x = transform.value(QStringLiteral("px")).toDouble(0.0);
    const double y = transform.value(QStringLiteral("py")).toDouble(0.0);
    const double anchorX = transform.value(QStringLiteral("ax")).toDouble(0.0);
    const double anchorY = transform.value(QStringLiteral("ay")).toDouble(0.0);
    const double rotation = transform.value(QStringLiteral("rx")).toDouble(0.0);
    const double scaleX = transform.value(QStringLiteral("sx")).toDouble(1.0);
    const double scaleY = transform.value(QStringLiteral("sy")).toDouble(1.0);

    painter->save();
    painter->translate(x, y);
    painter->rotate(rotation);
    painter->scale(scaleX, scaleY);
    painter->translate(-anchorX, -anchorY);
    painter->setOpacity(opacity);
    painter->drawImage(QRectF(0.0, 0.0, image.width(), image.height()), image);
    painter->restore();
    return true;
}

QSizeF diagnosticLayerSize(const QJsonObject& layer, const QSizeF& fallback)
{
    const auto widthHeightFrom = [&layer](const char* widthKey, const char* heightKey) -> std::optional<QSizeF> {
        if (!layer.contains(QString::fromLatin1(widthKey)) && !layer.contains(QString::fromLatin1(heightKey))) {
            return std::nullopt;
        }
        const double width = std::max(1.0, layer.value(QString::fromLatin1(widthKey)).toDouble(0.0));
        const double height = std::max(1.0, layer.value(QString::fromLatin1(heightKey)).toDouble(0.0));
        return QSizeF(width, height);
    };

    if (auto size = widthHeightFrom("solidWidth", "solidHeight")) return *size;
    if (auto size = widthHeightFrom("shapeWidth", "shapeHeight")) return *size;
    if (auto size = widthHeightFrom("image.width", "image.height")) return *size;
    if (auto size = widthHeightFrom("video.width", "video.height")) return *size;
    if (auto size = widthHeightFrom("audio.width", "audio.height")) return *size;
    if (auto size = widthHeightFrom("width", "height")) return *size;
    return QSizeF(std::max(1.0, fallback.width()), std::max(1.0, fallback.height()));
}

bool paintUnsupportedLayerPlaceholder(QPainter* painter, const QJsonObject& layer, const QString& kind, const RenderJobSummary& summary)
{
    if (!painter) {
        return false;
    }

    const QSizeF size = diagnosticLayerSize(layer, QSizeF(summary.outputWidth * 0.18, summary.outputHeight * 0.12));
    const double opacity = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0);
    const QJsonObject transform = layer.value(QStringLiteral("transform")).toObject();
    const double x = transform.value(QStringLiteral("px")).toDouble(0.0);
    const double y = transform.value(QStringLiteral("py")).toDouble(0.0);
    const double anchorX = transform.value(QStringLiteral("ax")).toDouble(0.0);
    const double anchorY = transform.value(QStringLiteral("ay")).toDouble(0.0);
    const double rotation = transform.value(QStringLiteral("rx")).toDouble(0.0);
    const double scaleX = transform.value(QStringLiteral("sx")).toDouble(1.0);
    const double scaleY = transform.value(QStringLiteral("sy")).toDouble(1.0);

    painter->save();
    painter->translate(x, y);
    painter->rotate(rotation);
    painter->scale(scaleX, scaleY);
    painter->translate(-anchorX, -anchorY);
    painter->setOpacity(std::max(0.12, opacity));
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(QColor(92, 101, 116, 180), 1.25));
    painter->setBrush(QColor(28, 32, 38, 190));
    painter->drawRoundedRect(QRectF(0.0, 0.0, size.width(), size.height()), 10.0, 10.0);

    QFont labelFont(QStringLiteral("Arial"));
    labelFont.setPixelSize(std::max(11, static_cast<int>(size.height() * 0.16)));
    labelFont.setBold(true);
    painter->setFont(labelFont);
    painter->setPen(QColor(232, 236, 242, 230));
    painter->drawText(QRectF(12.0, 10.0, size.width() - 24.0, size.height() * 0.28),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      kind);

    QFont detailFont(QStringLiteral("Arial"));
    detailFont.setPixelSize(std::max(10, static_cast<int>(size.height() * 0.11)));
    painter->setFont(detailFont);
    painter->setPen(QColor(164, 172, 184, 220));
    painter->drawText(QRectF(12.0, size.height() * 0.28, size.width() - 24.0, size.height() * 0.22),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QStringLiteral("diagnostic placeholder"));

    painter->restore();
    return true;
}

QImage rasterizeAudioLayer(const QJsonObject& layer, const RenderExecutionOptions& options, QString* errorOut)
{
    Q_UNUSED(options);

    const QString sourcePath = layer.value(QStringLiteral("audio.sourcePath")).toString().trimmed();
    const QString displayName = QFileInfo(sourcePath).fileName().isEmpty()
        ? QStringLiteral("audio")
        : QFileInfo(sourcePath).fileName();

    const int width = std::max(240, layer.value(QStringLiteral("audio.width")).toInt(320));
    const int height = std::max(72, layer.value(QStringLiteral("audio.height")).toInt(128));

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(14, 16, 20, 255));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const bool muted = layer.value(QStringLiteral("audio.muted")).toBool(false);
    const double volume = std::clamp(layer.value(QStringLiteral("audio.volume")).toDouble(1.0), 0.0, 1.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 34, 42, 255));
    painter.drawRoundedRect(QRectF(0.0, 0.0, width, height), 14.0, 14.0);

    painter.setPen(QColor(222, 228, 238, 255));
    QFont titleFont(QStringLiteral("Arial"));
    titleFont.setPixelSize(std::max(14, height / 5));
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(QRectF(16.0, 10.0, width - 32.0, height * 0.34),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("AUDIO"));

    QFont infoFont(QStringLiteral("Arial"));
    infoFont.setPixelSize(std::max(10, height / 8));
    painter.setFont(infoFont);
    painter.setPen(QColor(160, 170, 184, 255));
    painter.drawText(QRectF(16.0, height * 0.30, width - 32.0, height * 0.18),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     displayName);
    painter.drawText(QRectF(16.0, height * 0.48, width - 32.0, height * 0.18),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     muted ? QStringLiteral("muted") : QStringLiteral("volume %1").arg(volume, 0, 'f', 2));

    const QColor barColor = muted ? QColor(94, 104, 116, 255) : QColor(74, 162, 255, 255);
    const QColor barAccent = muted ? QColor(124, 132, 144, 255) : QColor(202, 232, 255, 255);
    uint state = qHash(sourcePath.isEmpty() ? QStringLiteral("audio") : sourcePath);
    const int barCount = 28;
    const double barAreaHeight = height * 0.22;
    const double barWidth = std::max(2.0, (width - 34.0) / static_cast<double>(barCount * 2));
    double x = 16.0;
    for (int i = 0; i < barCount; ++i) {
        state = state * 1664525u + 1013904223u;
        const double noise = static_cast<double>((state >> ((i % 5) * 5)) & 0x1F) / 31.0;
        const double level = muted ? 0.25 + noise * 0.35 : 0.35 + noise * (0.55 * volume + 0.15);
        const double barHeight = std::max(4.0, barAreaHeight * level);
        const double y = height - 16.0 - barHeight;
        painter.setBrush((i % 7 == 0) ? barAccent : barColor);
        painter.drawRoundedRect(QRectF(x, y, barWidth, barHeight), 2.0, 2.0);
        x += barWidth * 2.0;
    }

    if (sourcePath.isEmpty() && errorOut) {
        *errorOut = QStringLiteral("Missing audio.sourcePath");
    }

    return image;
}

QPainterPath buildShapePath(const QJsonObject& layer)
{
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kHalfPi = kPi * 0.5;

    const int rawShapeType = layer.value(QStringLiteral("shapeType")).toInt(0);
    const int width = std::max(1, layer.value(QStringLiteral("shapeWidth")).toInt(200));
    const int height = std::max(1, layer.value(QStringLiteral("shapeHeight")).toInt(200));
    const double cornerRadius = layer.value(QStringLiteral("cornerRadius")).toDouble(0.0);
    const int starPoints = std::max(3, layer.value(QStringLiteral("starPoints")).toInt(5));
    const double starInnerRadius = std::clamp(layer.value(QStringLiteral("starInnerRadius")).toDouble(0.382), 0.0, 1.0);
    const int polygonSides = std::max(3, layer.value(QStringLiteral("polygonSides")).toInt(6));

    const QJsonArray customPath = layer.value(QStringLiteral("customPath")).toArray();
    if (!customPath.isEmpty()) {
        QPainterPath path;
        const QJsonObject first = customPath.first().toObject();
        path.moveTo(first.value(QStringLiteral("px")).toDouble(0.0), first.value(QStringLiteral("py")).toDouble(0.0));
        const bool closed = layer.value(QStringLiteral("customPathClosed")).toBool(true);
        for (int i = 0; i < customPath.size(); ++i) {
            const int next = (i + 1) % customPath.size();
            if (!closed && next == 0) {
                break;
            }
            const QJsonObject v0 = customPath.at(i).toObject();
            const QJsonObject v1 = customPath.at(next).toObject();
            const QPointF c1(
                v0.value(QStringLiteral("px")).toDouble(0.0) + v0.value(QStringLiteral("ox")).toDouble(0.0),
                v0.value(QStringLiteral("py")).toDouble(0.0) + v0.value(QStringLiteral("oy")).toDouble(0.0));
            const QPointF c2(
                v1.value(QStringLiteral("px")).toDouble(0.0) + v1.value(QStringLiteral("ix")).toDouble(0.0),
                v1.value(QStringLiteral("py")).toDouble(0.0) + v1.value(QStringLiteral("iy")).toDouble(0.0));
            const QPointF end(
                v1.value(QStringLiteral("px")).toDouble(0.0),
                v1.value(QStringLiteral("py")).toDouble(0.0));
            path.cubicTo(c1, c2, end);
        }
        if (closed) {
            path.closeSubpath();
        }
        return path;
    }

    const QJsonArray customPolygonPoints = layer.value(QStringLiteral("customPolygonPoints")).toArray();
    if (rawShapeType == 3 && customPolygonPoints.size() >= 3) {
        QPainterPath path;
        const QJsonObject first = customPolygonPoints.first().toObject();
        path.moveTo(first.value(QStringLiteral("x")).toDouble(0.0), first.value(QStringLiteral("y")).toDouble(0.0));
        for (int i = 1; i < customPolygonPoints.size(); ++i) {
            const QJsonObject point = customPolygonPoints.at(i).toObject();
            path.lineTo(point.value(QStringLiteral("x")).toDouble(0.0), point.value(QStringLiteral("y")).toDouble(0.0));
        }
        if (layer.value(QStringLiteral("customPolygonClosed")).toBool(true)) {
            path.closeSubpath();
        }
        return path;
    }

    const double w = static_cast<double>(width);
    const double h = static_cast<double>(height);
    const double cx = w * 0.5;
    const double cy = h * 0.5;

    QPainterPath path;
    switch (rawShapeType) {
    case 1:
        path.addEllipse(QRectF(0.0, 0.0, w, h));
        break;
    case 2: {
        const double outerR = std::min(cx, cy);
        const double innerR = outerR * starInnerRadius;
        path.moveTo(cx + outerR * std::cos(-kHalfPi), cy + outerR * std::sin(-kHalfPi));
        const int pointCount = starPoints * 2;
        for (int i = 1; i < pointCount; ++i) {
            const double angle = static_cast<double>(i) * kPi / static_cast<double>(starPoints) - kHalfPi;
            const double radius = (i % 2 == 0) ? outerR : innerR;
            path.lineTo(cx + radius * std::cos(angle), cy + radius * std::sin(angle));
        }
        path.closeSubpath();
        break;
    }
    case 3: {
        const double radius = std::min(cx, cy);
        path.moveTo(cx + radius * std::cos(-kHalfPi), cy + radius * std::sin(-kHalfPi));
        for (int i = 1; i < polygonSides; ++i) {
            const double angle = static_cast<double>(i) * 2.0 * kPi / static_cast<double>(polygonSides) - kHalfPi;
            path.lineTo(cx + radius * std::cos(angle), cy + radius * std::sin(angle));
        }
        path.closeSubpath();
        break;
    }
    case 4:
        path.moveTo(0.0, cy);
        path.lineTo(w, cy);
        break;
    case 5:
        path.moveTo(cx, 0.0);
        path.lineTo(w, h);
        path.lineTo(0.0, h);
        path.closeSubpath();
        break;
    case 6: {
        const double side = std::min(w, h);
        const double left = (w - side) * 0.5;
        const double top = (h - side) * 0.5;
        const double radius = std::clamp(cornerRadius, 0.0, side * 0.5);
        if (radius > 0.0) {
            path.addRoundedRect(QRectF(left, top, side, side), radius, radius);
        } else {
            path.addRect(QRectF(left, top, side, side));
        }
        break;
    }
    default: {
        const double radius = std::clamp(cornerRadius, 0.0, std::min(w, h) * 0.5);
        if (radius > 0.0) {
            path.addRoundedRect(QRectF(0.0, 0.0, w, h), radius, radius);
        } else {
            path.addRect(QRectF(0.0, 0.0, w, h));
        }
        break;
    }
    }

    return path;
}

bool paintShapeLayer(QPainter* painter, const QJsonObject& layer)
{
    if (!painter) {
        return false;
    }

    const QPainterPath path = buildShapePath(layer);
    if (path.isEmpty()) {
        return false;
    }

    const double opacity = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0);
    const QJsonObject transform = layer.value(QStringLiteral("transform")).toObject();
    const double x = transform.value(QStringLiteral("px")).toDouble(0.0);
    const double y = transform.value(QStringLiteral("py")).toDouble(0.0);
    const double anchorX = transform.value(QStringLiteral("ax")).toDouble(0.0);
    const double anchorY = transform.value(QStringLiteral("ay")).toDouble(0.0);
    const double rotation = transform.value(QStringLiteral("rx")).toDouble(0.0);
    const double scaleX = transform.value(QStringLiteral("sx")).toDouble(1.0);
    const double scaleY = transform.value(QStringLiteral("sy")).toDouble(1.0);

    QColor fillColor(
        static_cast<int>(std::clamp(layer.value(QStringLiteral("fillR")).toDouble(1.0), 0.0, 1.0) * 255.0),
        static_cast<int>(std::clamp(layer.value(QStringLiteral("fillG")).toDouble(1.0), 0.0, 1.0) * 255.0),
        static_cast<int>(std::clamp(layer.value(QStringLiteral("fillB")).toDouble(1.0), 0.0, 1.0) * 255.0),
        static_cast<int>(std::clamp(layer.value(QStringLiteral("fillA")).toDouble(1.0) * opacity, 0.0, 1.0) * 255.0));
    QColor strokeColor(
        static_cast<int>(std::clamp(layer.value(QStringLiteral("strokeR")).toDouble(0.0), 0.0, 1.0) * 255.0),
        static_cast<int>(std::clamp(layer.value(QStringLiteral("strokeG")).toDouble(0.0), 0.0, 1.0) * 255.0),
        static_cast<int>(std::clamp(layer.value(QStringLiteral("strokeB")).toDouble(0.0), 0.0, 1.0) * 255.0),
        static_cast<int>(std::clamp(layer.value(QStringLiteral("strokeA")).toDouble(1.0) * opacity, 0.0, 1.0) * 255.0));

    const bool fillEnabled = layer.value(QStringLiteral("fillEnabled")).toBool(true);
    const bool strokeEnabled = layer.value(QStringLiteral("strokeEnabled")).toBool(false);
    const double strokeWidth = layer.value(QStringLiteral("strokeWidth")).toDouble(0.0);

    QPen pen = painter->pen();
    pen.setColor(strokeColor);
    pen.setWidthF(std::max(0.0, strokeWidth));
    switch (layer.value(QStringLiteral("strokeCap")).toInt(0)) {
    case 1: pen.setCapStyle(Qt::RoundCap); break;
    case 2: pen.setCapStyle(Qt::SquareCap); break;
    default: pen.setCapStyle(Qt::FlatCap); break;
    }
    switch (layer.value(QStringLiteral("strokeJoin")).toInt(0)) {
    case 1: pen.setJoinStyle(Qt::RoundJoin); break;
    case 2: pen.setJoinStyle(Qt::BevelJoin); break;
    default: pen.setJoinStyle(Qt::MiterJoin); break;
    }

    painter->save();
    painter->translate(x, y);
    painter->rotate(rotation);
    painter->scale(scaleX, scaleY);
    painter->translate(-anchorX, -anchorY);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setOpacity(1.0);
    painter->setBrush(fillEnabled ? fillColor : Qt::NoBrush);
    painter->setPen((strokeEnabled && strokeWidth > 0.0) ? pen : Qt::NoPen);
    painter->drawPath(path);
    painter->restore();
    return true;
}

QJsonObject findCompositionSnapshotRecursive(const QJsonObject& compositionSnapshot, const QString& compositionId)
{
    if (compositionSnapshot.value(QStringLiteral("id")).toString() == compositionId) {
        return compositionSnapshot;
    }

    const QJsonArray nestedCompositions = compositionSnapshot.value(QStringLiteral("compositions")).toArray();
    for (const QJsonValue& value : nestedCompositions) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject found = findCompositionSnapshotRecursive(value.toObject(), compositionId);
        if (!found.isEmpty()) {
            return found;
        }
    }

    return {};
}

bool paintSnapshotLayers(QImage* image, const RenderJobSummary& summary, const RenderExecutionOptions& options, int frameNumber, FrameRenderResult* stats)
{
    if (!image || image->isNull() || summary.layers.isEmpty()) {
        return false;
    }

    const QColor background = jsonColor(
        summary.compositionSnapshot.value(QStringLiteral("backgroundColor")).toObject(),
        QColor(24, 26, 30, 255));
    image->fill(background.rgba());

    QPainter painter(image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QSizeF compSize = compositionSizeForSummary(summary);
    painter.scale(
        static_cast<double>(image->width()) / compSize.width(),
        static_cast<double>(image->height()) / compSize.height());

    bool painted = false;
    int paintedLayerCount = 0;
    int unsupportedLayerCount = 0;
    auto appendUnique = [](QStringList& list, const QString& value) {
        if (!list.contains(value)) {
            list.append(value);
        }
    };
    for (const QJsonValue& value : summary.layers) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject layer = value.toObject();
        if (!layer.value(QStringLiteral("isVisible")).toBool(true)) {
            continue;
        }

        const int inPoint = layer.value(QStringLiteral("inPoint")).toInt(0);
        const int outPoint = layer.value(QStringLiteral("outPoint")).toInt(std::max(frameNumber + 1, 1));
        if (frameNumber < inPoint || frameNumber >= outPoint) {
            continue;
        }

        const int layerType = layer.value(QStringLiteral("type")).toInt(-1);
        const QString kind = layerKindName(layer);

        if (layerType == 3) {
            const int width = std::max(1, layer.value(QStringLiteral("solidWidth")).toInt(summary.outputWidth));
            const int height = std::max(1, layer.value(QStringLiteral("solidHeight")).toInt(summary.outputHeight));
            const QJsonObject transform = layer.value(QStringLiteral("transform")).toObject();
            const double x = transform.value(QStringLiteral("px")).toDouble(0.0);
            const double y = transform.value(QStringLiteral("py")).toDouble(0.0);
            const double anchorX = transform.value(QStringLiteral("ax")).toDouble(0.0);
            const double anchorY = transform.value(QStringLiteral("ay")).toDouble(0.0);
            const double rotation = transform.value(QStringLiteral("rx")).toDouble(0.0);
            const double scaleX = transform.value(QStringLiteral("sx")).toDouble(1.0);
            const double scaleY = transform.value(QStringLiteral("sy")).toDouble(1.0);
            const double opacity = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0);

            QColor color = jsonColor(layer.value(QStringLiteral("solidColor")).toObject(), QColor(255, 255, 255, 255));
            color.setAlphaF(std::clamp(color.alphaF() * opacity, 0.0, 1.0));
            painter.save();
            painter.translate(x, y);
            painter.rotate(rotation);
            painter.scale(scaleX, scaleY);
            painter.translate(-anchorX, -anchorY);
            painter.fillRect(QRectF(0.0, 0.0, width, height), color);
            painter.restore();
            painted = true;
            ++paintedLayerCount;
            if (stats) {
                appendUnique(stats->paintedLayerKinds, kind);
            }
            continue;
        }

        if (layerType == 6) {
            const bool textPainted = paintTextLayer(&painter, layer);
            painted = textPainted || painted;
            if (textPainted) {
                ++paintedLayerCount;
                if (stats) {
                    appendUnique(stats->paintedLayerKinds, kind);
                }
            } else {
                ++unsupportedLayerCount;
                if (stats) {
                    appendUnique(stats->unsupportedLayerKinds, kind);
                }
            }
            continue;
        }

        if (layerType == 7 && layer.contains(QStringLiteral("svg.sourcePath"))) {
            const bool svgPainted = paintSvgLayer(&painter, layer, options);
            painted = svgPainted || painted;
            if (svgPainted) {
                ++paintedLayerCount;
                if (stats) {
                    appendUnique(stats->paintedLayerKinds, QStringLiteral("SVG"));
                }
            } else {
                ++unsupportedLayerCount;
                if (stats) {
                    appendUnique(stats->unsupportedLayerKinds, QStringLiteral("SVG"));
                }
            }
            continue;
        }

        if (layerType == 7) {
            const bool shapePainted = paintShapeLayer(&painter, layer);
            painted = shapePainted || painted;
            if (shapePainted) {
                ++paintedLayerCount;
                if (stats) {
                    appendUnique(stats->paintedLayerKinds, QStringLiteral("Shape"));
                }
            } else {
                ++unsupportedLayerCount;
                if (stats) {
                    appendUnique(stats->unsupportedLayerKinds, QStringLiteral("Shape"));
                }
            }
            continue;
        }

        if (layerType == 8) {
            const QString sourceId = layer.value(QStringLiteral("composition.sourceId")).toString().trimmed();
            if (sourceId.isEmpty()) {
                ++unsupportedLayerCount;
                if (stats) {
                    appendUnique(stats->unsupportedLayerKinds, kind);
                }
                continue;
            }

            const QJsonObject childComposition = findCompositionSnapshotRecursive(summary.compositionSnapshot, sourceId);
            if (childComposition.isEmpty()) {
                ++unsupportedLayerCount;
                if (stats) {
                    appendUnique(stats->unsupportedLayerKinds, kind);
                }
                continue;
            }

            const int childWidth = std::max(1, childComposition.value(QStringLiteral("width")).toInt(summary.outputWidth));
            const int childHeight = std::max(1, childComposition.value(QStringLiteral("height")).toInt(summary.outputHeight));
            QImage childImage(childWidth, childHeight, QImage::Format_ARGB32);
            RenderJobSummary childSummary = summary;
            childSummary.compositionSnapshot = childComposition;
            childSummary.layers = childComposition.value(QStringLiteral("layers")).toArray();
            childSummary.outputWidth = childWidth;
            childSummary.outputHeight = childHeight;

            FrameRenderResult childStats;
            const bool childPainted = paintSnapshotLayers(&childImage, childSummary, options, frameNumber, &childStats);
            (void)childPainted;

            painter.save();
            const QJsonObject transform = layer.value(QStringLiteral("transform")).toObject();
            const double x = transform.value(QStringLiteral("px")).toDouble(0.0);
            const double y = transform.value(QStringLiteral("py")).toDouble(0.0);
            const double anchorX = transform.value(QStringLiteral("ax")).toDouble(0.0);
            const double anchorY = transform.value(QStringLiteral("ay")).toDouble(0.0);
            const double rotation = transform.value(QStringLiteral("rx")).toDouble(0.0);
            const double scaleX = transform.value(QStringLiteral("sx")).toDouble(1.0);
            const double scaleY = transform.value(QStringLiteral("sy")).toDouble(1.0);
            const double opacity = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0);
            painter.translate(x, y);
            painter.rotate(rotation);
            painter.scale(scaleX, scaleY);
            painter.translate(-anchorX, -anchorY);
            painter.setOpacity(opacity);
            painter.drawImage(QRectF(0.0, 0.0, childImage.width(), childImage.height()), childImage);
            painter.restore();

            painted = true;
            ++paintedLayerCount;
            if (stats) {
                appendUnique(stats->paintedLayerKinds, QStringLiteral("Precomp"));
                for (const QString& childKind : childStats.paintedLayerKinds) {
                    appendUnique(stats->paintedLayerKinds, childKind);
                }
                for (const QString& childKind : childStats.unsupportedLayerKinds) {
                    appendUnique(stats->unsupportedLayerKinds, childKind);
                }
            }
            paintedLayerCount += childStats.paintedLayerCount;
            unsupportedLayerCount += childStats.unsupportedLayerCount;
            continue;
        }

        if (layerType == 4 && layer.contains(QStringLiteral("image.sourcePath"))) {
            const bool imagePainted = paintImageLayer(&painter, layer, options);
            painted = imagePainted || painted;
            if (imagePainted) {
                ++paintedLayerCount;
                if (stats) {
                    appendUnique(stats->paintedLayerKinds, QStringLiteral("Image"));
                }
            } else {
                ++unsupportedLayerCount;
                if (stats) {
                    appendUnique(stats->unsupportedLayerKinds, QStringLiteral("Image"));
                }
            }
            continue;
        }

        if (layerType == 10) {
            const bool videoPainted = paintVideoLayer(&painter, layer, options);
            painted = videoPainted || painted;
            if (videoPainted) {
                ++paintedLayerCount;
                if (stats) {
                    appendUnique(stats->paintedLayerKinds, QStringLiteral("Video"));
                }
            } else {
                ++unsupportedLayerCount;
                if (stats) {
                    appendUnique(stats->unsupportedLayerKinds, QStringLiteral("Video"));
                }
            }
            continue;
        }

        if (layerType == 9) {
            const bool audioPainted = paintAudioLayer(&painter, layer, options);
            painted = audioPainted || painted;
            if (audioPainted) {
                ++paintedLayerCount;
                if (stats) {
                    appendUnique(stats->paintedLayerKinds, QStringLiteral("Audio"));
                }
            } else {
                ++unsupportedLayerCount;
                if (stats) {
                    appendUnique(stats->unsupportedLayerKinds, QStringLiteral("Audio"));
                }
            }
            continue;
        }

        ++unsupportedLayerCount;
        if (stats) {
            appendUnique(stats->unsupportedLayerKinds, kind);
        }
        painted = paintUnsupportedLayerPlaceholder(&painter, layer, kind, summary) || painted;
    }

    if (stats) {
        stats->paintedLayerCount = paintedLayerCount;
        stats->unsupportedLayerCount = unsupportedLayerCount;
    }

    return painted;
}

} // namespace

QJsonObject FrameRenderResult::toJson() const
{
    QJsonArray paintedKinds;
    for (const QString& kind : paintedLayerKinds) {
        paintedKinds.append(kind);
    }

    QJsonArray unsupportedKinds;
    for (const QString& kind : unsupportedLayerKinds) {
        unsupportedKinds.append(kind);
    }

    QJsonObject root;
    root.insert(QStringLiteral("ok"), ok);
    root.insert(QStringLiteral("canceled"), canceled);
    root.insert(QStringLiteral("cacheHit"), cacheHit);
    root.insert(QStringLiteral("frameNumber"), frameNumber);
    root.insert(QStringLiteral("attempts"), attempts);
    root.insert(QStringLiteral("paintedLayerCount"), paintedLayerCount);
    root.insert(QStringLiteral("unsupportedLayerCount"), unsupportedLayerCount);
    root.insert(QStringLiteral("paintedLayerKinds"), paintedKinds);
    root.insert(QStringLiteral("unsupportedLayerKinds"), unsupportedKinds);
    root.insert(QStringLiteral("backend"), backend);
    root.insert(QStringLiteral("outputFile"), outputFile);
    if (!errorMessage.isEmpty()) {
        root.insert(QStringLiteral("error"), errorMessage);
    }
    return root;
}

QJsonObject FrameRangeRenderResult::toJson() const
{
    QJsonArray files;
    for (const QString& file : outputFiles) {
        files.append(file);
    }

    QJsonObject root;
    root.insert(QStringLiteral("ok"), ok);
    root.insert(QStringLiteral("canceled"), canceled);
    root.insert(QStringLiteral("firstFrame"), firstFrame);
    root.insert(QStringLiteral("lastFrame"), lastFrame);
    root.insert(QStringLiteral("framesRendered"), framesRendered);
    root.insert(QStringLiteral("paintedLayerCount"), paintedLayerCount);
    root.insert(QStringLiteral("unsupportedLayerCount"), unsupportedLayerCount);
    root.insert(QStringLiteral("outputFiles"), files);
    if (!errorMessage.isEmpty()) {
        root.insert(QStringLiteral("error"), errorMessage);
    }
    return root;
}

FrameRenderResult ExternalFrameRenderer::renderFirstPngFrame(const RenderJobSummary& summary)
{
    RenderExecutionOptions options;
    options.backend = summary.backend;
    options.retryCount = summary.retryCount;
    options.resumeExistingFrames = summary.cacheMode == QStringLiteral("resume");
    return renderPngFrame(summary, options, summary.frameStart, 1);
}

FrameRangeRenderResult ExternalFrameRenderer::renderPngFrameRange(
    const RenderJobSummary& summary,
    const RenderExecutionOptions& options,
    const std::function<void(const FrameRenderResult&, int, int)>& progressCallback)
{
    FrameRangeRenderResult rangeResult;
    rangeResult.firstFrame = summary.frameStart;
    rangeResult.lastFrame = summary.frameStart;

    const int frameCount = std::max(1, summary.frameEnd - summary.frameStart);
    if (summary.outputFormat.compare(QStringLiteral("png"), Qt::CaseInsensitive) != 0) {
        rangeResult.errorMessage = QStringLiteral("Unsupported Phase 4 output format: %1").arg(summary.outputFormat);
        return rangeResult;
    }

    const QString backend = normalizeBackend(options.backend);
    if (backend != QStringLiteral("diagnostic")) {
        rangeResult.errorMessage = QStringLiteral("Unsupported Phase 4 renderer backend: %1").arg(options.backend);
        return rangeResult;
    }

    for (int i = 0; i < frameCount; ++i) {
        if (!options.cancelFile.trimmed().isEmpty() && QFileInfo::exists(options.cancelFile)) {
            rangeResult.ok = false;
            rangeResult.canceled = true;
            rangeResult.errorMessage = QStringLiteral("Render canceled by sentinel file: %1").arg(options.cancelFile);
            return rangeResult;
        }

        const int frameNumber = summary.frameStart + i;
        FrameRenderResult frameResult = renderPngFrame(summary, options, frameNumber, frameCount);
        if (!frameResult.ok) {
            rangeResult.errorMessage = frameResult.errorMessage;
            return rangeResult;
        }

        rangeResult.lastFrame = frameNumber;
        rangeResult.framesRendered += 1;
        rangeResult.paintedLayerCount += frameResult.paintedLayerCount;
        rangeResult.unsupportedLayerCount += frameResult.unsupportedLayerCount;
        rangeResult.outputFiles.append(frameResult.outputFile);
        if (progressCallback) {
            progressCallback(frameResult, rangeResult.framesRendered, frameCount);
        }
    }

    rangeResult.ok = true;
    return rangeResult;
}

FrameRenderResult ExternalFrameRenderer::renderPngFrame(
    const RenderJobSummary& summary,
    const RenderExecutionOptions& options,
    int frameNumber,
    int frameCount)
{
    FrameRenderResult result;
    result.frameNumber = frameNumber;
    result.backend = normalizeBackend(options.backend);
    result.outputFile = resolveOutputFile(summary, frameNumber, frameCount);

    if (options.resumeExistingFrames && QFileInfo::exists(result.outputFile)) {
        result.ok = true;
        result.cacheHit = true;
        return result;
    }

    QFileInfo outputInfo(result.outputFile);
    QDir outputDir = outputInfo.dir();
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        result.errorMessage = QStringLiteral("Failed to create output directory: %1").arg(outputDir.absolutePath());
        return result;
    }

    QImage image(std::max(1, summary.outputWidth),
                 std::max(1, summary.outputHeight),
                 QImage::Format_ARGB32);
    if (!paintSnapshotLayers(&image, summary, options, frameNumber, &result)) {
        paintDiagnosticPattern(&image, deriveJobColor(summary));
    }

    const int attempts = std::max(1, options.retryCount + 1);
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        result.attempts = attempt;
        if (image.save(result.outputFile, "PNG")) {
            result.ok = true;
            return result;
        }
    }

    result.errorMessage = QStringLiteral("Failed to write PNG after %1 attempt(s): %2")
        .arg(attempts)
        .arg(result.outputFile);
    return result;
}

QString ExternalFrameRenderer::resolveOutputFile(const RenderJobSummary& summary, int frameNumber, int frameCount)
{
    QFileInfo info(summary.outputPath);
    if (info.suffix().compare(QStringLiteral("png"), Qt::CaseInsensitive) == 0 && frameCount <= 1) {
        return info.absoluteFilePath();
    }

    const bool pathIsPng = info.suffix().compare(QStringLiteral("png"), Qt::CaseInsensitive) == 0;
    const QDir dir(pathIsPng ? info.absolutePath() : summary.outputPath);
    const QString stem = pathIsPng
        ? info.completeBaseName()
        : (summary.compositionId.isEmpty() ? QStringLiteral("frame") : summary.compositionId);
    const QString fileName = QStringLiteral("%1_%2.png")
        .arg(stem)
        .arg(frameNumber, 5, 10, QChar('0'));
    return dir.absoluteFilePath(fileName);
}

} // namespace ArtifactRenderer
