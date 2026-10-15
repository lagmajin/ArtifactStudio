#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstring>

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QString>
#include <QTemporaryDir>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imageio.h>

import Color.TransferFunction;
import IO.ImageExporter;
import IO.ImageImporter;
import Image.ExportOptions;
import Image.Raw;

using namespace ArtifactCore;

namespace {

TEST(ImageIoContractTest, ExportsCineonEncodedFloatPixelsAndPreservesAlpha)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString outputPath = temporaryDirectory.filePath(
        QStringLiteral("cineon-linear-source.exr"));

    const OIIO::ImageSpec sourceSpec(1, 1, 4, OIIO::TypeDesc::FLOAT);
    OIIO::ImageBuf source(sourceSpec);
    const std::array<float, 4> sourcePixel{0.18f, 1.0f, 4.0f, 0.375f};
    ASSERT_TRUE(source.set_pixels(OIIO::ROI::All(), OIIO::TypeDesc::FLOAT,
                                  sourcePixel.data()));

    ImageExportOptions options;
    options.dataType = OIIO::TypeDesc::FLOAT;
    options.colorSpace = QStringLiteral("Linear");
    options.inputTransferFunction = QStringLiteral("linear");
    options.outputTransferFunction = QStringLiteral("cineon");

    ImageExporter exporter;
    const ImageExportResult result = exporter.write(source, outputPath, options);
    ASSERT_TRUE(result.success) << result.errorStage.toStdString() << ": "
                                << result.errorMessage.toStdString();

    OIIO::ImageBuf encoded(outputPath.toUtf8().constData());
    ASSERT_TRUE(encoded.read(0, 0, true, OIIO::TypeDesc::FLOAT));
    std::array<float, 4> encodedPixel{};
    ASSERT_TRUE(encoded.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::FLOAT,
                                   encodedPixel.data()));
    EXPECT_NEAR(encodedPixel[0],
                ColorTransferFunction::encode(sourcePixel[0],
                                              TransferFunction::Cineon),
                1.0e-5f);
    EXPECT_NEAR(encodedPixel[1],
                ColorTransferFunction::encode(sourcePixel[1],
                                              TransferFunction::Cineon),
                1.0e-5f);
    EXPECT_NEAR(encodedPixel[2],
                ColorTransferFunction::encode(sourcePixel[2],
                                              TransferFunction::Cineon),
                1.0e-5f);
    EXPECT_FLOAT_EQ(encodedPixel[3], sourcePixel[3]);

    const OIIO::ParamValue* transfer = encoded.spec().find_attribute(
        "TransferFunction", OIIO::TypeDesc::STRING);
    ASSERT_NE(transfer, nullptr);
    EXPECT_EQ(transfer->get_string(), "cineon");
}

TEST(ImageIoContractTest, CineonTransferMatchesReferenceCodeAnchors)
{
    constexpr float blackCode = 95.0f / 1023.0f;
    constexpr float whiteCode = 685.0f / 1023.0f;
    EXPECT_NEAR(ColorTransferFunction::encode(0.0f, TransferFunction::Cineon),
                blackCode, 1.0e-6f);
    EXPECT_NEAR(ColorTransferFunction::encode(1.0f, TransferFunction::Cineon),
                whiteCode, 1.0e-6f);
    EXPECT_NEAR(ColorTransferFunction::decode(blackCode,
                                               TransferFunction::Cineon),
                0.0f, 1.0e-6f);
    EXPECT_NEAR(ColorTransferFunction::decode(whiteCode,
                                               TransferFunction::Cineon),
                1.0f, 1.0e-6f);
}

