#include <gtest/gtest.h>

#include <QPointF>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRectF>

#include <array>
#include <limits>
#include <vector>

import Tracking.MotionTracker;

using namespace ArtifactCore;

namespace {

TrackPoint point(int id, double x, double y, double confidence = 1.0,
                 bool active = true)
{
    TrackPoint value(id, QPointF(x, y));
    value.confidence = confidence;
    value.active = active;
    return value;
}

} // namespace

TEST(TrackingDataContractTest, PointLookupSortingAndHomographyProjectionAreConsistent)
{
    TrackFrame frame;
    frame.points = {point(9, 9.0, 1.0), point(2, 2.0, 3.0)};
    ASSERT_NE(frame.findPoint(9), nullptr);
    EXPECT_EQ(frame.findPoint(2)->position, QPointF(2.0, 3.0));
    EXPECT_EQ(frame.findPoint(100), nullptr);
    frame.sortPointsById();
    ASSERT_EQ(frame.points.size(), 2u);
    EXPECT_EQ(frame.points[0].id, 2);
    EXPECT_EQ(frame.points[1].id, 9);

    frame.hasHomography = true;
    frame.homography = {2.0, 0.0, 3.0,
                        0.0, 2.0, -1.0,
                        0.0, 0.0, 1.0};
    QPointF projected;
    ASSERT_TRUE(frame.projectPoint(QPointF(4.0, 5.0), projected));
    EXPECT_EQ(projected, QPointF(11.0, 9.0));

    std::array<QPointF, 4> corners;
    ASSERT_TRUE(frame.projectRect(QRectF(1.0, 2.0, 3.0, 4.0), corners));
    EXPECT_EQ(corners[0], QPointF(5.0, 3.0));
    EXPECT_EQ(corners[2], QPointF(11.0, 11.0));
}

TEST(TrackingDataContractTest, FailedProjectionLeavesCallerOutputUntouched)
{
    TrackFrame frame;
    frame.hasHomography = true;
    frame.homography = {1.0, 0.0, 0.0,
                        0.0, 1.0, 0.0,
                        1.0, 0.0, 0.0};
    QPointF projected(17.0, 19.0);
    EXPECT_FALSE(frame.projectPoint(QPointF(0.0, 1.0), projected));
    EXPECT_EQ(projected, QPointF(17.0, 19.0));

    std::array<QPointF, 4> corners;
    corners.fill(QPointF(23.0, 29.0));
    EXPECT_FALSE(frame.projectRect(QRectF(-1.0, 0.0, 1.0, 1.0), corners));
    for (const QPointF& corner : corners) EXPECT_EQ(corner, QPointF(23.0, 29.0));
}

TEST(TrackingDataContractTest, InterpolationUsesSharedIdsAndClampsAtTimeBounds)
{
    TrackResult result;
    TrackFrame first;
    first.time = 1.0;
    first.points = {point(3, 0.0, 2.0, 0.2), point(8, 5.0, 5.0, 0.8)};
    first.overallConfidence = 0.25;
    TrackFrame last;
    last.time = 3.0;
    last.points = {point(3, 10.0, 6.0, 0.6), point(11, 20.0, 20.0)};
    last.overallConfidence = 0.75;
    result.frames = {first, last};

    const TrackFrame middle = result.interpolateAt(2.0);
    EXPECT_DOUBLE_EQ(middle.time, 2.0);
    EXPECT_DOUBLE_EQ(middle.overallConfidence, 0.5);
    ASSERT_EQ(middle.points.size(), 1u);
    EXPECT_EQ(middle.points.front().id, 3);
    EXPECT_EQ(middle.points.front().position, QPointF(5.0, 4.0));
    EXPECT_DOUBLE_EQ(middle.points.front().confidence, 0.4);
    EXPECT_EQ(result.interpolateAt(0.0).time, 1.0);
    EXPECT_EQ(result.interpolateAt(5.0).time, 3.0);
    EXPECT_EQ(result.interpolateAt(std::numeric_limits<double>::quiet_NaN()).points.size(), 0u);
}

