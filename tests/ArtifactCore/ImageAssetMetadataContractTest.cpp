#include <gtest/gtest.h>

#include <QByteArray>
#include <QBuffer>
#include <QColor>
#include <QColorSpace>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QIODevice>
#include <QImage>
#include <QImageReader>
#include <QPoint>
#include <QTemporaryDir>
#include <QString>
#include <QStringList>
#include <QSize>

#include <cstdint>
#include <cstring>
#include <limits>
#include <array>
#include <chrono>
#include <thread>

import ImageAsset;
import Image.Raw;
import IO.ImageImporter;
import Media.ImageSequenceSource;
import Media.ISource;
import Utils.String.UniString;

using namespace ArtifactCore;

namespace {

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(bytes) == bytes.size();
}

QByteArray checkerPng()
{
    return QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAAIAAAACACAYAAADDPmHLAAABqElEQVR4nO3asQnAMBAEQZXz/VehruwihBFmJ/h4JNjw1sw8J7f3Pjr+XX/dfgBfAHwB8AXAFwBfAHwB8AXAFwBfAHwB8AXAFwBfAHwB8D8J4O8f4J/5Aoj7Aoj7Aoj7Aoj7Aoj7Aoj7BiFxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBx3yAk7gsg7gsg7gsg7gsg7gsg7gsg7gsg7gsg7gsg7gsg7gsg7gsg7gsg7huExH0BxH0BxH0BxH0BxH0BxH0BxH0BxH0BxH0BxH0BxH0BxH0BxH0BxH2DkLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLhvEBL3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3DULivgDivgDivgDivgDivgDivgDivgDivgDivgDivgDivgDivgDivgDivgDi/gsB3sj4A9iR2AAAAABJRU5ErkJggg=="));
}

QByteArray onePixelPng()
{
    return QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg=="));
}

QByteArray blueOnePixelPng()
{
    return QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGNgYPj/HwADAgH/5ncLrgAAAABJRU5ErkJggg=="));
}

QByteArray jpegWithExifOrientation(const QImage& image, int orientation)
{
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "JPEG", 100)) {
        return {};
    }

    QByteArray exif("Exif\0\0", 6);
    exif.append("II", 2);
    exif.append(static_cast<char>(0x2a));
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(8));
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(1)); // one IFD entry
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(0x12)); // Exif orientation tag 0x0112
    exif.append(static_cast<char>(0x01));
    exif.append(static_cast<char>(3)); // SHORT
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(1));
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(orientation));
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(0)); // next IFD offset
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(0));
    exif.append(static_cast<char>(0));

    const int app1Length = exif.size() + 2;
    QByteArray app1;
    app1.append(static_cast<char>(0xff));
    app1.append(static_cast<char>(0xe1));
    app1.append(static_cast<char>((app1Length >> 8) & 0xff));
    app1.append(static_cast<char>(app1Length & 0xff));
    app1.append(exif);
    return jpeg.left(2) + app1 + jpeg.mid(2);
}

QColor decodedPixel(const RawImage& image, int x, int y)
{
    const qsizetype offset =
        (static_cast<qsizetype>(y) * image.width + x) * image.channels;
    return QColor(static_cast<unsigned char>(image.data[offset]),
                  static_cast<unsigned char>(image.data[offset + 1]),
                  static_cast<unsigned char>(image.data[offset + 2]),
                  static_cast<unsigned char>(image.data[offset + 3]));
}

} // namespace

TEST(ImageAssetMetadataContractTest, RefreshesDimensionsAndBasicImageMetadata)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("checker.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageAssetFile image(path);
    ASSERT_FALSE(image.isLoaded());
    ASSERT_TRUE(image.load());
    EXPECT_TRUE(image.isLoaded());
    EXPECT_TRUE(image.isImageMetadataValid());
    EXPECT_EQ(image.imageMeta().width, 128);
    EXPECT_EQ(image.imageMeta().height, 128);
    EXPECT_EQ(image.imageMeta().channelCount, 4);
    EXPECT_GT(image.imageMeta().bitDepth, 0);
    EXPECT_FALSE(image.imageMeta().pixelType.isEmpty());
    EXPECT_EQ(image.meta().getValue(UniString("width")).toQString(), QStringLiteral("128"));
    EXPECT_EQ(image.meta().getValue(UniString("height")).toQString(), QStringLiteral("128"));
    EXPECT_EQ(image.meta().getValue(UniString("channels")).toQString(), QStringLiteral("4"));
    EXPECT_EQ(image.meta().getValue(UniString("bitDepth")).toQString(),
              QString::number(image.imageMeta().bitDepth));

    image.unload();
    EXPECT_FALSE(image.isLoaded());
}

TEST(ImageAssetMetadataContractTest, MetadataRefreshDoesNotChangeLoadState)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("refresh-state.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageAssetFile image(path);
    ASSERT_FALSE(image.isLoaded());
    ASSERT_TRUE(image.refreshMetadata());
    EXPECT_FALSE(image.isLoaded());

    ASSERT_TRUE(image.load());
    ASSERT_TRUE(image.isLoaded());
    ASSERT_TRUE(image.refreshMetadata());
    EXPECT_TRUE(image.isLoaded());
}

TEST(ImageAssetMetadataContractTest, RejectsMissingAndUndecodableImages)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString invalidPath = directory.filePath(QStringLiteral("broken.png"));
    ASSERT_TRUE(writeFile(invalidPath, QByteArrayLiteral("not an image")));

    ImageAssetFile missing(directory.filePath(QStringLiteral("missing.png")));
    EXPECT_FALSE(missing.refreshMetadata());
    EXPECT_FALSE(missing.isImageMetadataValid());

    ImageAssetFile invalid(invalidPath);
    EXPECT_FALSE(invalid.load());
    EXPECT_FALSE(invalid.isLoaded());
    EXPECT_FALSE(invalid.isImageMetadataValid());
}