TEST(ImageIoContractTest, ExportsCineonThroughQImageToDpxAtRequestedBitDepths)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    QImage source(3, 1, QImage::Format_RGBA8888);
    ASSERT_FALSE(source.isNull());
    source.setPixelColor(0, 0, QColor(0, 0, 0, 255));
    source.setPixelColor(1, 0, QColor(128, 128, 128, 128));
    source.setPixelColor(2, 0, QColor(255, 255, 255, 64));

    ImageExporter exporter;
    for (const int bitsPerSample : {10, 12, 16}) {
        SCOPED_TRACE(bitsPerSample);
        const QString imagePath = temporaryDirectory.filePath(
            QStringLiteral("cineon-qimage-%1.dpx").arg(bitsPerSample));
        ImageExportOptions options;
        options.dataType = OIIO::TypeDesc::UINT16;
        options.bitsPerSample = bitsPerSample;
        options.inputTransferFunction = QStringLiteral("linear");
        options.outputTransferFunction = QStringLiteral("cineon");

        const ImageExportResult writeResult =
            exporter.write(source, imagePath, options);
        ASSERT_TRUE(writeResult.success) << writeResult.errorStage.toStdString()
                                         << ": " << writeResult.errorMessage.toStdString();

        OIIO::ImageBuf encoded(imagePath.toUtf8().constData());
        ASSERT_TRUE(encoded.read(0, 0, true, OIIO::TypeDesc::FLOAT));
        EXPECT_EQ(encoded.spec().width, 3);
        EXPECT_EQ(encoded.spec().height, 1);
        EXPECT_EQ(encoded.spec().nchannels, 4);
        EXPECT_EQ(encoded.spec().get_int_attribute("oiio:BitsPerSample", 0),
                  bitsPerSample);
        EXPECT_EQ(encoded.spec().get_string_attribute("dpx:Transfer", ""),
                  "Logarithmic");

        std::array<float, 12> pixels{};
        ASSERT_TRUE(encoded.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::FLOAT,
                                       pixels.data()));
        const float maxCode = static_cast<float>((1u << bitsPerSample) - 1u);
        const float maxError = 1.5f / maxCode;
        EXPECT_NEAR(pixels[0], 95.0f / 1023.0f, maxError);
        EXPECT_NEAR(pixels[4],
                    (685.0f + 300.0f * std::log10(
                        (128.0f / 255.0f) * (1.0f - 0.0107977516232771f) +
                        0.0107977516232771f)) / 1023.0f,
                    maxError);
        EXPECT_NEAR(pixels[8], 685.0f / 1023.0f, maxError);
        EXPECT_NEAR(pixels[3], 1.0f, maxError);
        EXPECT_NEAR(pixels[7], 128.0f / 255.0f, maxError);
        EXPECT_NEAR(pixels[11], 64.0f / 255.0f, maxError);

        ImageImporter importer;
        ASSERT_TRUE(importer.open(imagePath));
        const RawImage imported = importer.readImage();
        ASSERT_TRUE(imported.isValid());
        EXPECT_EQ(imported.width, 3);
        EXPECT_EQ(imported.height, 1);
        EXPECT_EQ(imported.channels, 4);
        EXPECT_EQ(imported.bitsPerChannel, bitsPerSample);
        EXPECT_EQ(imported.colorSpace, QStringLiteral("KodakLog"));
        EXPECT_EQ(imported.transferFunction, QStringLiteral("cineon"));
        EXPECT_TRUE(imported.isLogEncoded);
        ASSERT_EQ(imported.data.size(), static_cast<qsizetype>(sizeof(pixels)));
        std::array<float, 12> importedPixels{};
        std::memcpy(importedPixels.data(), imported.data.constData(), sizeof(importedPixels));
        EXPECT_NEAR(importedPixels[0], pixels[0], maxError);
        EXPECT_NEAR(importedPixels[7], pixels[7], maxError);
        EXPECT_NEAR(importedPixels[11], pixels[11], maxError);
    }
}

