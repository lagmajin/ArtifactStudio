#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <QVector3D>

#include <DiligentCore/Common/interface/BasicMath.hpp>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

import Transform.Hlper;
import Math.Matrix4x4;
import FloatRGBA;
import Image.ImageF32x4_RGBA;
import Animation.Transform3D;
import Shape.Layer;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Layer transform parameters in the AE order:
// T(position) * R(rotation) * S(scale) * T(-anchor).
struct LayerParams {
    double positionX = 0.0;
    double positionY = 0.0;
    double rotation = 0.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double anchorX = 0.0;
    double anchorY = 0.0;
};

// Independent expectation, written from the transform definition instead of
// reusing the helper under test, so a sign or order mistake in the helper
// cannot satisfy both sides of the comparison.
QPointF expectedMappedPoint(const LayerParams& params, const QPointF& point)
{
    const double scaledX = (point.x() - params.anchorX) * params.scaleX;
    const double scaledY = (point.y() - params.anchorY) * params.scaleY;
    const double radians = params.rotation * kPi / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return QPointF(params.positionX + scaledX * c - scaledY * s,
                   params.positionY + scaledX * s + scaledY * c);
}

QVector3D expectedMappedPoint3D(const LayerParams& params, double positionZ,
                                double scaleZ, double anchorZ,
                                const QVector3D& point)
{
    const double scaledX = (point.x() - params.anchorX) * params.scaleX;
    const double scaledY = (point.y() - params.anchorY) * params.scaleY;
    const double scaledZ = (point.z() - anchorZ) * scaleZ;
    const double radians = params.rotation * kPi / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return QVector3D(static_cast<float>(params.positionX + scaledX * c - scaledY * s),
                     static_cast<float>(params.positionY + scaledX * s + scaledY * c),
                     static_cast<float>(positionZ + scaledZ));
}

// Shape content transforms follow the same AE order as makeLayerTransform2D:
// p -> R.S.(p - anchor) + position, so the anchor is the pivot and lands on the
// position.
QPointF expectedShapeContentPoint(const QPointF& point, const QPointF& position,
                                  const QPointF& anchor, double rotation,
                                  const QPointF& scale)
{
    const double radians = rotation * kPi / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    const double scaledX = (point.x() - anchor.x()) * scale.x();
    const double scaledY = (point.y() - anchor.y()) * scale.y();
    return QPointF(position.x() + scaledX * c - scaledY * s,
                   position.y() + scaledX * s + scaledY * c);
}