TEST(ImageAssetMetadataContractTest, FailedRefreshPreservesPreviouslyDecodedMetadata)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("refreshable.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageAssetFile image(path);
    ASSERT_TRUE(image.refreshMetadata());
    const auto previous = image.imageMeta();
    ASSERT_EQ(previous.width, 128);
    ASSERT_EQ(previous.height, 128);
    const QString previousWidth = image.meta().getValue(UniString("width")).toQString();
    const QString previousHeight = image.meta().getValue(UniString("height")).toQString();
    ASSERT_TRUE(writeFile(path, QByteArrayLiteral("not an image anymore")));

    EXPECT_FALSE(image.refreshMetadata());
    EXPECT_EQ(image.imageMeta().width, previous.width);
    EXPECT_EQ(image.imageMeta().height, previous.height);
    EXPECT_EQ(image.imageMeta().channelCount, previous.channelCount);
    EXPECT_EQ(image.imageMeta().bitDepth, previous.bitDepth);
    EXPECT_EQ(image.imageMeta().pixelType, previous.pixelType);
    EXPECT_EQ(image.imageMeta().colorSpace, previous.colorSpace);
    EXPECT_EQ(image.imageMeta().hdr, previous.hdr);
    EXPECT_EQ(image.meta().getValue(UniString("width")).toQString(), previousWidth);
    EXPECT_EQ(image.meta().getValue(UniString("height")).toQString(), previousHeight);
}

TEST(ImageAssetMetadataContractTest,
     SuccessfulRefreshReplacesMetadataAfterSourceDimensionsChange)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("resized-source.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageAssetFile image(path);
    ASSERT_TRUE(image.refreshMetadata());
    ASSERT_EQ(image.imageMeta().width, 128);
    ASSERT_EQ(image.imageMeta().height, 128);
    ASSERT_FALSE(image.isLoaded());

    ASSERT_TRUE(writeFile(path, onePixelPng()));
    ASSERT_TRUE(image.refreshMetadata());

    EXPECT_TRUE(image.isImageMetadataValid());
    EXPECT_FALSE(image.isLoaded());
    EXPECT_EQ(image.imageMeta().width, 1);
    EXPECT_EQ(image.imageMeta().height, 1);
    EXPECT_EQ(image.meta().getValue(UniString("width")).toQString(),
              QStringLiteral("1"));
    EXPECT_EQ(image.meta().getValue(UniString("height")).toQString(),
              QStringLiteral("1"));
}

TEST(ImageImporterContractTest, ReadsImageAndResetsAfterClose)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("checker.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageImporter importer;
    EXPECT_FALSE(importer.readImage().isValid());
    ASSERT_TRUE(importer.open(path));
    const RawImage decoded = importer.readImage();
    ASSERT_TRUE(decoded.isValid());
    EXPECT_EQ(decoded.width, 128);
    EXPECT_EQ(decoded.height, 128);
    EXPECT_EQ(decoded.channels, 4);
    const int bytesPerChannel = decoded.getPixelTypeSizeInBytes();
    ASSERT_GT(bytesPerChannel, 0);
    EXPECT_EQ(decoded.data.size(),
              decoded.width * decoded.height * decoded.channels * bytesPerChannel);

    importer.close();
    EXPECT_FALSE(importer.readImage().isValid());

    ASSERT_TRUE(importer.open(path));
    ASSERT_TRUE(importer.readImage().isValid());
    EXPECT_FALSE(importer.open(directory.filePath(QStringLiteral("missing.png"))));
    EXPECT_FALSE(importer.readImage().isValid());
}

TEST(ImageImporterContractTest, OpenRejectsMissingFile)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    ImageImporter importer;
    EXPECT_FALSE(importer.open(directory.filePath(QStringLiteral("missing.png"))));
    EXPECT_FALSE(importer.readImage().isValid());
}

TEST(ImageImporterContractTest,
     PreservesEmbeddedIccProfileInDecodedRawImage)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("profiled.png"));

    QImage sourceImage(2, 1, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(80, 120, 160, 255));
    sourceImage.setColorSpace(QColorSpace(QColorSpace::SRgb));
    ASSERT_FALSE(sourceImage.colorSpace().iccProfile().isEmpty());
    ASSERT_TRUE(sourceImage.save(path, "PNG"));

    ImageImporter importer;
    ASSERT_TRUE(importer.open(path));
    const RawImage decoded = importer.readImage();

    ASSERT_TRUE(decoded.isValid());
    EXPECT_FALSE(decoded.iccProfileData.isEmpty());
    EXPECT_TRUE(decoded.metadata.value(QStringLiteral("ICCProfile"))
                    .startsWith(QStringLiteral("embedded (")));
}

TEST(ImageImporterContractTest, PreservesUnassociatedRgbForTranslucentPngPixels)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("translucent.png"));
    QImage sourceImage(1, 1, QImage::Format_RGBA8888);
    sourceImage.setPixelColor(0, 0, QColor(220, 100, 20, 50));
    ASSERT_TRUE(sourceImage.save(path, "PNG"));

    ImageImporter importer;
    ASSERT_TRUE(importer.open(path));
    const RawImage decoded = importer.readImage();

    ASSERT_TRUE(decoded.isValid());
    ASSERT_EQ(decoded.pixelType, QStringLiteral("uint16"));
    ASSERT_EQ(decoded.data.size(), 4 * static_cast<int>(sizeof(std::uint16_t)));
    std::uint16_t rgba[4]{};
    std::memcpy(rgba, decoded.data.constData(), sizeof(rgba));

    // The source red channel exceeds alpha. Premultiplication would make it
    // smaller than alpha, so this relation checks raw straight-alpha storage.
    EXPECT_GT(rgba[0], rgba[3]);
    EXPECT_GT(rgba[1], rgba[2]);
    EXPECT_GT(rgba[2], 0);
}