TEST(TrackingDataContractTest,
     CameraPoseStreamNormalizesThenInterpolatesAcrossFailedFramesAndClamps)
{
    CameraPoseStream stream;
    CameraPoseFrame later;
    later.time = 2.0;
    later.pose.rotation = {2.0, 4.0, 6.0};
    later.pose.translation = {20.0, 40.0, 60.0};
    later.meanReprojectionError = 4.0;
    later.maxReprojectionError = 8.0;
    later.valid = true;
    later.diagnostic = QStringLiteral("later valid pose");

    CameraPoseFrame failed;
    failed.time = 1.0;
    failed.valid = false;
    failed.diagnostic = QStringLiteral("solver failed");

    CameraPoseFrame earlier;
    earlier.time = 0.0;
    earlier.pose.rotation = {0.0, 2.0, 4.0};
    earlier.pose.translation = {0.0, 20.0, 40.0};
    earlier.meanReprojectionError = 2.0;
    earlier.maxReprojectionError = 6.0;
    earlier.valid = true;
    earlier.diagnostic = QStringLiteral("earlier valid pose");

    CameraPoseFrame invalidTime = later;
    invalidTime.time = std::numeric_limits<double>::quiet_NaN();
    stream.frames = {later, failed, invalidTime, earlier};
    stream.normalize();

    ASSERT_EQ(stream.frames.size(), 3u);
    EXPECT_DOUBLE_EQ(stream.frames[0].time, 0.0);
    EXPECT_DOUBLE_EQ(stream.frames[1].time, 1.0);
    EXPECT_DOUBLE_EQ(stream.frames[2].time, 2.0);
    EXPECT_EQ(stream.validFrameCount(), 2);
    EXPECT_EQ(stream.failedFrameCount(), 1);
    EXPECT_DOUBLE_EQ(stream.averageReprojectionError(), 3.0);

    CameraPoseFrame interpolated;
    ASSERT_TRUE(stream.interpolatePoseAt(1.0, interpolated));
    EXPECT_DOUBLE_EQ(interpolated.time, 1.0);
    EXPECT_EQ(interpolated.pose.rotation, (std::array<double, 3>{1.0, 3.0, 5.0}));
    EXPECT_EQ(interpolated.pose.translation,
              (std::array<double, 3>{10.0, 30.0, 50.0}));
    EXPECT_DOUBLE_EQ(interpolated.meanReprojectionError, 3.0);
    EXPECT_DOUBLE_EQ(interpolated.maxReprojectionError, 7.0);
    EXPECT_EQ(interpolated.diagnostic,
              QStringLiteral("Interpolated between valid camera poses."));

    CameraPoseFrame clamped;
    ASSERT_TRUE(stream.interpolatePoseAt(-1.0, clamped));
    EXPECT_DOUBLE_EQ(clamped.time, -1.0);
    EXPECT_EQ(clamped.pose.translation, earlier.pose.translation);
    ASSERT_TRUE(stream.interpolatePoseAt(3.0, clamped));
    EXPECT_DOUBLE_EQ(clamped.time, 3.0);
    EXPECT_EQ(clamped.pose.translation, later.pose.translation);

    CameraPoseFrame untouched;
    untouched.time = 17.0;
    untouched.pose.translation = {7.0, 8.0, 9.0};
    EXPECT_FALSE(stream.interpolatePoseAt(std::numeric_limits<double>::infinity(),
                                          untouched));
    EXPECT_DOUBLE_EQ(untouched.time, 17.0);
    EXPECT_EQ(untouched.pose.translation, (std::array<double, 3>{7.0, 8.0, 9.0}));
}