QRectF expectedShapeContentBounds(const QRectF& source, const QPointF& position,
                                  const QPointF& anchor, double rotation,
                                  const QPointF& scale)
{
    const std::array<QPointF, 4> corners{{
        source.topLeft(), source.topRight(), source.bottomLeft(), source.bottomRight()}};
    const QPointF first =
        expectedShapeContentPoint(corners[0], position, anchor, rotation, scale);
    double minX = first.x();
    double maxX = first.x();
    double minY = first.y();
    double maxY = first.y();
    for (std::size_t i = 1; i < corners.size(); ++i) {
        const QPointF mapped =
            expectedShapeContentPoint(corners[i], position, anchor, rotation, scale);
        minX = std::min(minX, mapped.x());
        maxX = std::max(maxX, mapped.x());
        minY = std::min(minY, mapped.y());
        maxY = std::max(maxY, mapped.y());
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

void expectPointNear(const QPointF& actual, const QPointF& expected, double tolerance)
{
    EXPECT_NEAR(actual.x(), expected.x(), tolerance) << "x";
    EXPECT_NEAR(actual.y(), expected.y(), tolerance) << "y";
}

void expectVectorNear(const QVector3D& actual, const QVector3D& expected, float tolerance)
{
    EXPECT_NEAR(actual.x(), expected.x(), tolerance) << "x";
    EXPECT_NEAR(actual.y(), expected.y(), tolerance) << "y";
    EXPECT_NEAR(actual.z(), expected.z(), tolerance) << "z";
}

QVector3D mapCustomMatrix(const ArtifactCore::Matrix4x4& matrix, const QVector3D& point)
{
    const auto& m = matrix.m;
    return QVector3D(m[0] * point.x() + m[1] * point.y() + m[2] * point.z() + m[3],
                     m[4] * point.x() + m[5] * point.y() + m[6] * point.z() + m[7],
                     m[8] * point.x() + m[9] * point.y() + m[10] * point.z() + m[11]);
}

// Diligent float4x4 is row-major with the translation in row 3, so points are
// transformed as p' = p * M.
Diligent::float3 mapDiligentMatrix(const Diligent::float4x4& matrix,
                                   const Diligent::float3& point)
{
    const auto& m = matrix.m;
    return Diligent::float3(
        point.x * m[0][0] + point.y * m[1][0] + point.z * m[2][0] + m[3][0],
        point.x * m[0][1] + point.y * m[1][1] + point.z * m[2][1] + m[3][1],
        point.x * m[0][2] + point.y * m[1][2] + point.z * m[2][2] + m[3][2]);
}

const std::array<LayerParams, 6>& sampleLayerParams()
{
    static const std::array<LayerParams, 6> params{{
        {120.0, 85.0, 37.0, 1.7, 0.65, 24.0, -13.0},
        {-40.0, 210.0, -137.0, 0.5, 2.4, 0.0, 0.0},
        {512.0, 384.0, 90.0, -1.2, 0.9, 512.0, 384.0},
        {0.0, 0.0, 181.0, 1.0, 1.0, 300.0, -250.0},
        {33.5, -12.25, 0.0, 3.0, 3.0, 16.0, 16.0},
        {1000.0, -1000.0, 255.5, 0.75, 1.25, -640.0, 360.0}}};
    return params;
}

const std::array<QPointF, 4>& sampleProbes()
{
    static const std::array<QPointF, 4> probes{{
        QPointF(0.0, 0.0), QPointF(100.0, -40.0),
        QPointF(-30.0, 250.0), QPointF(1.5, -2.5)}};
    return probes;
}

// --- image capture helpers -------------------------------------------------

constexpr int kCanvasWidth = 220;
constexpr int kCanvasHeight = 160;

QString captureDirectory()
{
    QString directory = qEnvironmentVariable("ARTIFACT_TRANSFORM_ANCHOR_CAPTURE_DIR");
    if (directory.isEmpty()) {
        directory = QDir::tempPath() + QStringLiteral("/artifact-transform-anchor");
    }
    QDir().mkpath(directory);
    return directory;
}

QString baselineDirectory()
{
    QString directory = qEnvironmentVariable("ARTIFACT_TRANSFORM_ANCHOR_BASELINE_DIR");
    if (directory.isEmpty()) {
#ifdef ARTIFACT_TRANSFORM_ANCHOR_BASELINE_DIR
        directory = QString::fromUtf8(ARTIFACT_TRANSFORM_ANCHOR_BASELINE_DIR);
#endif
    }
    return directory;
}

bool updateBaselinesRequested()
{
    return qEnvironmentVariableIntValue("ARTIFACT_TRANSFORM_ANCHOR_UPDATE_BASELINES") == 1;
}

// The comparison runs on the exact PNG payload the test writes, so the stored
// baseline and the generated image go through the same conversion.
QImage toDisplayImage(const cv::Mat& image)
{
    ArtifactCore::ImageF32x4_RGBA wrapped;
    wrapped.setFromCVMat(image);
    return wrapped.toQImage();
}

void savePng(const QImage& image, const QString& name)
{
    const QString path = QDir(captureDirectory()).filePath(name);
    const bool saved = image.save(path, "PNG");
    std::cout << "[transform-anchor] " << (saved ? "wrote " : "failed to write ")
              << path.toStdString() << std::endl;
    EXPECT_TRUE(saved) << "failed to write " << path.toStdString();
}

struct ImageDiffStats {
    double meanAbsoluteDifference = 0.0;
    double differingFraction = 0.0;
    int maxChannelDifference = 0;
    bool sizeMismatch = false;
};

ImageDiffStats compareRgbaImages(const QImage& expected, const QImage& actual,
                                 int channelTolerance)
{
    ImageDiffStats stats;
    if (expected.size() != actual.size()) {
        stats.sizeMismatch = true;
        return stats;
    }

    const QImage left = expected.convertToFormat(QImage::Format_RGBA8888);
    const QImage right = actual.convertToFormat(QImage::Format_RGBA8888);
    const long long pixelCount =
        static_cast<long long>(left.width()) * static_cast<long long>(left.height());

    long long differingPixels = 0;
    long long totalDifference = 0;
    for (int y = 0; y < left.height(); ++y) {
        const uchar* leftRow = left.constScanLine(y);
        const uchar* rightRow = right.constScanLine(y);
        for (int x = 0; x < left.width(); ++x) {
            int pixelMaximum = 0;
            for (int channel = 0; channel < 4; ++channel) {
                const int delta = std::abs(static_cast<int>(leftRow[x * 4 + channel]) -
                                           static_cast<int>(rightRow[x * 4 + channel]));
                pixelMaximum = std::max(pixelMaximum, delta);
                totalDifference += delta;
            }
            stats.maxChannelDifference = std::max(stats.maxChannelDifference, pixelMaximum);
            if (pixelMaximum > channelTolerance) {
                ++differingPixels;
            }
        }
    }

    if (pixelCount > 0) {
        stats.differingFraction =
            static_cast<double>(differingPixels) / static_cast<double>(pixelCount);
        stats.meanAbsoluteDifference =
            static_cast<double>(totalDifference) / static_cast<double>(pixelCount * 4);
    }
    return stats;
}

// Writes the baseline, actual and a magenta-on-gray difference map so a
// mismatch can be inspected without re-running the test.
void writeBaselineDiff(const QString& name, const QImage& expected, const QImage& actual,
                       int channelTolerance)
{
    const QString directory = captureDirectory();
    const QString stem = QFileInfo(name).completeBaseName();
    expected.save(QDir(directory).filePath(
                      QStringLiteral("baseline_%1_expected.png").arg(stem)), "PNG");
    actual.save(QDir(directory).filePath(
                    QStringLiteral("baseline_%1_actual.png").arg(stem)), "PNG");
    if (expected.size() != actual.size()) {
        return;
    }

    const QImage left = expected.convertToFormat(QImage::Format_RGBA8888);
    const QImage right = actual.convertToFormat(QImage::Format_RGBA8888);
    QImage diff(left.size(), QImage::Format_RGBA8888);
    for (int y = 0; y < left.height(); ++y) {
        const uchar* leftRow = left.constScanLine(y);
        const uchar* rightRow = right.constScanLine(y);
        uchar* diffRow = diff.scanLine(y);
        for (int x = 0; x < left.width(); ++x) {
            int pixelMaximum = 0;
            for (int channel = 0; channel < 4; ++channel) {
                pixelMaximum = std::max(
                    pixelMaximum,
                    std::abs(static_cast<int>(leftRow[x * 4 + channel]) -
                             static_cast<int>(rightRow[x * 4 + channel])));
            }
            if (pixelMaximum > channelTolerance) {
                diffRow[x * 4 + 0] = 255;
                diffRow[x * 4 + 1] = 0;
                diffRow[x * 4 + 2] = 255;
                diffRow[x * 4 + 3] = 255;
            } else {
                const uchar gray = static_cast<uchar>(leftRow[x * 4 + 0] / 4);
                diffRow[x * 4 + 0] = gray;
                diffRow[x * 4 + 1] = gray;
                diffRow[x * 4 + 2] = gray;
                diffRow[x * 4 + 3] = 255;
            }
        }
    }
    diff.save(QDir(directory).filePath(
                  QStringLiteral("baseline_%1_diff.png").arg(stem)), "PNG");
}

void verifyAgainstBaseline(const QImage& actual, const QString& name)
{
    const QString directory = baselineDirectory();
    if (directory.isEmpty()) {
        std::cout << "[transform-anchor] baseline directory is not configured; skipping "
                  << name.toStdString() << std::endl;
        return;
    }

    const QString path = QDir(directory).filePath(name);
    if (updateBaselinesRequested()) {
        QDir().mkpath(directory);
        const bool saved = actual.save(path, "PNG");
        std::cout << "[transform-anchor] baseline "
                  << (saved ? "updated: " : "write failed: ") << path.toStdString()
                  << std::endl;
        EXPECT_TRUE(saved) << "failed to write baseline " << path.toStdString();
        return;
    }

    if (!QFileInfo::exists(path)) {
        ADD_FAILURE() << "missing baseline " << path.toStdString()
                      << " (set ARTIFACT_TRANSFORM_ANCHOR_UPDATE_BASELINES=1 to create it)";
        return;
    }

    const QImage expected(path);
    ASSERT_FALSE(expected.isNull()) << "failed to load baseline " << path.toStdString();

    // Anchor mistakes move whole pixels, not single channels, so a tight
    // per-channel tolerance still catches a one-pixel pivot shift.
    constexpr int kChannelTolerance = 2;
    constexpr double kMaximumDifferingFraction = 0.0005;
    constexpr double kMaximumMeanDifference = 0.2;

    const ImageDiffStats stats =
        compareRgbaImages(expected, actual, kChannelTolerance);
    if (stats.sizeMismatch) {
        writeBaselineDiff(name, expected, actual, kChannelTolerance);
        ADD_FAILURE() << "baseline size mismatch for " << name.toStdString();
        return;
    }
    if (stats.differingFraction > kMaximumDifferingFraction ||
        stats.meanAbsoluteDifference > kMaximumMeanDifference) {
        writeBaselineDiff(name, expected, actual, kChannelTolerance);
        ADD_FAILURE() << "baseline mismatch for " << name.toStdString()
                      << ": differingFraction=" << stats.differingFraction
                      << " meanAbs=" << stats.meanAbsoluteDifference
                      << " maxChannelDiff=" << stats.maxChannelDifference
                      << " (expected " << path.toStdString() << ", captures in "
                      << captureDirectory().toStdString() << ")";
    }
}

// Opaque rectangle offset from the anchor plus a dot exactly on the anchor.
// The offset block exposes pivot errors, the dot exposes position errors.
cv::Mat makeMarkerImage(int width, int height, const cv::Point2d& anchor)
{
    cv::Mat image(height, width, CV_32FC4, cv::Scalar(0.0, 0.0, 0.0, 0.0));
    const int anchorX = static_cast<int>(std::lround(anchor.x));
    const int anchorY = static_cast<int>(std::lround(anchor.y));

    const cv::Rect block(anchorX + 18, anchorY - 20, 40, 22);
    image(block).setTo(cv::Scalar(255.0, 160.0, 0.0, 255.0));

    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            image.at<cv::Vec4f>(anchorY + dy, anchorX + dx) =
                cv::Vec4f(0.0f, 255.0f, 0.0f, 255.0f);
        }
    }
    return image;
}