TEST(ImageIoContractTest, ExportsEveryNamedTransferFunction)
{
    struct TransferCase {
        const char* name;
        TransferFunction transfer;
    };
    constexpr std::array transferCases{
        TransferCase{"linear", TransferFunction::Linear},
        TransferCase{"srgb", TransferFunction::sRGB},
        TransferCase{"gamma22", TransferFunction::Gamma22},
        TransferCase{"gamma24", TransferFunction::Gamma24},
        TransferCase{"gamma26", TransferFunction::Gamma26},
        TransferCase{"rec709", TransferFunction::Rec709},
        TransferCase{"rec2020", TransferFunction::Rec2020_10},
        TransferCase{"rec2020_10", TransferFunction::Rec2020_10},
        TransferCase{"pq", TransferFunction::Rec2084_PQ},
        TransferCase{"st2084", TransferFunction::Rec2084_PQ},
        TransferCase{"hlg", TransferFunction::HLG},
        TransferCase{"acescc", TransferFunction::ACEScc},
        TransferCase{"acescct", TransferFunction::ACEScct},
        TransferCase{"davinciintermediate", TransferFunction::DaVinciIntermediate},
        TransferCase{"davinci_intermediate", TransferFunction::DaVinciIntermediate},
        TransferCase{"slog3", TransferFunction::SonySLog3},
        TransferCase{"sony_slog3", TransferFunction::SonySLog3},
        TransferCase{"cineon", TransferFunction::Cineon},
        TransferCase{"kodaklog", TransferFunction::Cineon},
        TransferCase{"cineonlog", TransferFunction::Cineon},
        TransferCase{"canonlog2", TransferFunction::CanonLog2},
        TransferCase{"canon_log2", TransferFunction::CanonLog2},
        TransferCase{"canonlog3", TransferFunction::CanonLog3},
        TransferCase{"canon_log3", TransferFunction::CanonLog3},
    };

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const OIIO::ImageSpec sourceSpec(1, 1, 3, OIIO::TypeDesc::FLOAT);
    OIIO::ImageBuf source(sourceSpec);
    const std::array<float, 3> sourcePixel{0.18f, 0.18f, 0.18f};
    ASSERT_TRUE(source.set_pixels(OIIO::ROI::All(), OIIO::TypeDesc::FLOAT,
                                  sourcePixel.data()));

    ImageExporter exporter;
    for (std::size_t i = 0; i < transferCases.size(); ++i) {
        const TransferCase& testCase = transferCases[i];
        SCOPED_TRACE(testCase.name);
        const QString outputPath = temporaryDirectory.filePath(
            QStringLiteral("transfer-%1.exr").arg(i));
        ImageExportOptions options;
        options.dataType = OIIO::TypeDesc::FLOAT;
        options.inputTransferFunction = QStringLiteral("linear");
        options.outputTransferFunction = QString::fromLatin1(testCase.name);
        const ImageExportResult writeResult =
            exporter.write(source, outputPath, options);
        ASSERT_TRUE(writeResult.success)
            << writeResult.errorStage.toStdString() << ": "
            << writeResult.errorMessage.toStdString();

        OIIO::ImageBuf encoded(outputPath.toUtf8().constData());
        ASSERT_TRUE(encoded.read(0, 0, true, OIIO::TypeDesc::FLOAT));
        std::array<float, 3> encodedPixel{};
        ASSERT_TRUE(encoded.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::FLOAT,
                                       encodedPixel.data()));
        const float expected = ColorTransferFunction::encode(
            sourcePixel[0], testCase.transfer);
        EXPECT_NEAR(encodedPixel[0], expected, 1.0e-5f);
        EXPECT_NEAR(encodedPixel[1], expected, 1.0e-5f);
        EXPECT_NEAR(encodedPixel[2], expected, 1.0e-5f);
        const OIIO::ParamValue* transfer = encoded.spec().find_attribute(
            "TransferFunction", OIIO::TypeDesc::STRING);
        ASSERT_NE(transfer, nullptr);
        EXPECT_EQ(transfer->get_string(), testCase.name);
    }
}

TEST(ImageIoContractTest, RejectsMissingOrUnknownInputTransferFunction)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString outputPath = temporaryDirectory.filePath(
        QStringLiteral("invalid-transfer.exr"));

    const OIIO::ImageSpec sourceSpec(1, 1, 3, OIIO::TypeDesc::FLOAT);
    OIIO::ImageBuf source(sourceSpec);
    const std::array<float, 3> sourcePixel{0.18f, 0.18f, 0.18f};
    ASSERT_TRUE(source.set_pixels(OIIO::ROI::All(), OIIO::TypeDesc::FLOAT,
                                  sourcePixel.data()));

    ImageExporter exporter;
    ImageExportOptions options;
    options.outputTransferFunction = QStringLiteral("cineon");
    const ImageExportResult missingInput =
        exporter.write(source, outputPath, options);
    EXPECT_FALSE(missingInput.success);
    EXPECT_EQ(missingInput.errorStage, QStringLiteral("color.transfer"));

    options.inputTransferFunction = QStringLiteral("not-a-transfer");
    const ImageExportResult unknownInput =
        exporter.write(source, outputPath, options);
    EXPECT_FALSE(unknownInput.success);
    EXPECT_EQ(unknownInput.errorStage, QStringLiteral("color.transfer"));

    options.inputTransferFunction = QStringLiteral("linear");
    options.outputTransferFunction = QStringLiteral("unknown-output");
    const ImageExportResult unknownOutput =
        exporter.write(source, outputPath, options);
    EXPECT_FALSE(unknownOutput.success);
    EXPECT_EQ(unknownOutput.errorStage, QStringLiteral("color.transfer"));
}