TEST(TrackingDataContractTest,
     CameraPoseStreamJsonRoundTripIsOrderedAndMalformedFrameIsTransactional)
{
    CameraPoseStream source;
    source.schemaVersion = 3;
    source.cancelled = true;
    source.solveSettings.focalLengthX = 1200.0;
    source.solveSettings.focalLengthY = 1180.0;
    source.solveSettings.principalPointX = 640.0;
    source.solveSettings.principalPointY = 360.0;
    source.solveSettings.distortion = {0.1, -0.02, 0.003, -0.004,
                                       0.005, -0.006, 0.007, -0.008};
    source.solveSettings.confidence = 0.875;
    source.solveSettings.iterations = 321;
    source.solveSettings.useExtrinsicGuess = true;

    CameraPoseFrame late;
    late.time = 2.0;
    late.pose.rotation = {0.2, -0.3, 0.4};
    late.pose.translation = {5.0, 6.0, 7.0};
    late.meanReprojectionError = 0.75;
    late.maxReprojectionError = 1.5;
    late.valid = true;
    late.diagnostic = QStringLiteral("late frame");
    CameraPoseFrame early;
    early.time = 0.5;
    early.pose.rotation = {-0.1, 0.25, 0.5};
    early.pose.translation = {1.0, 2.0, 3.0};
    early.meanReprojectionError = 0.25;
    early.maxReprojectionError = 0.5;
    early.valid = true;
    early.diagnostic = QStringLiteral("early frame");
    source.frames = {late, early};

    const QString json = cameraPoseStreamToJson(source);
    EXPECT_DOUBLE_EQ(source.frames.front().time, 2.0)
        << "serialization must normalize a copy rather than reorder caller state";

    CameraPoseStream restored;
    ASSERT_TRUE(cameraPoseStreamFromJson(json, restored));
    EXPECT_EQ(restored.schemaVersion, 3);
    EXPECT_TRUE(restored.cancelled);
    EXPECT_DOUBLE_EQ(restored.solveSettings.focalLengthX, 1200.0);
    EXPECT_DOUBLE_EQ(restored.solveSettings.focalLengthY, 1180.0);
    EXPECT_DOUBLE_EQ(restored.solveSettings.principalPointX, 640.0);
    EXPECT_DOUBLE_EQ(restored.solveSettings.principalPointY, 360.0);
    EXPECT_EQ(restored.solveSettings.distortion, source.solveSettings.distortion);
    EXPECT_DOUBLE_EQ(restored.solveSettings.confidence, 0.875);
    EXPECT_EQ(restored.solveSettings.iterations, 321);
    EXPECT_TRUE(restored.solveSettings.useExtrinsicGuess);
    ASSERT_EQ(restored.frames.size(), 2u);
    EXPECT_DOUBLE_EQ(restored.frames[0].time, 0.5);
    EXPECT_EQ(restored.frames[0].pose.rotation, early.pose.rotation);
    EXPECT_EQ(restored.frames[0].pose.translation, early.pose.translation);
    EXPECT_EQ(restored.frames[0].diagnostic, early.diagnostic);
    EXPECT_DOUBLE_EQ(restored.frames[1].time, 2.0);
    EXPECT_EQ(restored.frames[1].pose.rotation, late.pose.rotation);
    EXPECT_EQ(restored.frames[1].pose.translation, late.pose.translation);

    QJsonObject malformed = QJsonDocument::fromJson(json.toUtf8()).object();
    QJsonArray frames = malformed.value(QStringLiteral("frames")).toArray();
    QJsonObject malformedFrame = frames[1].toObject();
    malformedFrame[QStringLiteral("rotation")] = QJsonArray{1.0, 2.0};
    frames[1] = malformedFrame;
    malformed[QStringLiteral("frames")] = frames;

    EXPECT_FALSE(cameraPoseStreamFromJson(
        QString::fromUtf8(QJsonDocument(malformed).toJson(QJsonDocument::Compact)),
        restored));
    EXPECT_EQ(restored.schemaVersion, 3);
    EXPECT_TRUE(restored.cancelled);
    ASSERT_EQ(restored.frames.size(), 2u);
    EXPECT_DOUBLE_EQ(restored.frames[0].time, 0.5);
    EXPECT_EQ(restored.frames[0].pose.translation, early.pose.translation);
    EXPECT_DOUBLE_EQ(restored.frames[1].time, 2.0);
    EXPECT_EQ(restored.frames[1].pose.translation, late.pose.translation);
}