cv::Mat warpWithTransform(const cv::Mat& source, const QTransform& transform)
{
    // Mirrors the affine extraction in Layer2D::transformedLayer()
    // (ArtifactCore/src/Layer/Layer2D.cpp:63-74) so the pixels follow the same
    // 2D path as the layer renderer.
    cv::Mat affine(2, 3, CV_64FC1);
    affine.at<double>(0, 0) = transform.m11();
    affine.at<double>(0, 1) = transform.m21();
    affine.at<double>(0, 2) = transform.m31();
    affine.at<double>(1, 0) = transform.m12();
    affine.at<double>(1, 1) = transform.m22();
    affine.at<double>(1, 2) = transform.m32();

    cv::Mat warped;
    cv::warpAffine(source, warped, affine, source.size(), cv::INTER_NEAREST,
                   cv::BORDER_CONSTANT, cv::Scalar(0.0, 0.0, 0.0, 0.0));
    return warped;
}

cv::Point2d alphaCentroid(const cv::Mat& image)
{
    double sumX = 0.0;
    double sumY = 0.0;
    double sumWeight = 0.0;
    for (int y = 0; y < image.rows; ++y) {
        const cv::Vec4f* row = image.ptr<cv::Vec4f>(y);
        for (int x = 0; x < image.cols; ++x) {
            const double weight = std::max(0.0f, row[x][3]);
            sumX += weight * x;
            sumY += weight * y;
            sumWeight += weight;
        }
    }
    if (sumWeight <= 0.0) {
        return cv::Point2d(-1.0, -1.0);
    }
    return cv::Point2d(sumX / sumWeight, sumY / sumWeight);
}