TEST(ImageImporterContractTest, PreservesTrueSixteenBitPngChannelValues)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("sixteen-bit.png"));
    QImage sourceImage(1, 1, QImage::Format_RGBA64);
    sourceImage.setPixelColor(
        0, 0, QColor::fromRgba64(12345, 23456, 34567, 45678));
    ASSERT_TRUE(sourceImage.save(path, "PNG"));

    ImageImporter importer;
    ASSERT_TRUE(importer.open(path));
    const RawImage decoded = importer.readImage();

    ASSERT_TRUE(decoded.isValid());
    ASSERT_EQ(decoded.pixelType, QStringLiteral("uint16"));
    ASSERT_EQ(decoded.data.size(), 4 * static_cast<int>(sizeof(std::uint16_t)));
    std::uint16_t rgba[4]{};
    std::memcpy(rgba, decoded.data.constData(), sizeof(rgba));
    EXPECT_EQ(rgba[0], 12345);
    EXPECT_EQ(rgba[1], 23456);
    EXPECT_EQ(rgba[2], 34567);
    EXPECT_EQ(rgba[3], 45678);
}

TEST(ImageImporterContractTest, AppliesAllExifOrientationTransformsToPixelsAndDimensions)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    constexpr int sourceWidth = 64;
    constexpr int sourceHeight = 32;
    QImage source(sourceWidth, sourceHeight, QImage::Format_RGB32);
    const std::array<QColor, 4> sourceColors{
        QColor(240, 20, 20), QColor(20, 240, 20),
        QColor(20, 20, 240), QColor(240, 240, 20)};
    for (int y = 0; y < sourceHeight; ++y) {
        for (int x = 0; x < sourceWidth; ++x) {
            const int quadrant = (y >= sourceHeight / 2 ? 2 : 0) +
                                 (x >= sourceWidth / 2 ? 1 : 0);
            source.setPixelColor(x, y, sourceColors[static_cast<std::size_t>(quadrant)]);
        }
    }

    const std::array<QPoint, 4> sourceCenters{
        QPoint(16, 8), QPoint(48, 8), QPoint(16, 24), QPoint(48, 24)};
    for (int orientation = 1; orientation <= 8; ++orientation) {
        const QString path = directory.filePath(
            QStringLiteral("orientation-%1.jpg").arg(orientation));
        ASSERT_TRUE(writeFile(path, jpegWithExifOrientation(source, orientation)))
            << "orientation=" << orientation;

        ImageImporter importer;
        ASSERT_TRUE(importer.open(path));
        const RawImage decoded = importer.readImage();
        ASSERT_TRUE(decoded.isValid()) << "orientation=" << orientation;
        const bool swapsDimensions = orientation >= 5;
        const int expectedWidth = swapsDimensions ? sourceHeight : sourceWidth;
        const int expectedHeight = swapsDimensions ? sourceWidth : sourceHeight;
        ASSERT_EQ(decoded.width, expectedWidth) << "orientation=" << orientation;
        ASSERT_EQ(decoded.height, expectedHeight) << "orientation=" << orientation;
        ASSERT_EQ(decoded.channels, 4);
        ASSERT_EQ(decoded.pixelType, QStringLiteral("uint8"));

        if (orientation == 6) {
            ImageAssetFile asset(path);
            ASSERT_TRUE(asset.refreshMetadata());
            EXPECT_EQ(asset.imageMeta().width, expectedWidth);
            EXPECT_EQ(asset.imageMeta().height, expectedHeight);
        }

        for (std::size_t quadrant = 0; quadrant < sourceCenters.size(); ++quadrant) {
            const int x = sourceCenters[quadrant].x();
            const int y = sourceCenters[quadrant].y();
            int destinationX = x;
            int destinationY = y;
            switch (orientation) {
            case 2: destinationX = sourceWidth - 1 - x; break;
            case 3:
                destinationX = sourceWidth - 1 - x;
                destinationY = sourceHeight - 1 - y;
                break;
            case 4: destinationY = sourceHeight - 1 - y; break;
            case 5:
                destinationX = y;
                destinationY = x;
                break;
            case 6:
                destinationX = sourceHeight - 1 - y;
                destinationY = x;
                break;
            case 7:
                destinationX = sourceHeight - 1 - y;
                destinationY = sourceWidth - 1 - x;
                break;
            case 8:
                destinationX = y;
                destinationY = sourceWidth - 1 - x;
                break;
            default: break;
            }

            const QColor actual = decodedPixel(decoded, destinationX, destinationY);
            const QColor expected = sourceColors[quadrant];
            EXPECT_NEAR(actual.red(), expected.red(), 45)
                << "orientation=" << orientation << " quadrant=" << quadrant;
            EXPECT_NEAR(actual.green(), expected.green(), 45)
                << "orientation=" << orientation << " quadrant=" << quadrant;
            EXPECT_NEAR(actual.blue(), expected.blue(), 45)
                << "orientation=" << orientation << " quadrant=" << quadrant;
            EXPECT_EQ(actual.alpha(), 255)
                << "orientation=" << orientation << " quadrant=" << quadrant;
        }
    }
}