TEST(TrackingDataContractTest, NormalizeOrdersAndSanitizesFramesAndFailureTimes)
{
    TrackResult result;
    TrackFrame later;
    later.time = 2.0;
    later.overallConfidence = std::numeric_limits<double>::infinity();
    later.hasHomography = true;
    later.homography[0] = std::numeric_limits<double>::quiet_NaN();
    later.points = {
        point(9, 9.0, 9.0, 2.0),
        point(2, std::numeric_limits<double>::infinity(), 1.0),
    };
    TrackFrame earlier;
    earlier.time = 1.0;
    earlier.points = {point(4, 4.0, 4.0, -1.0)};
    TrackFrame invalidTime;
    invalidTime.time = std::numeric_limits<double>::quiet_NaN();
    result.frames = {later, invalidTime, earlier};
    result.failureFrames = {2.0, std::numeric_limits<double>::infinity(),
                            1.0, 2.0 + 1.0e-10};

    result.normalize();

    ASSERT_EQ(result.frames.size(), 2u);
    EXPECT_DOUBLE_EQ(result.startTime, 1.0);
    EXPECT_DOUBLE_EQ(result.endTime, 2.0);
    EXPECT_TRUE(result.isValid);
    ASSERT_EQ(result.frames[0].points.size(), 1u);
    EXPECT_DOUBLE_EQ(result.frames[0].points[0].confidence, 0.0);
    ASSERT_EQ(result.frames[1].points.size(), 2u);
    EXPECT_EQ(result.frames[1].points[0].id, 2);
    EXPECT_FALSE(result.frames[1].points[0].active);
    EXPECT_DOUBLE_EQ(result.frames[1].points[0].confidence, 0.0);
    EXPECT_DOUBLE_EQ(result.frames[1].points[1].confidence, 1.0);
    EXPECT_DOUBLE_EQ(result.frames[1].overallConfidence, 0.0);
    EXPECT_FALSE(result.frames[1].hasHomography);
    EXPECT_EQ(result.failureFrames, (std::vector<double>{1.0, 2.0}));
}

