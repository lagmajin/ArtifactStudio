#include <gtest/gtest.h>

#include <QVector>
#include <QString>
#include <QByteArray>
#include <QMap>
#include <QtGlobal>

#include <utility>

import Image.Raw;
import Memory.SharedPtr;

using namespace ArtifactCore;

TEST(RawImageContractTest, ValidityRequiresPositiveShapeAndPopulatedDescriptionAndData)
{
    RawImage image;
    EXPECT_FALSE(image.isValid());

    image.width = 1;
    image.height = 1;
    image.channels = 1;
    image.pixelType = QStringLiteral("uint8");
    EXPECT_FALSE(image.isValid());

    image.data.push_back(0);
    EXPECT_TRUE(image.isValid());

    image.width = 0;
    EXPECT_FALSE(image.isValid());
    image.width = 1;
    image.height = -1;
    EXPECT_FALSE(image.isValid());
    image.height = 1;
    image.channels = 0;
    EXPECT_FALSE(image.isValid());
    image.channels = 1;
    image.pixelType.clear();
    EXPECT_FALSE(image.isValid());
    image.pixelType = QStringLiteral("uint8");
    image.data.clear();
    EXPECT_FALSE(image.isValid());
}

TEST(RawImageContractTest, PixelTypeByteWidthsMatchSupportedScalarTypes)
{
    struct PixelTypeCase {
        const char* name;
        int expectedBytes;
    };
    const PixelTypeCase cases[] = {
        {"uint8", 1}, {"int8", 1}, {"uint16", 2}, {"int16", 2},
        {"half", 2}, {"uint32", 4}, {"int32", 4}, {"float", 4},
        {"uint64", 8}, {"int64", 8}, {"double", 8}, {"unknown", 0},
    };

    RawImage image;
    for (const auto& testCase : cases) {
        image.pixelType = QString::fromLatin1(testCase.name);
        EXPECT_EQ(image.getPixelTypeSizeInBytes(), testCase.expectedBytes)
            << "pixel type: " << testCase.name;
    }
}

TEST(RawImageContractTest, RejectsUnknownPixelTypeDespitePopulatedShapeAndData)
{
    RawImage image;
    image.width = 1;
    image.height = 1;
    image.channels = 1;
    image.pixelType = QStringLiteral("unknown-format");
    image.data = {0};

    EXPECT_EQ(image.getPixelTypeSizeInBytes(), 0);
    EXPECT_FALSE(image.isValid());
}

TEST(RawImageContractTest, RejectsPixelBufferShorterThanDescribedImage)
{
    RawImage image;
    image.width = 1;
    image.height = 1;
    image.channels = 4;
    image.pixelType = QStringLiteral("float");
    image.data.fill(0, 15);

    EXPECT_EQ(image.getPixelTypeSizeInBytes(), 4);
    EXPECT_FALSE(image.isValid());
}

TEST(RawImageContractTest, CopyPreservesMetadataAndDetachesPixelStorageOnWrite)
{
    RawImage source;
    source.width = 1;
    source.height = 1;
    source.channels = 1;
    source.pixelType = QStringLiteral("uint8");
    source.colorSpace = QStringLiteral("Display P3");
    source.metadata.insert(QStringLiteral("camera"), QStringLiteral("test-body"));
    source.iccProfileData = QByteArray::fromHex("0102ff");
    source.data = {11};

    RawImage copied = source;
    copied.data[0] = 99;
    copied.metadata.insert(QStringLiteral("camera"), QStringLiteral("copy"));

    EXPECT_TRUE(copied.isValid());
    EXPECT_EQ(copied.colorSpace, QStringLiteral("Display P3"));
    EXPECT_EQ(copied.iccProfileData, QByteArray::fromHex("0102ff"));
    EXPECT_EQ(copied.data[0], 99);
    EXPECT_EQ(source.data[0], 11);
    EXPECT_EQ(source.metadata.value(QStringLiteral("camera")), QStringLiteral("test-body"));
}