TEST(ImageImporterContractTest, CanOpenValidSourceAgainAfterMissingPathFailure)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString validPath = directory.filePath(QStringLiteral("valid.png"));
    const QString missingPath = directory.filePath(QStringLiteral("missing.png"));
    ASSERT_TRUE(writeFile(validPath, checkerPng()));

    ImageImporter importer;
    ASSERT_TRUE(importer.open(validPath));
    ASSERT_TRUE(importer.readImage().isValid());

    EXPECT_FALSE(importer.open(missingPath));
    EXPECT_FALSE(importer.readImage().isValid());

    ASSERT_TRUE(importer.open(validPath));
    const RawImage recovered = importer.readImage();
    ASSERT_TRUE(recovered.isValid());
    EXPECT_EQ(recovered.width, 128);
    EXPECT_EQ(recovered.height, 128);
}

TEST(ImageImporterContractTest, DefersDecodeFailureUntilRead)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("broken.png"));
    ASSERT_TRUE(writeFile(path, QByteArrayLiteral("not an image")));

    ImageImporter importer;
    EXPECT_TRUE(importer.open(path));
    EXPECT_FALSE(importer.readImage().isValid());
}

TEST(ImageImporterContractTest,
     ReopeningChangedSourceTransitionsValidInvalidAndValidWithoutStalePixels)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("mutable-source.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageImporter importer;
    ASSERT_TRUE(importer.open(path));
    const RawImage initial = importer.readImage();
    ASSERT_TRUE(initial.isValid());
    EXPECT_EQ(initial.width, 128);
    EXPECT_EQ(initial.height, 128);

    ASSERT_TRUE(writeFile(path, QByteArrayLiteral("not an image anymore")));
    ASSERT_TRUE(importer.open(path));
    EXPECT_FALSE(importer.readImage().isValid());

    ASSERT_TRUE(writeFile(path, onePixelPng()));
    ASSERT_TRUE(importer.open(path));
    const RawImage recovered = importer.readImage();
    ASSERT_TRUE(recovered.isValid());
    EXPECT_EQ(recovered.width, 1);
    EXPECT_EQ(recovered.height, 1);
}

TEST(ImageSequenceSourceContractTest, PreservesExplicitMissingFrameSlots)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("shot_0001.png"));
    const QString missingPath = directory.filePath(QStringLiteral("shot_0002.png"));
    const QString thirdPath = directory.filePath(QStringLiteral("shot_0003.png"));
    ASSERT_TRUE(writeFile(firstPath, checkerPng()));
    ASSERT_TRUE(writeFile(thirdPath, checkerPng()));

    ImageSequenceSource source;
    source.setFrameRate(12.0);
    ASSERT_TRUE(source.openFramePaths({firstPath, missingPath, thirdPath}));

    EXPECT_TRUE(source.isOpen());
    EXPECT_EQ(source.frameCount(), 3);
    EXPECT_EQ(source.frameRate(), 12.0);
    EXPECT_EQ(source.sourceFrameNumberAt(0), 0);
    EXPECT_EQ(source.sourceFrameNumberAt(1), 1);
    EXPECT_EQ(source.sourceFrameNumberAt(2), 2);
    EXPECT_EQ(source.sequenceIndexForSourceFrame(1), 1);
    EXPECT_EQ(source.sequenceIndexForSourceFrame(3), -1);
    const SourceMetadata metadata = source.metadata();
    EXPECT_EQ(metadata.frameCount, 3);
    EXPECT_EQ(metadata.frameStart, 0);
    EXPECT_EQ(metadata.frameEnd, 2);
    EXPECT_EQ(metadata.missingFrameCount, 1);
    EXPECT_TRUE(metadata.hasVideo);
    EXPECT_TRUE(metadata.isSequence);
    EXPECT_EQ(metadata.uri, QFileInfo(firstPath).absoluteFilePath());
    EXPECT_EQ(source.kind(), SourceKind::ImageSequence);
    EXPECT_EQ(metadata.frameSize.width(), 128);
    EXPECT_EQ(metadata.frameSize.height(), 128);
    EXPECT_TRUE(source.frameAt(1).isNull());
    EXPECT_FALSE(source.frameAt(2).isNull());
    EXPECT_EQ(source.metadata().missingFrameCount, 1);

    ASSERT_TRUE(writeFile(missingPath, checkerPng()));
    EXPECT_FALSE(source.frameAt(1).isNull());
    EXPECT_EQ(source.metadata().missingFrameCount, 0);
    ASSERT_TRUE(QFile::remove(missingPath));
    EXPECT_TRUE(source.frameAt(1).isNull());
    EXPECT_EQ(source.metadata().missingFrameCount, 1);

    source.close();
    EXPECT_FALSE(source.isOpen());
    EXPECT_EQ(source.frameCount(), 0);
    ASSERT_TRUE(source.openFramePaths({firstPath, thirdPath}));
    EXPECT_DOUBLE_EQ(source.frameRate(), 12.0);
}