TEST(TrackingDataContractTest,
     NormalizeMergesNearDuplicateFramesBeforePointAndHomographyInterpolation)
{
    TrackResult result;
    TrackFrame last;
    last.time = 3.0;
    last.overallConfidence = 0.8;
    last.hasHomography = true;
    last.homography = {1.0, 0.0, 30.0,
                       0.0, 1.0, 0.0,
                       0.0, 0.0, 1.0};
    last.points = {point(4, 30.0, 0.0, 0.8)};

    TrackFrame first;
    first.time = 1.0;
    first.overallConfidence = 0.2;
    first.hasHomography = true;
    first.homography = {1.0, 0.0, 0.0,
                        0.0, 1.0, 0.0,
                        0.0, 0.0, 1.0};
    first.points = {point(4, 0.0, 0.0, 0.2)};

    TrackFrame nearDuplicate;
    nearDuplicate.time = 1.0 + 5.0e-10;
    nearDuplicate.overallConfidence = 0.6;
    nearDuplicate.hasHomography = true;
    nearDuplicate.homography = {1.0, 0.0, 10.0,
                                0.0, 1.0, 0.0,
                                0.0, 0.0, 1.0};
    nearDuplicate.points = {point(4, 10.0, 0.0, 0.6)};
    result.frames = {last, nearDuplicate, first};

    result.normalize();

    ASSERT_EQ(result.frames.size(), 2u);
    EXPECT_DOUBLE_EQ(result.startTime, nearDuplicate.time);
    EXPECT_DOUBLE_EQ(result.endTime, 3.0);
    ASSERT_EQ(result.frames.front().points.size(), 1u);
    EXPECT_EQ(result.frames.front().points.front().position, QPointF(10.0, 0.0));

    const double sampleTime = 2.00000000025;
    const TrackFrame interpolated = result.interpolateAt(sampleTime);
    EXPECT_DOUBLE_EQ(interpolated.time, sampleTime);
    EXPECT_DOUBLE_EQ(interpolated.overallConfidence, 0.7);
    ASSERT_EQ(interpolated.points.size(), 1u);
    EXPECT_EQ(interpolated.points.front().id, 4);
    EXPECT_NEAR(interpolated.points.front().position.x(), 20.0, 1.0e-8);
    EXPECT_DOUBLE_EQ(interpolated.points.front().confidence, 0.7);
    ASSERT_TRUE(interpolated.hasHomography);
    EXPECT_NEAR(interpolated.homography[2], 20.0, 1.0e-8);

    QPointF projected;
    ASSERT_TRUE(interpolated.projectPoint(QPointF(1.0, 2.0), projected));
    EXPECT_NEAR(projected.x(), 21.0, 1.0e-8);
    EXPECT_NEAR(projected.y(), 2.0, 1.0e-8);
}

TEST(TrackingDataContractTest,
     SingularInterpolatedHomographyFallsBackWithoutLosingPointInterpolation)
{
    TrackFrame first;
    first.time = 0.0;
    first.hasHomography = true;
    first.homography = {1.0, 0.0, 0.0,
                        0.0, 1.0, 0.0,
                        0.0, 0.0, 1.0};
    first.points = {point(3, 0.0, 4.0, 0.2)};

    TrackFrame last;
    last.time = 1.0;
    last.hasHomography = true;
    // A homogeneous scale of -1 represents the same projective identity, but
    // linear coefficient interpolation crosses the all-zero matrix at t=0.5.
    last.homography = {-1.0, 0.0, 0.0,
                       0.0, -1.0, 0.0,
                       0.0, 0.0, -1.0};
    last.points = {point(3, 10.0, 8.0, 0.8)};

    TrackResult result;
    result.frames = {first, last};
    const TrackFrame middle = result.interpolateAt(0.5);

    EXPECT_FALSE(middle.hasHomography);
    EXPECT_EQ(middle.homography,
              (std::array<double, 9>{1.0, 0.0, 0.0,
                                     0.0, 1.0, 0.0,
                                     0.0, 0.0, 1.0}));
    ASSERT_EQ(middle.points.size(), 1u);
    EXPECT_EQ(middle.points.front().id, 3);
    EXPECT_EQ(middle.points.front().position, QPointF(5.0, 6.0));
    EXPECT_DOUBLE_EQ(middle.points.front().confidence, 0.5);

    QPointF projected(17.0, 19.0);
    EXPECT_FALSE(middle.projectPoint(QPointF(1.0, 2.0), projected));
    EXPECT_EQ(projected, QPointF(17.0, 19.0));
}

TEST(TrackingDataContractTest, SetFrameReplacesNearTimestampAndMotionPathFillsOutput)
{
    TrackResult result;
    TrackFrame first;
    first.time = 1.0;
    first.points = {point(5, 1.0, 2.0)};
    result.setFrame(first);
    TrackFrame replacement;
    replacement.time = 1.0 + 1.0e-10;
    replacement.points = {point(5, 7.0, 8.0)};
    result.setFrame(replacement);

    EXPECT_EQ(result.frameCount(), 1u);
    EXPECT_EQ(result.frames.front().points.front().position, QPointF(7.0, 8.0));
    TrackFrame next;
    next.time = 2.0;
    next.points = {point(5, 9.0, 10.0)};
    result.setFrame(next);

    std::vector<QPointF> path{QPointF(-1.0, -1.0)};
    EXPECT_TRUE(result.motionPath(5, path));
    EXPECT_EQ(path, (std::vector<QPointF>{QPointF(7.0, 8.0), QPointF(9.0, 10.0)}));
    EXPECT_FALSE(result.motionPath(99, path));
    EXPECT_TRUE(path.empty());
}