TEST(ImageIoContractTest, FailedExportPreservesExistingOutput)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString outputPath = temporaryDirectory.filePath(
        QStringLiteral("existing-image.exr"));
    const QByteArray originalContents("preserve-existing-deliverable");
    QFile existingOutput(outputPath);
    ASSERT_TRUE(existingOutput.open(QIODevice::WriteOnly));
    ASSERT_EQ(existingOutput.write(originalContents), originalContents.size());
    existingOutput.close();

    QImage source(1, 1, QImage::Format_RGBA8888);
    source.fill(QColor(64, 128, 192, 255));
    ImageExportOptions options;
    options.inputTransferFunction = QStringLiteral("linear");
    options.outputTransferFunction = QStringLiteral("unknown-transfer");

    ImageExporter exporter;
    const ImageExportResult result = exporter.write(source, outputPath, options);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorStage, QStringLiteral("color.transfer"));

    QFile preservedOutput(outputPath);
    ASSERT_TRUE(preservedOutput.open(QIODevice::ReadOnly));
    EXPECT_EQ(preservedOutput.readAll(), originalContents);
}

TEST(ImageIoContractTest, ImportsAnExportedImageWithoutStartingTheApplication)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString imagePath = temporaryDirectory.filePath(
        QStringLiteral("round-trip.png"));

    QImage source(2, 1, QImage::Format_RGBA8888);
    ASSERT_FALSE(source.isNull());
    source.setPixelColor(0, 0, QColor(32, 96, 160, 255));
    source.setPixelColor(1, 0, QColor(220, 180, 40, 128));

    ImageExporter exporter;
    const ImageExportResult writeResult =
        exporter.write(source, imagePath, ImageExportOptions{});
    ASSERT_TRUE(writeResult.success) << writeResult.errorStage.toStdString()
                                     << ": " << writeResult.errorMessage.toStdString();

    ImageImporter importer;
    ASSERT_TRUE(importer.open(imagePath));
    const RawImage decoded = importer.readImage();
    ASSERT_TRUE(decoded.isValid());
    EXPECT_EQ(decoded.width, 2);
    EXPECT_EQ(decoded.height, 1);
    EXPECT_EQ(decoded.channels, 4);
    ASSERT_EQ(decoded.pixelType, QStringLiteral("uint16"));
    ASSERT_EQ(decoded.data.size(), 16);
    std::array<quint16, 8> decodedPixels{};
    std::memcpy(decodedPixels.data(), decoded.data.constData(),
                static_cast<std::size_t>(decoded.data.size()));
    EXPECT_EQ(decodedPixels[0], 32u * 257u);
    EXPECT_EQ(decodedPixels[1], 96u * 257u);
    EXPECT_EQ(decodedPixels[2], 160u * 257u);
    EXPECT_EQ(decodedPixels[4], 220u * 257u);
    EXPECT_EQ(decodedPixels[5], 180u * 257u);
    EXPECT_EQ(decodedPixels[6], 40u * 257u);
    EXPECT_EQ(decodedPixels[7], 128u * 257u);
}

TEST(ImageIoContractTest, PreservesUnassociatedAlphaForImageBufPngExport)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString imagePath = temporaryDirectory.filePath(
        QStringLiteral("imagebuf-round-trip.png"));

    const OIIO::ImageSpec sourceSpec(1, 1, 4, OIIO::TypeDesc::FLOAT);
    OIIO::ImageBuf source(sourceSpec);
    const std::array<float, 4> sourcePixel{
        64.0f / 255.0f, 128.0f / 255.0f, 192.0f / 255.0f, 128.0f / 255.0f};
    ASSERT_TRUE(source.set_pixels(OIIO::ROI::All(), OIIO::TypeDesc::FLOAT,
                                  sourcePixel.data()));

    ImageExporter exporter;
    const ImageExportResult writeResult =
        exporter.write(source, imagePath, ImageExportOptions{});
    ASSERT_TRUE(writeResult.success) << writeResult.errorStage.toStdString()
                                     << ": " << writeResult.errorMessage.toStdString();

    ImageImporter importer;
    ASSERT_TRUE(importer.open(imagePath));
    const RawImage decoded = importer.readImage();
    ASSERT_TRUE(decoded.isValid());
    ASSERT_EQ(decoded.pixelType, QStringLiteral("uint16"));
    ASSERT_EQ(decoded.data.size(), 8);
    std::array<quint16, 4> decodedPixel{};
    std::memcpy(decodedPixel.data(), decoded.data.constData(),
                static_cast<std::size_t>(decoded.data.size()));
    EXPECT_EQ(decodedPixel[0], 64u * 257u);
    EXPECT_EQ(decodedPixel[1], 128u * 257u);
    EXPECT_EQ(decodedPixel[2], 192u * 257u);
    EXPECT_EQ(decodedPixel[3], 128u * 257u);
}

TEST(ImageIoContractTest, RejectsMissingInputFile)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    ImageImporter importer;
    EXPECT_FALSE(importer.open(
        temporaryDirectory.filePath(QStringLiteral("missing.png"))));
    EXPECT_FALSE(importer.readImage().isValid());
}

} // namespace