TEST(ImageSequenceSourceContractTest, MapsNumberedFilesAcrossGaps)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("take_0001.png"));
    const QString thirdPath = directory.filePath(QStringLiteral("take_0003.png"));
    ASSERT_TRUE(writeFile(firstPath, checkerPng()));
    ASSERT_TRUE(writeFile(thirdPath, checkerPng()));

    ImageSequenceSource source;
    ASSERT_TRUE(source.open(firstPath));
    const SourceMetadata metadata = source.metadata();
    EXPECT_EQ(source.frameCount(), 2);
    EXPECT_EQ(metadata.frameStart, 1);
    EXPECT_EQ(metadata.frameEnd, 3);
    EXPECT_EQ(metadata.missingFrameCount, 1);
    EXPECT_TRUE(metadata.isSequence);
    EXPECT_EQ(source.sourceFrameNumberAt(0), 1);
    EXPECT_EQ(source.sourceFrameNumberAt(1), 3);
    EXPECT_EQ(source.sequenceIndexForSourceFrame(3), 1);
    EXPECT_EQ(source.sequenceIndexForSourceFrame(2), -1);
    EXPECT_FALSE(source.seek(-1));
    EXPECT_FALSE(source.seek(source.frameCount()));
    EXPECT_EQ(source.currentFrameIndex(), 0);

    EXPECT_TRUE(source.seekSourceFrame(3));
    EXPECT_EQ(source.currentFrameIndex(), 1);
    EXPECT_FALSE(source.seekSourceFrame(2));
    EXPECT_EQ(source.frameIndexAtTime(1, 24.0), 1);
    EXPECT_EQ(source.frameIndexAtTime(100, 24.0), 1);
}

TEST(ImageSequenceSourceContractTest,
     ASingleNumberedImagePathRemainsOneStillFrameNotASequence)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("shot_0042.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageSequenceSource source;
    ASSERT_TRUE(source.open(path));

    const SourceMetadata metadata = source.metadata();
    EXPECT_EQ(source.frameCount(), 1);
    EXPECT_EQ(source.sourceFrameNumberAt(0), 42);
    EXPECT_EQ(source.sequenceIndexForSourceFrame(42), 0);
    EXPECT_EQ(metadata.frameStart, 42);
    EXPECT_EQ(metadata.frameEnd, 42);
    EXPECT_EQ(metadata.missingFrameCount, 0);
    EXPECT_FALSE(metadata.isSequence);
    EXPECT_TRUE(metadata.hasVideo);
    const QImage frame = source.frameAt(0);
    ASSERT_FALSE(frame.isNull());
    EXPECT_EQ(frame.size(), QSize(128, 128));
}

TEST(ImageSequenceSourceContractTest,
     InvalidFrameRatesDoNotReplaceTheLastValidRequestedRate)
{
    ImageSequenceSource source;
    source.setFrameRate(48.0);
    ASSERT_DOUBLE_EQ(source.frameRate(), 48.0);

    const double invalidRates[] = {
        -1.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };
    for (const double invalidRate : invalidRates) {
        source.setFrameRate(invalidRate);
        EXPECT_DOUBLE_EQ(source.frameRate(), 48.0) << invalidRate;
    }
}

TEST(ImageSequenceSourceContractTest,
     ExplicitFrameRateSurvivesSingleFileOpenCloseAndReopen)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("single.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageSequenceSource source;
    source.setFrameRate(48.0);
    ASSERT_TRUE(source.open(path));
    EXPECT_DOUBLE_EQ(source.frameRate(), 48.0);
    EXPECT_DOUBLE_EQ(source.metadata().frameRate, 48.0);

    source.close();
    EXPECT_DOUBLE_EQ(source.frameRate(), 48.0);
    ASSERT_TRUE(source.open(path));
    EXPECT_DOUBLE_EQ(source.frameRate(), 48.0);
    EXPECT_DOUBLE_EQ(source.metadata().frameRate, 48.0);
}

TEST(ImageSequenceSourceContractTest,
     AnimatedGifUsesAverageFrameDelayAndExposesEverySubframe)
{
    if (!QImageReader::supportedImageFormats().contains(QByteArrayLiteral("gif"))) {
        GTEST_SKIP() << "Qt GIF image plugin is unavailable";
    }

    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("timed.gif"));
    const QByteArray gif = QByteArray::fromBase64(QByteArrayLiteral(
        "R0lGODlhAQABAIEAAP8AAAAAAAAAAAAAACH/C05FVFNDQVBFMi4wAwEAAAAh+QQICgAAACwAAAAA"
        "AQABAAAIBAABBAQAIfkECBQAAAAsAAAAAAEAAQCBAP8AAAAAAAAAAAAACAQAAQQEACH5BAgeAAAA"
        "LAAAAAABAAEAgQAA/wAAAAAAAAAAAAgEAAEEBAA7"));
    ASSERT_TRUE(writeFile(path, gif));

    QImageReader reader(path);
    ASSERT_TRUE(reader.canRead());
    ASSERT_TRUE(reader.supportsAnimation());
    ASSERT_EQ(reader.imageCount(), 3);

    ImageSequenceSource source;
    ASSERT_TRUE(source.open(path));
    EXPECT_EQ(source.frameCount(), 3);
    EXPECT_TRUE(source.metadata().isSequence);
    EXPECT_DOUBLE_EQ(source.frameRate(), 5.0)
        << "three frames with 100+200+300 ms delays average to 5 FPS";

    const QColor expectedColors[] = {QColor(255, 0, 0), QColor(0, 255, 0),
                                     QColor(0, 0, 255)};
    for (qint64 index = 0; index < 3; ++index) {
        EXPECT_EQ(source.sourceFrameNumberAt(index), index);
        const QImage frame = source.frameAt(index);
        ASSERT_FALSE(frame.isNull()) << "subframe=" << index;
        EXPECT_EQ(frame.pixelColor(0, 0), expectedColors[index])
            << "subframe=" << index;
    }
}