TEST(TrackingDataContractTest, JsonRoundTripPreservesSettingsRegionsAndTrackedFrameData)
{
    MotionTracker source(41);
    source.setName(QStringLiteral("Camera solve"));
    source.setTrackerType(TrackerType::Perspective);
    TrackerSettings settings;
    settings.method = TrackingMethod::Hybrid;
    settings.quality = TrackingQuality::High;
    settings.maxFeatures = 237;
    settings.minDistance = 4.25;
    settings.trackBackward = true;
    settings.searchRegion = QRectF(12.0, 18.0, 320.0, 180.0);
    source.setSettings(settings);
    source.addTrackPoint(QPointF(20.0, 30.0));
    source.addTrackRegion(QRectF(10.0, 15.0, 40.0, 25.0));

    TrackResult expected;
    expected.schemaVersion = 3;
    expected.trackerId = source.id();
    expected.name = source.name();
    expected.sourceName = QStringLiteral("Plate A");
    expected.sourcePath = QStringLiteral("shots/plate_a.exr");
    expected.sourceType = QStringLiteral("image-sequence");
    expected.startTime = 1.0;
    expected.endTime = 2.0;
    expected.isValid = true;

    TrackFrame first;
    first.time = 1.0;
    first.overallConfidence = 0.875;
    first.hasHomography = true;
    first.homography = {1.0, 0.1, 3.0,
                        -0.05, 1.0, 4.0,
                        0.001, -0.002, 1.0};
    TrackPoint firstPoint = point(1, 23.0, 34.0, 0.9);
    firstPoint.velocity = QPointF(3.0, 4.0);
    first.points = {firstPoint};
    TrackFrame last;
    last.time = 2.0;
    last.overallConfidence = 0.625;
    TrackPoint lastPoint = point(1, 26.0, 39.0, 0.7, false);
    lastPoint.velocity = QPointF(3.0, 5.0);
    last.points = {lastPoint};
    expected.frames = {first, last};
    expected.failureFrames = {1.5};
    expected.normalize();
    QJsonObject payload = QJsonDocument::fromJson(source.toJson().toUtf8()).object();
    payload[QStringLiteral("schemaVersion")] = expected.schemaVersion;
    payload[QStringLiteral("sourceName")] = expected.sourceName;
    payload[QStringLiteral("sourcePath")] = expected.sourcePath;
    payload[QStringLiteral("sourceType")] = expected.sourceType;
    payload[QStringLiteral("startTime")] = expected.startTime;
    payload[QStringLiteral("endTime")] = expected.endTime;
    payload[QStringLiteral("resultValid")] = expected.isValid;
    QJsonArray frames;
    for (const TrackFrame& frame : expected.frames) {
        QJsonArray points;
        for (const TrackPoint& trackedPoint : frame.points) {
            points.append(QJsonObject{
                {QStringLiteral("id"), trackedPoint.id},
                {QStringLiteral("x"), trackedPoint.position.x()},
                {QStringLiteral("y"), trackedPoint.position.y()},
                {QStringLiteral("vx"), trackedPoint.velocity.x()},
                {QStringLiteral("vy"), trackedPoint.velocity.y()},
                {QStringLiteral("confidence"), trackedPoint.confidence},
                {QStringLiteral("active"), trackedPoint.active},
            });
        }
        QJsonArray homography;
        for (double value : frame.homography) homography.append(value);
        frames.append(QJsonObject{
            {QStringLiteral("time"), frame.time},
            {QStringLiteral("confidence"), frame.overallConfidence},
            {QStringLiteral("hasHomography"), frame.hasHomography},
            {QStringLiteral("homography"), homography},
            {QStringLiteral("points"), points},
        });
    }
    payload[QStringLiteral("frames")] = frames;
    payload[QStringLiteral("failureFrames")] = QJsonArray{1.5};

    MotionTracker restored;
    ASSERT_TRUE(restored.fromJson(QString::fromUtf8(
        QJsonDocument(payload).toJson(QJsonDocument::Compact))));
    MotionTracker roundTripped;
    ASSERT_TRUE(roundTripped.fromJson(restored.toJson()));

    const TrackResult actual = restored.result();
    const TrackResult roundTrippedResult = roundTripped.result();
    EXPECT_EQ(restored.id(), source.id());
    EXPECT_EQ(restored.name(), source.name());
    EXPECT_EQ(restored.trackerType(), TrackerType::Perspective);
    EXPECT_EQ(restored.settings().method, TrackingMethod::Hybrid);
    EXPECT_EQ(restored.settings().quality, TrackingQuality::High);
    EXPECT_EQ(restored.settings().maxFeatures, 237);
    EXPECT_DOUBLE_EQ(restored.settings().minDistance, 4.25);
    EXPECT_TRUE(restored.settings().trackBackward);
    EXPECT_EQ(restored.settings().searchRegion, QRectF(12.0, 18.0, 320.0, 180.0));
    EXPECT_EQ(restored.trackPointCount(), 1);
    EXPECT_EQ(restored.trackPoints().front().position, QPointF(20.0, 30.0));
    EXPECT_EQ(restored.trackRegionCount(), 1);
    EXPECT_EQ(restored.trackPoints().front().id, source.trackPoints().front().id);
    ASSERT_EQ(actual.frameCount(), 2u);
    EXPECT_EQ(actual.sourceName, QStringLiteral("Plate A"));
    EXPECT_EQ(actual.sourcePath, QStringLiteral("shots/plate_a.exr"));
    EXPECT_EQ(actual.sourceType, QStringLiteral("image-sequence"));
    EXPECT_EQ(actual.failureFrames, (std::vector<double>{1.5}));
    ASSERT_TRUE(actual.frames[0].hasHomography);
    EXPECT_EQ(actual.frames[0].homography, first.homography);
    ASSERT_EQ(actual.frames[0].points.size(), 1u);
    EXPECT_EQ(actual.frames[0].points[0].position, QPointF(23.0, 34.0));
    EXPECT_EQ(actual.frames[0].points[0].velocity, QPointF(3.0, 4.0));
    EXPECT_DOUBLE_EQ(actual.frames[0].points[0].confidence, 0.9);
    EXPECT_FALSE(actual.frames[1].points[0].active);
    EXPECT_EQ(restored.pointPositionAt(1, 1.5), QPointF(24.5, 36.5));
    ASSERT_EQ(roundTrippedResult.frameCount(), actual.frameCount());
    EXPECT_EQ(roundTrippedResult.sourcePath, actual.sourcePath);
    EXPECT_EQ(roundTrippedResult.failureFrames, actual.failureFrames);
    ASSERT_EQ(roundTrippedResult.frames[0].points.size(), 1u);
    EXPECT_EQ(roundTrippedResult.frames[0].homography, actual.frames[0].homography);
    EXPECT_EQ(roundTrippedResult.frames[0].points[0].position,
              actual.frames[0].points[0].position);
}