void expectOpaqueAt(const cv::Mat& image, const QPointF& point)
{
    const int x = static_cast<int>(std::lround(point.x()));
    const int y = static_cast<int>(std::lround(point.y()));
    ASSERT_GE(x, 0);
    ASSERT_LT(x, image.cols);
    ASSERT_GE(y, 0);
    ASSERT_LT(y, image.rows);
    EXPECT_GT(image.at<cv::Vec4f>(y, x)[3], 0.5f)
        << "no opaque pixel at the expected anchor position (" << x << ", " << y << ")";
}

// The arms start two pixels out so the anchor dot stays visible: a baseline that
// paints over the dot would hide a one-pixel pivot shift.
void drawCrosshair(cv::Mat& image, const QPointF& point, const cv::Vec4f& color)
{
    const int cx = static_cast<int>(std::lround(point.x()));
    const int cy = static_cast<int>(std::lround(point.y()));
    for (int distance = 2; distance <= 7; ++distance) {
        for (const int sign : std::array<int, 2>{-1, 1}) {
            const int x = cx + distance * sign;
            const int y = cy + distance * sign;
            if (x >= 0 && x < image.cols && cy >= 0 && cy < image.rows) {
                image.at<cv::Vec4f>(cy, x) = color;
            }
            if (y >= 0 && y < image.rows && cx >= 0 && cx < image.cols) {
                image.at<cv::Vec4f>(y, cx) = color;
            }
        }
    }
}

cv::Mat makeStrip(const cv::Mat& left, const cv::Mat& right)
{
    cv::Mat strip(left.rows, left.cols + right.cols, CV_32FC4,
                  cv::Scalar(0.0, 0.0, 0.0, 0.0));
    left.copyTo(strip(cv::Rect(0, 0, left.cols, left.rows)));
    right.copyTo(strip(cv::Rect(left.cols, 0, right.cols, right.rows)));
    return strip;
}

struct AnchorImageCase {
    cv::Mat source;
    cv::Mat warped;
    QPointF expectedAnchor;
    QPointF expectedCentroid;
};

AnchorImageCase warpMarker(const LayerParams& params)
{
    AnchorImageCase result;
    result.source = makeMarkerImage(kCanvasWidth, kCanvasHeight,
                                    cv::Point2d(params.anchorX, params.anchorY));
    const QTransform transform = ArtifactCore::makeLayerTransform2D(
        params.positionX, params.positionY, params.rotation,
        params.scaleX, params.scaleY, params.anchorX, params.anchorY);
    result.warped = warpWithTransform(result.source, transform);

    const cv::Point2d sourceCentroid = alphaCentroid(result.source);
    result.expectedAnchor =
        expectedMappedPoint(params, QPointF(params.anchorX, params.anchorY));
    result.expectedCentroid =
        expectedMappedPoint(params, QPointF(sourceCentroid.x, sourceCentroid.y));
    return result;
}

AnchorImageCase warpMarkerThroughHierarchy(const LayerParams& child,
                                           const LayerParams& parent)
{
    AnchorImageCase result;
    result.source = makeMarkerImage(kCanvasWidth, kCanvasHeight,
                                    cv::Point2d(child.anchorX, child.anchorY));
    const QTransform local = ArtifactCore::makeLayerTransform2D(
        child.positionX, child.positionY, child.rotation,
        child.scaleX, child.scaleY, child.anchorX, child.anchorY);
    const QTransform parentTransform = ArtifactCore::makeLayerTransform2D(
        parent.positionX, parent.positionY, parent.rotation,
        parent.scaleX, parent.scaleY, parent.anchorX, parent.anchorY);
    result.warped =
        warpWithTransform(result.source, ArtifactCore::combineLayerTransform2D(local, parentTransform));

    const cv::Point2d sourceCentroid = alphaCentroid(result.source);
    result.expectedAnchor = expectedMappedPoint(
        parent, expectedMappedPoint(child, QPointF(child.anchorX, child.anchorY)));
    result.expectedCentroid = expectedMappedPoint(
        parent, expectedMappedPoint(child, QPointF(sourceCentroid.x, sourceCentroid.y)));
    return result;
}

void expectAndSaveAnchorImage(const AnchorImageCase& image, const QString& name)
{
    // The dot sits on the source anchor, so it has to survive on the position.
    expectOpaqueAt(image.warped, image.expectedAnchor);

    // The offset block has to follow R * S about the anchor, not about the origin.
    const cv::Point2d centroid = alphaCentroid(image.warped);
    EXPECT_NEAR(centroid.x, image.expectedCentroid.x(), 2.0)
        << "horizontal centroid drifted from the anchored expectation";
    EXPECT_NEAR(centroid.y, image.expectedCentroid.y(), 2.0)
        << "vertical centroid drifted from the anchored expectation";

    cv::Mat annotated = image.warped.clone();
    drawCrosshair(annotated, image.expectedAnchor, cv::Vec4f(255.0f, 0.0f, 255.0f, 255.0f));

    const QImage display = toDisplayImage(makeStrip(image.source, annotated));
    savePng(display, name);
    verifyAgainstBaseline(display, name);
}

} // namespace

// --- anchored pivot invariants --------------------------------------------