TEST(ImageSequenceSourceContractTest, CachesDecodedFramesAndExplicitlyClearsCache)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("frame.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths({path}));
    EXPECT_EQ(source.frameCacheHitCount(), 0u);
    EXPECT_EQ(source.frameCacheMissCount(), 0u);
    EXPECT_TRUE(source.frameAt(-1).isNull());
    EXPECT_TRUE(source.frameAt(source.frameCount()).isNull());
    EXPECT_EQ(source.frameCacheEntryCount(), 0);
    EXPECT_EQ(source.frameCacheMissCount(), 0u);

    ASSERT_FALSE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 1u);
    EXPECT_EQ(source.frameCacheHitCount(), 0u);
    ASSERT_FALSE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 1u);
    EXPECT_EQ(source.frameCacheHitCount(), 1u);
    EXPECT_EQ(source.frameCacheEntryCount(), 1);
    EXPECT_GT(source.frameCacheBytes(), 0u);

    source.clearFrameCache();
    EXPECT_EQ(source.frameCacheEntryCount(), 0);
    EXPECT_EQ(source.frameCacheBytes(), 0u);
    EXPECT_EQ(source.frameCacheHitCount(), 0u);
    EXPECT_EQ(source.frameCacheMissCount(), 0u);
}

TEST(ImageSequenceSourceContractTest,
     ReopeningSequenceDiscardsCompletedAndInFlightPrefetchFromPreviousGeneration)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString oldPath = directory.filePath(QStringLiteral("old_red.png"));
    const QString newPath = directory.filePath(QStringLiteral("new_blue.png"));
    ASSERT_TRUE(writeFile(oldPath, onePixelPng()));
    ASSERT_TRUE(writeFile(newPath, blueOnePixelPng()));
    ASSERT_EQ(QFileInfo(oldPath).size(), QFileInfo(newPath).size());
    constexpr qint64 sharedTimestampMs = 1700000000000LL;
    const auto setModifiedTime = [sharedTimestampMs](const QString& path) {
        QFile file(path);
        return file.open(QIODevice::ReadWrite) &&
               file.setFileTime(QDateTime::fromMSecsSinceEpoch(sharedTimestampMs),
                                QFileDevice::FileModificationTime);
    };
    ASSERT_TRUE(setModifiedTime(oldPath));
    ASSERT_TRUE(setModifiedTime(newPath));
    ASSERT_EQ(QFileInfo(oldPath).lastModified().toMSecsSinceEpoch(), sharedTimestampMs);
    ASSERT_EQ(QFileInfo(newPath).lastModified().toMSecsSinceEpoch(), sharedTimestampMs);

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths({oldPath}));
    source.prefetchFrame(0);
    ASSERT_TRUE(source.openFramePaths({newPath}));

    QImage frame;
    bool ready = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!ready && std::chrono::steady_clock::now() < deadline) {
        ready = source.tryFrameAt(0, frame);
        if (!ready) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_TRUE(ready) << "the new generation's prefetch should eventually complete";
    ASSERT_FALSE(frame.isNull());
    EXPECT_EQ(frame.pixelColor(0, 0).red(), 0);
    EXPECT_EQ(frame.pixelColor(0, 0).blue(), 255);
}

TEST(ImageSequenceSourceContractTest, KeepsDecodedFrameCacheWithinCapacity)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QStringList framePaths;
    for (int index = 0; index < 10; ++index) {
        const QString path = directory.filePath(
            QStringLiteral("frame_%1.png").arg(index, 4, 10, QLatin1Char('0')));
        ASSERT_TRUE(writeFile(path, checkerPng()));
        framePaths.push_back(path);
    }

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths(framePaths));
    for (qint64 index = 0; index < source.frameCount(); ++index) {
        ASSERT_FALSE(source.frameAt(index).isNull());
    }

    EXPECT_EQ(source.frameCacheCapacity(), 8);
    EXPECT_LE(source.frameCacheEntryCount(), source.frameCacheCapacity());
    EXPECT_LE(source.frameCacheBytes(), source.frameCacheByteCapacity());
    EXPECT_EQ(source.frameCacheMissCount(), 10u);
    ASSERT_FALSE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 11u);
    ASSERT_FALSE(source.frameAt(9).isNull());
    EXPECT_EQ(source.frameCacheHitCount(), 1u);
}

TEST(ImageSequenceSourceContractTest, InvalidatesCachedFrameWhenSourceFileChanges)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("replaceable.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths({path}));
    ASSERT_FALSE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 1u);

    ASSERT_TRUE(writeFile(path, QByteArrayLiteral("invalid replacement")));
    EXPECT_TRUE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 2u);
    EXPECT_EQ(source.frameCacheBytes(), 0u);
    EXPECT_TRUE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 2u);
    EXPECT_EQ(source.frameCacheHitCount(), 1u);

    ASSERT_TRUE(writeFile(path, checkerPng()));
    EXPECT_FALSE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 3u);
}

TEST(ImageSequenceSourceContractTest, InvalidatesSameSizeReplacementByModificationTime)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("same_size.png"));
    const QByteArray firstImage = onePixelPng();
    const QByteArray replacementImage = blueOnePixelPng();
    ASSERT_EQ(firstImage.size(), replacementImage.size());
    ASSERT_TRUE(writeFile(path, firstImage));

    const auto setModifiedTime = [&path](qint64 millisecondsSinceEpoch) {
        QFile file(path);
        return file.open(QIODevice::ReadWrite) &&
               file.setFileTime(QDateTime::fromMSecsSinceEpoch(millisecondsSinceEpoch),
                                QFileDevice::FileModificationTime);
    };
    ASSERT_TRUE(setModifiedTime(1700000000000LL));

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths({path}));
    const QImage firstFrame = source.frameAt(0);
    ASSERT_FALSE(firstFrame.isNull());
    EXPECT_EQ(firstFrame.pixelColor(0, 0).red(), 255);
    EXPECT_EQ(firstFrame.pixelColor(0, 0).blue(), 0);

    ASSERT_TRUE(writeFile(path, replacementImage));
    ASSERT_TRUE(setModifiedTime(1700000010000LL));
    const QImage replacementFrame = source.frameAt(0);
    ASSERT_FALSE(replacementFrame.isNull());
    EXPECT_EQ(replacementFrame.pixelColor(0, 0).red(), 0);
    EXPECT_EQ(replacementFrame.pixelColor(0, 0).blue(), 255);
    EXPECT_EQ(source.frameCacheMissCount(), 2u);
}