TEST(TrackingDataContractTest, MalformedJsonDoesNotReplaceExistingTrackerState)
{
    MotionTracker tracker(52);
    tracker.setName(QStringLiteral("Preserved tracker"));
    tracker.setTrackerType(TrackerType::Planar);
    tracker.addTrackPoint(QPointF(7.0, 11.0));
    QJsonObject payload = QJsonDocument::fromJson(tracker.toJson().toUtf8()).object();
    payload[QStringLiteral("frames")] = QJsonArray{QJsonObject{
        {QStringLiteral("time"), 3.0},
        {QStringLiteral("confidence"), 1.0},
        {QStringLiteral("hasHomography"), false},
        {QStringLiteral("points"), QJsonArray{QJsonObject{
            {QStringLiteral("id"), 1},
            {QStringLiteral("x"), 17.0},
            {QStringLiteral("y"), 19.0},
            {QStringLiteral("vx"), 0.0},
            {QStringLiteral("vy"), 0.0},
            {QStringLiteral("confidence"), 1.0},
            {QStringLiteral("active"), true},
        }}},
    }}};
    ASSERT_TRUE(tracker.fromJson(QString::fromUtf8(
        QJsonDocument(payload).toJson(QJsonDocument::Compact))));

    EXPECT_FALSE(tracker.fromJson(QStringLiteral("{")));
    EXPECT_EQ(tracker.id(), 52);
    EXPECT_EQ(tracker.name(), QStringLiteral("Preserved tracker"));
    EXPECT_EQ(tracker.trackerType(), TrackerType::Planar);
    EXPECT_EQ(tracker.trackPointCount(), 1);
    EXPECT_EQ(tracker.trackPoints().front().position, QPointF(7.0, 11.0));
    ASSERT_EQ(tracker.result().frameCount(), 1u);
    EXPECT_DOUBLE_EQ(tracker.result().frames.front().time, 3.0);
    EXPECT_EQ(tracker.pointPositionAt(1, 3.0), QPointF(17.0, 19.0));
    EXPECT_EQ(tracker.resultRef().sourceName, QStringLiteral("Preserved tracker"));
}