TEST(TransformAnchorContractTest, AnchorPointMapsToPositionForEveryParameterSet)
{
    for (const LayerParams& params : sampleLayerParams()) {
        const QTransform transform = ArtifactCore::makeLayerTransform2D(
            params.positionX, params.positionY, params.rotation,
            params.scaleX, params.scaleY, params.anchorX, params.anchorY);

        expectPointNear(transform.map(QPointF(params.anchorX, params.anchorY)),
                        QPointF(params.positionX, params.positionY), 1.0e-9);
    }
}

TEST(TransformAnchorContractTest, OffAnchorProbesFollowIndependentRotationAndScale)
{
    const auto& params = sampleLayerParams();
    const auto& probes = sampleProbes();
    for (std::size_t i = 0; i < params.size(); ++i) {
        const QTransform transform = ArtifactCore::makeLayerTransform2D(
            params[i].positionX, params[i].positionY, params[i].rotation,
            params[i].scaleX, params[i].scaleY, params[i].anchorX, params[i].anchorY);
        for (std::size_t j = 0; j < probes.size(); ++j) {
            SCOPED_TRACE(::testing::Message() << "params[" << i << "] probe[" << j << "]");
            expectPointNear(transform.map(probes[j]),
                            expectedMappedPoint(params[i], probes[j]), 1.0e-9);
        }
    }
}

TEST(TransformAnchorContractTest, NegativeAndNonUniformScaleKeepTheAnchorAsPivot)
{
    const LayerParams mirrored{60.0, -20.0, 0.0, -1.4, 0.6, 12.0, 8.0};
    const QTransform alignment = ArtifactCore::makeLayerTransform2D(
        mirrored.positionX, mirrored.positionY, mirrored.rotation,
        mirrored.scaleX, mirrored.scaleY, mirrored.anchorX, mirrored.anchorY);

    expectPointNear(alignment.map(QPointF(12.0, 8.0)), QPointF(60.0, -20.0), 1.0e-9);
    // A point to the right of the anchor must end up to the LEFT of the position
    // because scaleX is negative, and it must stay on the same scanline.
    expectPointNear(alignment.map(QPointF(22.0, 8.0)), QPointF(46.0, -20.0), 1.0e-9);
    // A point below the anchor must end up below the position scaled by 0.6.
    expectPointNear(alignment.map(QPointF(12.0, 18.0)), QPointF(60.0, -14.0), 1.0e-9);

    const LayerParams mirroredRotated{60.0, -20.0, 51.0, -1.4, 0.6, 12.0, 8.0};
    const QTransform rotated = ArtifactCore::makeLayerTransform2D(
        mirroredRotated.positionX, mirroredRotated.positionY, mirroredRotated.rotation,
        mirroredRotated.scaleX, mirroredRotated.scaleY,
        mirroredRotated.anchorX, mirroredRotated.anchorY);
    for (const QPointF& probe : sampleProbes()) {
        expectPointNear(rotated.map(probe),
                        expectedMappedPoint(mirroredRotated, probe), 1.0e-9);
    }
}

TEST(TransformAnchorContractTest, ZeroAnchorMatchesPlainTranslateRotateScale)
{
    const LayerParams params{45.0, -12.0, -73.0, 2.25, 0.4, 0.0, 0.0};

    QTransform manual;
    manual.translate(params.positionX, params.positionY);
    manual.rotate(params.rotation);
    manual.scale(params.scaleX, params.scaleY);

    const QTransform helper = ArtifactCore::makeLayerTransform2D(
        params.positionX, params.positionY, params.rotation,
        params.scaleX, params.scaleY, params.anchorX, params.anchorY);

    for (const QPointF& probe : sampleProbes()) {
        expectPointNear(helper.map(probe), manual.map(probe), 1.0e-9);
    }
}

TEST(TransformAnchorContractTest, ChildAnchorMapsThroughTheParentTransform)
{
    const LayerParams child{40.0, 10.0, 25.0, 1.2, 0.9, 18.0, -6.0};
    const LayerParams parent{100.0, 50.0, 37.0, 1.4, 0.8, 200.0, -75.0};

    const QTransform local = ArtifactCore::makeLayerTransform2D(
        child.positionX, child.positionY, child.rotation,
        child.scaleX, child.scaleY, child.anchorX, child.anchorY);
    const QTransform parentTransform = ArtifactCore::makeLayerTransform2D(
        parent.positionX, parent.positionY, parent.rotation,
        parent.scaleX, parent.scaleY, parent.anchorX, parent.anchorY);
    const QTransform combined = ArtifactCore::combineLayerTransform2D(local, parentTransform);

    // The child anchor is carried to the child position by the local transform,
    // and the parent then moves that position. The parent anchor must not leak
    // into where the child pivot lands.
    expectPointNear(combined.map(QPointF(child.anchorX, child.anchorY)),
                    expectedMappedPoint(parent, QPointF(child.positionX, child.positionY)),
                    1.0e-9);

    for (const QPointF& probe : sampleProbes()) {
        expectPointNear(combined.map(probe),
                        parentTransform.map(local.map(probe)), 1.0e-9);
    }
}

