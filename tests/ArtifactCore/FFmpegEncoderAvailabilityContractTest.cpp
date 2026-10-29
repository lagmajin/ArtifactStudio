#include <gtest/gtest.h>

#include <OpenImageIO/imageio.h>

#include <array>
#include <string>
#include <vector>

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QIODevice>
#include <QTemporaryDir>
#include <QString>
#include <QStringList>

import Encoder.FFmpegEncoder;
import Image.ImageF32x4_RGBA;

using ArtifactCore::FFmpegEncoder;

TEST(FFmpegEncoderAvailabilityContractTest,
     UnsupportedSequenceFormatLeavesEncoderClosedAndAllowsRetry)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    FFmpegEncoder encoder;
    ArtifactCore::FFmpegImageSequenceSettings settings;
    settings.width = 1;
    settings.height = 1;
    settings.format = QStringLiteral("unsupported-image-format");

    const QString invalidPattern = directory.filePath(QStringLiteral("invalid_%04d.png"));
    const QString protectedFrame = directory.filePath(
        QStringLiteral("invalid_0001.png"));
    const QByteArray previousFrame = QByteArrayLiteral("preserve-existing-frame");
    QFile existingFrame(protectedFrame);
    ASSERT_TRUE(existingFrame.open(QIODevice::WriteOnly));
    ASSERT_EQ(existingFrame.write(previousFrame), previousFrame.size());
    existingFrame.close();

    EXPECT_FALSE(encoder.openImageSequence(invalidPattern, settings));
    EXPECT_FALSE(encoder.isOpen());
    EXPECT_FALSE(encoder.isImageSequence());
    EXPECT_TRUE(encoder.lastError().contains(QStringLiteral("Unsupported image sequence format")));
    QFile verifyExistingFrame(protectedFrame);
    ASSERT_TRUE(verifyExistingFrame.open(QIODevice::ReadOnly));
    EXPECT_EQ(verifyExistingFrame.readAll(), previousFrame);

    settings.format = QStringLiteral("exr");
    const QString exrPattern = directory.filePath(QStringLiteral("valid_%04d.exr"));
    ASSERT_TRUE(encoder.openImageSequence(exrPattern, settings))
        << encoder.lastError().toStdString();
    EXPECT_TRUE(encoder.isOpen());
    EXPECT_TRUE(encoder.isImageSequence());
    encoder.close();
    EXPECT_FALSE(encoder.isOpen());
}

TEST(FFmpegEncoderAvailabilityContractTest,
     ExrImageSequenceWritesFloatRgbaFrameAtConfiguredNumber)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString pattern = directory.filePath(QStringLiteral("frame_%04d.exr"));
    ArtifactCore::FFmpegEncoder encoder;
    ArtifactCore::FFmpegImageSequenceSettings settings;
    settings.format = QStringLiteral("exr");
    settings.width = 2;
    settings.height = 1;
    settings.startFrame = 7;
    settings.padding = 3;
    settings.is32bit = true;
    ASSERT_TRUE(encoder.openImageSequence(pattern, settings))
        << encoder.lastError().toStdString();

    constexpr std::array<float, 8> sourcePixels{
        0.125f, 0.25f, 0.5f, 0.75f,
        0.8f, 0.6f, 0.4f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(sourcePixels.data(), 2, 1);
    ASSERT_TRUE(encoder.addImage(image)) << encoder.lastError().toStdString();
    encoder.close();

    const QString framePath = directory.filePath(QStringLiteral("frame_007.exr"));
    const QByteArray encodedPath = framePath.toUtf8();
    auto input = OIIO::ImageInput::open(encodedPath.constData());
    ASSERT_NE(input, nullptr) << framePath.toStdString();
    const auto& spec = input->spec();
    EXPECT_EQ(spec.width, 2);
    EXPECT_EQ(spec.height, 1);
    EXPECT_EQ(spec.nchannels, 4);
    EXPECT_EQ(spec.format.basetype, OIIO::TypeDesc::FLOAT);
    EXPECT_EQ(spec.channelnames,
              (std::vector<std::string>{"R", "G", "B", "A"}));
    std::vector<float> decoded(sourcePixels.size());
    ASSERT_TRUE(input->read_image(OIIO::TypeDesc::FLOAT, decoded.data()));
    input->close();
    for (std::size_t index = 0; index < sourcePixels.size(); ++index) {
        EXPECT_FLOAT_EQ(decoded[index], sourcePixels[index])
            << "component " << index;
    }
}

TEST(FFmpegEncoderAvailabilityContractTest,
     ReportedVideoCodecsAreUniqueAndPassTheAvailabilityPredicate)
{
    const QStringList codecs = FFmpegEncoder::availableVideoCodecs();
    for (int index = 0; index < codecs.size(); ++index) {
        const QString& codec = codecs.at(index);
        EXPECT_FALSE(codecs.mid(index + 1).contains(codec)) << codec.toStdString();
        EXPECT_TRUE(FFmpegEncoder::isCodecAvailable(codec)) << codec.toStdString();
    }
}

TEST(FFmpegEncoderAvailabilityContractTest,
     CodecAliasesNormalizeCaseAndWhitespaceWithoutInventingUnknownCodecs)
{
    const QStringList aliases{
        QStringLiteral("h264"), QStringLiteral("avc"), QStringLiteral("libx264"),
        QStringLiteral("h265"), QStringLiteral("hevc"), QStringLiteral("libx265"),
        QStringLiteral("prores"), QStringLiteral("apple_prores"),
        QStringLiteral("vp9"), QStringLiteral("libvpx-vp9"),
        QStringLiteral("webp"), QStringLiteral("libwebp_anim"),
        QStringLiteral("libwebp")};

    for (const QString& alias : aliases) {
        const bool available = FFmpegEncoder::isCodecAvailable(alias);
        EXPECT_EQ(FFmpegEncoder::isCodecAvailable(
                      QStringLiteral("  %1  ").arg(alias.toUpper())),
                  available) << alias.toStdString();
    }

    EXPECT_FALSE(FFmpegEncoder::isCodecAvailable(QStringLiteral("unknown-codec")));
    EXPECT_FALSE(FFmpegEncoder::isCodecAvailable(QStringLiteral("")));
}