TEST(RawImageContractTest, CopyAssignmentReplacesAllImageDescriptionFields)
{
    RawImage source;
    source.width = 2;
    source.height = 1;
    source.channels = 3;
    source.pixelType = QStringLiteral("half");
    source.transferFunction = QStringLiteral("linear");
    source.primaries = QStringLiteral("Rec.2020");
    source.chromaSubsampling = QStringLiteral("4:4:4");
    source.bitsPerChannel = 16;
    source.isHDR = true;
    source.isLogEncoded = true;
    source.data = {1, 2, 3, 4, 5, 6};

    RawImage assigned;
    assigned.width = 99;
    assigned.colorSpace = QStringLiteral("obsolete");
    assigned = source;

    EXPECT_EQ(assigned.width, 2);
    EXPECT_EQ(assigned.height, 1);
    EXPECT_EQ(assigned.channels, 3);
    EXPECT_EQ(assigned.pixelType, QStringLiteral("half"));
    EXPECT_EQ(assigned.transferFunction, QStringLiteral("linear"));
    EXPECT_EQ(assigned.primaries, QStringLiteral("Rec.2020"));
    EXPECT_EQ(assigned.chromaSubsampling, QStringLiteral("4:4:4"));
    EXPECT_EQ(assigned.bitsPerChannel, 16);
    EXPECT_TRUE(assigned.isHDR);
    EXPECT_TRUE(assigned.isLogEncoded);
    ASSERT_EQ(assigned.data.size(), source.data.size());
    for (qsizetype index = 0; index < source.data.size(); ++index) {
        EXPECT_EQ(assigned.data[index], source.data[index]);
    }
    EXPECT_TRUE(assigned.isValid());
}

TEST(RawImageContractTest, MoveConstructionPreservesPixelAndColorMetadata)
{
    RawImage source;
    source.width = 1;
    source.height = 2;
    source.channels = 4;
    source.pixelType = QStringLiteral("float");
    source.colorSpace = QStringLiteral("ACEScg");
    source.data = {1, 2, 3, 4};

    RawImage moved = std::move(source);

    EXPECT_TRUE(moved.isValid());
    EXPECT_EQ(moved.width, 1);
    EXPECT_EQ(moved.height, 2);
    EXPECT_EQ(moved.channels, 4);
    EXPECT_EQ(moved.pixelType, QStringLiteral("float"));
    EXPECT_EQ(moved.colorSpace, QStringLiteral("ACEScg"));
    EXPECT_EQ(moved.data.size(), 4);
}

TEST(RawImageContractTest, MoveAssignmentReplacesPreviousImageState)
{
    RawImage source;
    source.width = 3;
    source.height = 1;
    source.channels = 2;
    source.pixelType = QStringLiteral("uint16");
    source.colorSpace = QStringLiteral("linear-sRGB");
    source.iccProfileData = QByteArray::fromHex("ab00cd");
    source.data = {5, 8, 13, 21, 34, 55};

    RawImage destination;
    destination.width = 100;
    destination.colorSpace = QStringLiteral("stale");
    destination = std::move(source);

    EXPECT_TRUE(destination.isValid());
    EXPECT_EQ(destination.width, 3);
    EXPECT_EQ(destination.height, 1);
    EXPECT_EQ(destination.channels, 2);
    EXPECT_EQ(destination.pixelType, QStringLiteral("uint16"));
    EXPECT_EQ(destination.colorSpace, QStringLiteral("linear-sRGB"));
    EXPECT_EQ(destination.iccProfileData, QByteArray::fromHex("ab00cd"));
    EXPECT_EQ(destination.data.size(), 6);
    EXPECT_EQ(destination.data[5], 55);
}

TEST(RawImageContractTest, RawImagePointerAliasSharesAndReleasesOwnership)
{
    RawImagePtr first = makeShared<RawImage>();
    ASSERT_NE(first.get(), nullptr);
    first->width = 1;
    first->height = 1;
    first->channels = 1;
    first->pixelType = QStringLiteral("uint8");
    first->data = {42};

    RawImagePtr second = first;
    EXPECT_EQ(first.useCount(), 2);
    second->colorSpace = QStringLiteral("sRGB");
    EXPECT_EQ(first->colorSpace, QStringLiteral("sRGB"));
    EXPECT_TRUE(first->isValid());

    second.reset();
    EXPECT_EQ(first.useCount(), 1);
    EXPECT_NE(first.get(), nullptr);
}