TEST(TransformAnchorContractTest, ThreeLevelAnchoredHierarchyMatchesSequentialMapping)
{
    const LayerParams grandParent{60.0, -30.0, 12.0, 1.35, 1.35, 5.0, 5.0};
    const LayerParams parent{-14.0, 22.0, -96.0, 0.8, 1.6, 300.0, 40.0};
    const LayerParams child{9.0, -7.0, 240.0, 1.1, 1.1, -20.0, 12.0};

    const QTransform grandParentTransform = ArtifactCore::makeLayerTransform2D(
        grandParent.positionX, grandParent.positionY, grandParent.rotation,
        grandParent.scaleX, grandParent.scaleY, grandParent.anchorX, grandParent.anchorY);
    const QTransform parentTransform = ArtifactCore::makeLayerTransform2D(
        parent.positionX, parent.positionY, parent.rotation,
        parent.scaleX, parent.scaleY, parent.anchorX, parent.anchorY);
    const QTransform childTransform = ArtifactCore::makeLayerTransform2D(
        child.positionX, child.positionY, child.rotation,
        child.scaleX, child.scaleY, child.anchorX, child.anchorY);

    const QTransform childAndParent =
        ArtifactCore::combineLayerTransform2D(childTransform, parentTransform);
    const QTransform combined =
        ArtifactCore::combineLayerTransform2D(childAndParent, grandParentTransform);

    for (const QPointF& probe : sampleProbes()) {
        const QPointF sequential =
            grandParentTransform.map(parentTransform.map(childTransform.map(probe)));
        expectPointNear(combined.map(probe), sequential, 1.0e-9);
    }

    expectPointNear(combined.map(QPointF(child.anchorX, child.anchorY)),
                    expectedMappedPoint(
                        grandParent,
                        expectedMappedPoint(parent, QPointF(child.positionX, child.positionY))),
                    1.0e-9);
}

// --- 3D and cross-path agreement ------------------------------------------

TEST(TransformAnchorContractTest, ThreeDimensionalAnchorMapsToPositionIncludingDepth)
{
    const QMatrix4x4 transform = ArtifactCore::makeLayerTransform3D(
        120.0, 85.0, -30.0, 37.0, 1.7, 0.65, 2.2, 24.0, -13.0, 7.0);

    const QVector3D anchor(24.0f, -13.0f, 7.0f);
    expectVectorNear(transform.map(anchor), QVector3D(120.0f, 85.0f, -30.0f), 1.0e-3f);

    const LayerParams planar{120.0, 85.0, 37.0, 1.7, 0.65, 24.0, -13.0};
    const QVector3D offset(24.0f + 10.0f, -13.0f - 6.0f, 7.0f + 4.0f);
    expectVectorNear(transform.map(offset),
                     expectedMappedPoint3D(planar, -30.0, 2.2, 7.0, offset), 1.0e-3f);
}

TEST(TransformAnchorContractTest, CustomMatrixAndQtPathsAgreeForAnchorsAcrossParameterGrid)
{
    const std::array<double, 6> rotations{{-137.0, -45.0, 0.0, 33.0, 90.0, 181.0}};
    const std::array<double, 4> scalesX{{-2.2, 0.5, 1.0, 1.75}};
    const std::array<double, 4> scalesY{{-0.75, 0.3, 1.0, 2.4}};
    const std::array<QPointF, 3> anchors{{
        QPointF(0.0, 0.0), QPointF(24.0, -13.0), QPointF(160.0, 120.0)}};

    for (const double rotation : rotations) {
        for (const double scaleX : scalesX) {
            for (const double scaleY : scalesY) {
                for (const QPointF& anchor : anchors) {
                    SCOPED_TRACE(::testing::Message()
                                 << "rotation=" << rotation << " scaleX=" << scaleX
                                 << " scaleY=" << scaleY << " anchor=(" << anchor.x()
                                 << ", " << anchor.y() << ")");

                    const QMatrix4x4 qt = ArtifactCore::makeLayerTransform3D(
                        31.0, -17.0, 5.0, rotation, scaleX, scaleY, 1.0,
                        anchor.x(), anchor.y(), 0.0);
                    const ArtifactCore::Matrix4x4 custom = ArtifactCore::makeLayerMatrix4x4(
                        31.0, -17.0, 5.0, rotation, scaleX, scaleY, 1.0,
                        anchor.x(), anchor.y(), 0.0);

                    const std::array<QVector3D, 2> probes{{
                        QVector3D(static_cast<float>(anchor.x()),
                                  static_cast<float>(anchor.y()), 0.0f),
                        QVector3D(static_cast<float>(anchor.x()) + 31.0f,
                                  static_cast<float>(anchor.y()) + 9.0f, 11.0f)}};

                    for (const QVector3D& probe : probes) {
                        expectVectorNear(mapCustomMatrix(custom, probe), qt.map(probe),
                                         1.0e-3f);
                    }
                }
            }
        }
    }
}

