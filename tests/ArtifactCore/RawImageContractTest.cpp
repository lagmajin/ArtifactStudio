#include <gtest/gtest.h>

#include <QVector>
#include <QString>

import Image.Raw;

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