TEST(ImageSequenceSourceContractTest,
     InvalidatesSameSizeReplacementWhenModificationTimeIsPreserved)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("same_metadata.png"));
    const QByteArray firstImage = onePixelPng();
    const QByteArray replacementImage = blueOnePixelPng();
    ASSERT_EQ(firstImage.size(), replacementImage.size());
    ASSERT_TRUE(writeFile(path, firstImage));

    constexpr qint64 fixedTimestampMs = 1700000000000LL;
    const auto setModifiedTime = [&path](qint64 millisecondsSinceEpoch) {
        QFile file(path);
        return file.open(QIODevice::ReadWrite) &&
               file.setFileTime(QDateTime::fromMSecsSinceEpoch(millisecondsSinceEpoch),
                                QFileDevice::FileModificationTime);
    };
    ASSERT_TRUE(setModifiedTime(fixedTimestampMs));
    const QFileInfo originalInfo(path);
    ASSERT_TRUE(originalInfo.exists());
    const qint64 originalSize = originalInfo.size();
    const qint64 originalTimestampMs =
        originalInfo.lastModified().toMSecsSinceEpoch();
    ASSERT_EQ(originalTimestampMs, fixedTimestampMs);

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths({path}));
    const QImage originalFrame = source.frameAt(0);
    ASSERT_FALSE(originalFrame.isNull());
    EXPECT_EQ(originalFrame.pixelColor(0, 0).red(), 255);
    EXPECT_EQ(originalFrame.pixelColor(0, 0).blue(), 0);

    ASSERT_TRUE(writeFile(path, replacementImage));
    ASSERT_TRUE(setModifiedTime(fixedTimestampMs));
    const QFileInfo replacementInfo(path);
    ASSERT_EQ(replacementInfo.size(), originalSize);
    ASSERT_EQ(replacementInfo.lastModified().toMSecsSinceEpoch(),
              originalTimestampMs);

    const QImage replacementFrame = source.frameAt(0);
    ASSERT_FALSE(replacementFrame.isNull());
    EXPECT_EQ(replacementFrame.pixelColor(0, 0).red(), 0);
    EXPECT_EQ(replacementFrame.pixelColor(0, 0).blue(), 255);
    EXPECT_EQ(source.frameCacheMissCount(), 2u);
    EXPECT_EQ(source.frameCacheHitCount(), 0u);
}

TEST(ImageSequenceSourceContractTest,
     TryFrameAtDropsStaleSameSizeFrameBeforeAsyncRefresh)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("async-replacement.png"));
    const QByteArray originalImage = onePixelPng();
    const QByteArray replacementImage = blueOnePixelPng();
    ASSERT_EQ(originalImage.size(), replacementImage.size());
    ASSERT_TRUE(writeFile(path, originalImage));

    const auto setModifiedTime = [&path](qint64 millisecondsSinceEpoch) {
        QFile file(path);
        return file.open(QIODevice::ReadWrite) &&
               file.setFileTime(QDateTime::fromMSecsSinceEpoch(millisecondsSinceEpoch),
                                QFileDevice::FileModificationTime);
    };
    ASSERT_TRUE(setModifiedTime(1700000000000LL));

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths({path}));
    const QImage originalFrame = source.frameAt(0);
    ASSERT_FALSE(originalFrame.isNull());
    EXPECT_EQ(originalFrame.pixelColor(0, 0).red(), 255);
    EXPECT_EQ(originalFrame.pixelColor(0, 0).blue(), 0);

    ASSERT_TRUE(writeFile(path, replacementImage));
    ASSERT_TRUE(setModifiedTime(1700000010000LL));
    ASSERT_EQ(QFileInfo(path).size(), originalImage.size());

    QImage refreshedFrame;
    EXPECT_FALSE(source.tryFrameAt(0, refreshedFrame));
    EXPECT_TRUE(refreshedFrame.isNull())
        << "a stale cached pixel must not be returned while the new frame is loading";

    bool ready = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!ready && std::chrono::steady_clock::now() < deadline) {
        ready = source.tryFrameAt(0, refreshedFrame);
        if (!ready) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_TRUE(ready) << "the replacement frame should eventually finish prefetch";
    ASSERT_FALSE(refreshedFrame.isNull());
    EXPECT_EQ(refreshedFrame.pixelColor(0, 0).red(), 0);
    EXPECT_EQ(refreshedFrame.pixelColor(0, 0).blue(), 255);
}

TEST(ImageSequenceSourceContractTest, ConvertsTimelineFramesUsingSourceAndTimelineRates)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QStringList framePaths;
    for (int index = 0; index < 6; ++index) {
        const QString path = directory.filePath(
            QStringLiteral("rate_%1.png").arg(index));
        ASSERT_TRUE(writeFile(path, checkerPng()));
        framePaths.push_back(path);
    }

    ImageSequenceSource source;
    source.setFrameRate(12.0);
    ASSERT_TRUE(source.openFramePaths(framePaths));
    EXPECT_DOUBLE_EQ(source.frameRate(), 12.0);
    EXPECT_EQ(source.frameIndexAtTime(0, 24.0), 0);
    EXPECT_EQ(source.frameIndexAtTime(1, 24.0), 0);
    EXPECT_EQ(source.frameIndexAtTime(2, 24.0), 1);
    EXPECT_EQ(source.frameIndexAtTime(3, 24.0), 1);
    EXPECT_EQ(source.frameIndexAtTime(10, 24.0), 5);
}