TEST(TransformAnchorContractTest, TwoDimensionalAndCustomMatrixAgreeAcrossParameterGrid)
{
    const std::array<double, 6> rotations{{-137.0, -45.0, 0.0, 33.0, 90.0, 181.0}};
    const std::array<double, 4> scalesX{{-2.2, 0.5, 1.0, 1.75}};
    const std::array<double, 4> scalesY{{-0.75, 0.3, 1.0, 2.4}};
    const std::array<QPointF, 3> anchors{{
        QPointF(0.0, 0.0), QPointF(24.0, -13.0), QPointF(160.0, 120.0)}};

    for (const double rotation : rotations) {
        for (const double scaleX : scalesX) {
            for (const double scaleY : scalesY) {
                for (const QPointF& anchor : anchors) {
                    SCOPED_TRACE(::testing::Message()
                                 << "rotation=" << rotation << " scaleX=" << scaleX
                                 << " scaleY=" << scaleY << " anchor=(" << anchor.x()
                                 << ", " << anchor.y() << ")");

                    const QTransform planar = ArtifactCore::makeLayerTransform2D(
                        31.0, -17.0, rotation, scaleX, scaleY, anchor.x(), anchor.y());
                    const ArtifactCore::Matrix4x4 custom =
                        ArtifactCore::makeLayerMatrix4x4(
                            31.0, -17.0, 0.0, rotation, scaleX, scaleY, 1.0,
                            anchor.x(), anchor.y(), 0.0);

                    for (const QPointF& probe : sampleProbes()) {
                        const QVector3D mapped = mapCustomMatrix(
                            custom, QVector3D(static_cast<float>(probe.x()),
                                              static_cast<float>(probe.y()), 0.0f));
                        expectPointNear(planar.map(probe),
                                        QPointF(mapped.x(), mapped.y()), 1.0e-3);
                    }
                }
            }
        }
    }
}

TEST(TransformAnchorContractTest, ShapeLayerContentTransformUsesTheLayerTransformOrder)
{
    const QRectF source(10.0, 20.0, 30.0, 40.0);
    const QPointF position(200.0, 100.0);
    const QPointF anchor(50.0, 25.0);

    // The untransformed bounds include the shape's stroke padding, so measure
    // them instead of assuming the source rectangle.
    const QRectF base = ArtifactCore::ShapeLayer::createRectangle(source).boundingRect();

    ArtifactCore::ShapeLayer layer = ArtifactCore::ShapeLayer::createRectangle(source);
    layer.setAnchorPoint({anchor.x(), anchor.y()});
    layer.setPosition({position.x(), position.y()});

    // Without rotation or scale the content moves by position - anchor.
    const QRectF translated = layer.boundingRect();
    const QRectF expectedTranslated = base.translated(position.x() - anchor.x(),
                                                      position.y() - anchor.y());
    EXPECT_NEAR(translated.x(), expectedTranslated.x(), 1.0e-9);
    EXPECT_NEAR(translated.y(), expectedTranslated.y(), 1.0e-9);
    EXPECT_NEAR(translated.width(), expectedTranslated.width(), 1.0e-9);
    EXPECT_NEAR(translated.height(), expectedTranslated.height(), 1.0e-9);

    layer.setRotation(45.0);
    layer.setScale({1.5, 0.5});
    const QRectF expected =
        expectedShapeContentBounds(base, position, anchor, 45.0, QPointF(1.5, 0.5));
    const QRectF actual = layer.boundingRect();
    EXPECT_NEAR(actual.x(), expected.x(), 1.0e-9);
    EXPECT_NEAR(actual.y(), expected.y(), 1.0e-9);
    EXPECT_NEAR(actual.width(), expected.width(), 1.0e-9);
    EXPECT_NEAR(actual.height(), expected.height(), 1.0e-9);
}

TEST(TransformAnchorContractTest, AnchorFarOutsideTheLayerStillDefinesThePivot)
{
    const LayerParams params{-320.0, 480.0, -63.0, 2.5, 0.4, -1200.0, 900.0};
    const QTransform transform = ArtifactCore::makeLayerTransform2D(
        params.positionX, params.positionY, params.rotation,
        params.scaleX, params.scaleY, params.anchorX, params.anchorY);

    expectPointNear(transform.map(QPointF(params.anchorX, params.anchorY)),
                    QPointF(params.positionX, params.positionY), 1.0e-9);
    for (const QPointF& probe : sampleProbes()) {
        expectPointNear(transform.map(probe), expectedMappedPoint(params, probe), 1.0e-9);
    }
}

TEST(TransformAnchorContractTest, TwoDimensionalAndThreeDimensionalPathsAgreeOnTheZPlane)
{
    for (const LayerParams& params : sampleLayerParams()) {
        const QTransform planar = ArtifactCore::makeLayerTransform2D(
            params.positionX, params.positionY, params.rotation,
            params.scaleX, params.scaleY, params.anchorX, params.anchorY);
        const QMatrix4x4 spatial = ArtifactCore::makeLayerTransform3D(
            params.positionX, params.positionY, 0.0, params.rotation,
            params.scaleX, params.scaleY, 1.0, params.anchorX, params.anchorY, 0.0);

        for (const QPointF& probe : sampleProbes()) {
            const QVector3D mapped =
                spatial.map(QVector3D(static_cast<float>(probe.x()),
                                      static_cast<float>(probe.y()), 0.0f));
            expectPointNear(planar.map(probe),
                            QPointF(mapped.x(), mapped.y()), 1.0e-3);
        }
    }
}