TEST(TrackingDataContractTest, TrackerManagerRoundTripSortsIdsAndSkipsInvalidOrDuplicateEntries)
{
    TrackerManager& manager = TrackerManager::instance();
    manager.clearTrackers();

    QJsonArray input{
        QJsonObject{
            {QStringLiteral("id"), 902},
            {QStringLiteral("name"), QStringLiteral("Second")},
            {QStringLiteral("trackerType"), static_cast<int>(TrackerType::Planar)},
        },
        QJsonValue(QStringLiteral("not an object")),
        QJsonObject{
            {QStringLiteral("id"), 0},
            {QStringLiteral("name"), QStringLiteral("Invalid id")},
        },
        QJsonObject{
            {QStringLiteral("id"), 901},
            {QStringLiteral("name"), QStringLiteral("First")},
        },
        QJsonObject{
            {QStringLiteral("id"), 902},
            {QStringLiteral("name"), QStringLiteral("Duplicate id")},
        },
    };

    manager.fromJson(input);
    ASSERT_EQ(manager.trackerCount(), 2);
    ASSERT_NE(manager.tracker(901), nullptr);
    ASSERT_NE(manager.tracker(902), nullptr);
    EXPECT_EQ(manager.tracker(901)->name(), QStringLiteral("First"));
    EXPECT_EQ(manager.tracker(902)->name(), QStringLiteral("Second"));
    EXPECT_EQ(manager.tracker(902)->trackerType(), TrackerType::Planar);

    const QJsonArray saved = manager.toJson();
    ASSERT_EQ(saved.size(), 2);
    EXPECT_EQ(saved[0].toObject()[QStringLiteral("id")].toInt(), 901);
    EXPECT_EQ(saved[1].toObject()[QStringLiteral("id")].toInt(), 902);

    manager.fromJson(saved);
    ASSERT_EQ(manager.trackerCount(), 2);
    EXPECT_EQ(manager.tracker(901)->name(), QStringLiteral("First"));
    EXPECT_EQ(manager.tracker(902)->name(), QStringLiteral("Second"));
    manager.clearTrackers();
}