TEST(FFmpegEncoderAvailabilityContractTest,
     CodecNameNormalizationIsCaseAndWhitespaceInsensitive)
{
    for (const QString& codec : {QStringLiteral("h264"), QStringLiteral("h265"),
                                 QStringLiteral("prores"), QStringLiteral("vp9"),
                                 QStringLiteral("mjpeg"), QStringLiteral("png")}) {
        const bool available = FFmpegEncoder::isCodecAvailable(codec);
        EXPECT_EQ(FFmpegEncoder::isCodecAvailable(codec.toUpper()), available)
            << codec.toStdString();
        EXPECT_EQ(FFmpegEncoder::isCodecAvailable(
                      QStringLiteral("  %1  ").arg(codec)), available)
            << codec.toStdString();
    }
}

TEST(FFmpegEncoderAvailabilityContractTest,
     ReportedContainersAreUniqueAndPassTheAvailabilityPredicate)
{
    const QStringList containers = FFmpegEncoder::availableContainers();
    for (int index = 0; index < containers.size(); ++index) {
        const QString& container = containers.at(index);
        EXPECT_FALSE(containers.mid(index + 1).contains(container))
            << container.toStdString();
        EXPECT_TRUE(FFmpegEncoder::isContainerAvailable(container))
            << container.toStdString();
    }

    EXPECT_FALSE(FFmpegEncoder::isContainerAvailable(
        QStringLiteral("artifact-unknown-container")));
}

TEST(FFmpegEncoderAvailabilityContractTest,
     ReportedContainersAreCanonicalNamesThatResolveToTheirPublishedFormats)
{
    const QStringList containers = FFmpegEncoder::availableContainers();
    ASSERT_FALSE(containers.isEmpty());
    for (const QString& container : containers) {
        EXPECT_EQ(container, container.toLower()) << container.toStdString();
        EXPECT_EQ(container, container.trimmed()) << container.toStdString();
        EXPECT_TRUE(FFmpegEncoder::isContainerAvailable(container))
            << container.toStdString();
    }
}

TEST(FFmpegEncoderAvailabilityContractTest,
     EncoderNamesRejectWhitespaceOnlyAndUnknownNames)
{
    EXPECT_FALSE(FFmpegEncoder::isEncoderAvailableByName(QStringLiteral("")));
    EXPECT_FALSE(FFmpegEncoder::isEncoderAvailableByName(QStringLiteral("   ")));
    EXPECT_FALSE(FFmpegEncoder::isEncoderAvailableByName(
        QStringLiteral("artifact-unknown-encoder")));
}

TEST(FFmpegEncoderAvailabilityContractTest,
     MissingPreferredHardwareEncoderFallsBackToSoftwareCodec)
{
    if (!FFmpegEncoder::isCodecAvailable(QStringLiteral("mjpeg"))) {
        GTEST_SKIP() << "FFmpeg MJPEG software encoder is unavailable";
    }

    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString outputPath = directory.filePath(
        QStringLiteral("software-fallback.avi"));
    ArtifactCore::FFmpegEncoderSettings settings;
    settings.width = 16;
    settings.height = 16;
    settings.fps = 2.0;
    settings.videoCodec = QStringLiteral("mjpeg");
    settings.container = QStringLiteral("avi");
    settings.encoderName = QStringLiteral("artifact-absent-hardware-encoder");
    settings.preferHardware = true;
    settings.threadCount = 1;
    EXPECT_FALSE(FFmpegEncoder::isEncoderAvailableByName(settings.encoderName));

    FFmpegEncoder encoder;
    ASSERT_TRUE(encoder.open(outputPath, settings))
        << encoder.lastError().toStdString();
    EXPECT_TRUE(encoder.isOpen());

    QImage frame(settings.width, settings.height, QImage::Format_RGBA8888);
    frame.fill(QColor(220, 40, 20, 255));
    ASSERT_TRUE(encoder.addImage(frame)) << encoder.lastError().toStdString();
    encoder.close();

    EXPECT_FALSE(encoder.isOpen());
    ASSERT_TRUE(QFileInfo::exists(outputPath));
    EXPECT_GT(QFileInfo(outputPath).size(), 0);
}

TEST(FFmpegEncoderAvailabilityContractTest,
     ImageSequenceFormatsMatchThePublishedListAndNormalizeCase)
{
    const QStringList formats = FFmpegEncoder::availableImageSequenceFormats();
    ASSERT_FALSE(formats.isEmpty());
    for (int index = 0; index < formats.size(); ++index) {
        const QString& format = formats.at(index);
        EXPECT_FALSE(formats.mid(index + 1).contains(format))
            << format.toStdString();
        EXPECT_TRUE(FFmpegEncoder::isImageSequenceFormatAvailable(format))
            << format.toStdString();
        EXPECT_TRUE(FFmpegEncoder::isImageSequenceFormatAvailable(format.toUpper()))
            << format.toStdString();
    }

    EXPECT_FALSE(FFmpegEncoder::isImageSequenceFormatAvailable(
        QStringLiteral("artifact-unknown-image-sequence")));
    EXPECT_TRUE(FFmpegEncoder::isImageSequenceFormatAvailable(
        QStringLiteral("jpg")));
    EXPECT_FALSE(formats.contains(QStringLiteral("jpg")));
}