// AnimatableTransform3D stores Diligent row-vector matrices, so the anchored
// matrix has to agree with makeLayerTransform2D / makeLayerTransform3D /
// makeLayerMatrix4x4: the anchor is the pivot and lands on the position.
TEST(TransformAnchorContractTest, AnimatedMatrixAgreesWithTheOtherTransformPaths)
{
    ArtifactCore::AnimatableTransform3D transform;
    transform.setCurrentPosition(120.0f, 85.0f);
    transform.setCurrentPositionZ(-30.0f);
    transform.setCurrentAnchor(24.0f, -13.0f, 0.0f);
    transform.setCurrentRotation(37.0f);
    transform.setCurrentScale(1.7f, 0.65f);

    const LayerParams params{120.0, 85.0, 37.0, 1.7, 0.65, 24.0, -13.0};
    const Diligent::float4x4 anchoredMatrix = transform.getAllMatrix();

    const Diligent::float3 mappedAnchor = mapDiligentMatrix(
        anchoredMatrix, Diligent::float3(24.0f, -13.0f, 0.0f));
    expectPointNear(QPointF(mappedAnchor.x, mappedAnchor.y),
                    QPointF(120.0, 85.0), 1.0e-2);

    for (const QPointF& probe : sampleProbes()) {
        const Diligent::float3 mapped = mapDiligentMatrix(
            anchoredMatrix,
            Diligent::float3(static_cast<float>(probe.x()),
                             static_cast<float>(probe.y()), 0.0f));
        expectPointNear(QPointF(mapped.x, mapped.y),
                        expectedMappedPoint(params, probe), 1.0e-2);
    }

    // anchorPosition() reports the anchor in canvas space, which is the position
    // once the anchor is the pivot.
    const Diligent::float2 reported = transform.anchorPosition();
    expectPointNear(QPointF(reported.x, reported.y), QPointF(120.0, 85.0), 1.0e-2);
}

TEST(TransformAnchorContractTest, AnimatedSnapshotExposesBothMatricesAndAnchorCanvasPosition)
{
    ArtifactCore::AnimatableTransform3D transform;
    transform.setCurrentPosition(120.0f, 85.0f);
    transform.setCurrentAnchor(24.0f, -13.0f, 0.0f);
    transform.setCurrentRotation(37.0f);
    transform.setCurrentScale(1.7f, 0.65f);

    const LayerParams params{120.0, 85.0, 37.0, 1.7, 0.65, 24.0, -13.0};
    const LayerParams noAnchor{120.0, 85.0, 37.0, 1.7, 0.65, 0.0, 0.0};

    const ArtifactCore::Transform3DSnapshot snapshot = transform.snapshot();
    EXPECT_FLOAT_EQ(snapshot.anchorX, 24.0f);
    EXPECT_FLOAT_EQ(snapshot.anchorY, -13.0f);

    const QPointF probe(24.0, -13.0);
    const Diligent::float3 probe3D(static_cast<float>(probe.x()),
                                   static_cast<float>(probe.y()), 0.0f);
    const Diligent::float3 anchored = mapDiligentMatrix(snapshot.matrixWithAnchor, probe3D);
    const Diligent::float3 unanchored = mapDiligentMatrix(snapshot.matrix, probe3D);

    // matrixWithAnchor pivots on the anchor; matrix is plain T * R * S.
    expectPointNear(QPointF(anchored.x, anchored.y), expectedMappedPoint(params, probe),
                    1.0e-2);
    expectPointNear(QPointF(unanchored.x, unanchored.y),
                    expectedMappedPoint(noAnchor, probe), 1.0e-2);

    // anchorCanvasPosition is the anchor in canvas space, not the translation row.
    expectPointNear(QPointF(snapshot.anchorCanvasPosition.x,
                            snapshot.anchorCanvasPosition.y),
                    QPointF(120.0, 85.0), 1.0e-2);
}

// --- render-level anchor checks with PNG output ---------------------------

TEST(TransformAnchorContractTest, RotatedLayerKeepsTheAnchorMarkerOnThePosition)
{
    const LayerParams params{120.0, 70.0, 30.0, 1.0, 1.0, 110.0, 80.0};
    expectAndSaveAnchorImage(warpMarker(params), QStringLiteral("anchor_rotate_30.png"));
}

TEST(TransformAnchorContractTest, NonUniformScaleKeepsTheAnchorMarkerOnThePosition)
{
    const LayerParams params{118.0, 74.0, 0.0, 1.35, 0.75, 110.0, 80.0};
    expectAndSaveAnchorImage(warpMarker(params),
                             QStringLiteral("anchor_nonuniform_scale.png"));
}

TEST(TransformAnchorContractTest, NegativeScaleMirrorsTheMarkerAroundTheAnchor)
{
    const LayerParams params{120.0, 70.0, 0.0, -1.3, 0.85, 110.0, 80.0};
    expectAndSaveAnchorImage(warpMarker(params),
                             QStringLiteral("anchor_negative_scale.png"));
}

TEST(TransformAnchorContractTest, ParentCompositionKeepsTheChildAnchorMarkerOnThePosition)
{
    // The parent offset keeps the whole rotated marker inside the canvas; a
    // clipped block would move the centroid and mask an anchor error.
    const LayerParams child{118.0, 74.0, -18.0, 1.15, 0.9, 110.0, 80.0};
    const LayerParams parent{20.0, 40.0, -22.0, 0.9, 1.1, 0.0, 0.0};
    expectAndSaveAnchorImage(warpMarkerThroughHierarchy(child, parent),
                             QStringLiteral("anchor_parent_composition.png"));
}

TEST(TransformAnchorContractTest, AnchorNearTheLayerEdgeKeepsTheMarkerOnThePosition)
{
    const LayerParams params{120.0, 70.0, 25.0, 1.1, 1.1, 24.0, 26.0};
    expectAndSaveAnchorImage(warpMarker(params),
                             QStringLiteral("anchor_near_edge.png"));
}

TEST(TransformAnchorContractTest, FractionalAnchorKeepsTheMarkerOnTheRoundedPosition)
{
    const LayerParams params{120.5, 70.5, 30.0, 1.0, 1.0, 110.5, 80.25};
    expectAndSaveAnchorImage(warpMarker(params),
                             QStringLiteral("anchor_fractional.png"));
}