TEST(ImageSequenceSourceContractTest,
     ConvertsFractionalVideoRatesAcrossBoundariesWithoutDrifting)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QStringList framePaths;
    for (int index = 0; index < 10; ++index) {
        const QString path = directory.filePath(
            QStringLiteral("fractional_%1.png").arg(index));
        ASSERT_TRUE(writeFile(path, checkerPng()));
        framePaths.push_back(path);
    }

    ImageSequenceSource source;
    source.setFrameRate(24000.0 / 1001.0);
    ASSERT_TRUE(source.openFramePaths(framePaths));

    constexpr std::array expectedIndices{0, 0, 1, 2, 3, 4, 4, 5, 6, 7, 8, 8, 9};
    constexpr double timelineRate = 30000.0 / 1001.0;
    for (std::size_t timelineFrame = 0;
         timelineFrame < expectedIndices.size(); ++timelineFrame) {
        EXPECT_EQ(source.frameIndexAtTime(
                      static_cast<qint64>(timelineFrame), timelineRate),
                  expectedIndices[timelineFrame])
            << "timeline frame=" << timelineFrame;
    }
}

TEST(ImageSequenceSourceContractTest, ConvertsTimeToSequenceIndexAcrossSparseSourceFrames)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("shot_0100.png"));
    const QString laterPath = directory.filePath(QStringLiteral("shot_0104.png"));
    ASSERT_TRUE(writeFile(firstPath, checkerPng()));
    ASSERT_TRUE(writeFile(laterPath, checkerPng()));

    ImageSequenceSource source;
    source.setFrameRate(30.0);
    ASSERT_TRUE(source.open(firstPath));

    // Time mapping returns the position in the opened sequence, not the filename frame number.
    EXPECT_EQ(source.frameIndexAtTime(0, 24.0), 0);
    EXPECT_EQ(source.frameIndexAtTime(1, 24.0), 1);
    EXPECT_EQ(source.frameIndexAtTime(2, 24.0), 1);
    EXPECT_EQ(source.frameIndexAtTime(3, 24.0), 1);
    EXPECT_EQ(source.sourceFrameNumberAt(source.frameIndexAtTime(1, 24.0)), 104);
    EXPECT_EQ(source.sequenceIndexForSourceFrame(104), 1);
    EXPECT_EQ(source.sequenceIndexForSourceFrame(101), -1);
}

TEST(ImageSequenceSourceContractTest, SaturatesUnrepresentableTimelinePositionsToLastFrame)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QStringList framePaths;
    for (int index = 0; index < 3; ++index) {
        const QString path = directory.filePath(
            QStringLiteral("overflow_%1.png").arg(index));
        ASSERT_TRUE(writeFile(path, checkerPng()));
        framePaths.push_back(path);
    }

    ImageSequenceSource source;
    source.setFrameRate(30.0);
    ASSERT_TRUE(source.openFramePaths(framePaths));

    EXPECT_EQ(source.frameIndexAtTime(std::numeric_limits<qint64>::max(), 24.0),
              2);
    EXPECT_EQ(source.frameIndexAtTime(1, std::numeric_limits<double>::min()), 2);
}

TEST(ImageSequenceSourceContractTest, FailedReopenClearsPreviouslyOpenedSequence)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("frame.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths({path}));
    ASSERT_TRUE(source.isOpen());
    ASSERT_EQ(source.frameCount(), 1);

    EXPECT_FALSE(source.openFramePaths({}));
    EXPECT_FALSE(source.isOpen());
    EXPECT_EQ(source.frameCount(), 0);
    EXPECT_TRUE(source.uri().isEmpty());
    EXPECT_TRUE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameIndexAtTime(1, 24.0), -1);
}

TEST(ImageSequenceSourceContractTest, DirectoryOpenListsSupportedImagesAtTopLevel)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(writeFile(directory.filePath(QStringLiteral("b.png")), checkerPng()));
    ASSERT_TRUE(writeFile(directory.filePath(QStringLiteral("a.png")), onePixelPng()));
    ASSERT_TRUE(writeFile(directory.filePath(QStringLiteral("notes.txt")),
                          QByteArrayLiteral("not an image")));
    ASSERT_TRUE(QDir().mkdir(directory.filePath(QStringLiteral("nested"))));
    ASSERT_TRUE(writeFile(directory.filePath(QStringLiteral("nested/c.png")), onePixelPng()));

    ImageSequenceSource source;
    ASSERT_TRUE(source.open(directory.path()));
    EXPECT_EQ(source.frameCount(), 2);
    EXPECT_EQ(source.sourceFrameNumberAt(0), 0);
    EXPECT_EQ(source.sourceFrameNumberAt(1), 1);
    EXPECT_EQ(source.frameSize().width(), 1);
    EXPECT_EQ(source.frameSize().height(), 1);
    EXPECT_EQ(source.metadata().missingFrameCount, 0);
    EXPECT_TRUE(source.metadata().isSequence);
    EXPECT_EQ(source.frameAt(0).size(), QSize(1, 1));
    EXPECT_EQ(source.frameAt(1).size(), QSize(128, 128));
}
