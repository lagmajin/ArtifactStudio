#include <gtest/gtest.h>

#include <cstddef>
#include <string_view>

import Light.IESProfile;

namespace {

using ArtifactCore::IESLutTexture;
using ArtifactCore::IESParser;
using ArtifactCore::IESProfile;

constexpr std::string_view kTwoPlaneProfile =
    "IESNA:LM-63-2002\n"
    "TEST=contract\n"
    "TESTLAB\n"
    "ISSUEDATE\n"
    "MANUFAC\n"
    "LUMINAIRE\n"
    "1000 1 3 2 1 2 0 0 0 1 100\n"
    "0 90 180\n"
    "0 90\n"
    "10 20 30\n"
    "30 40 50\n";

constexpr std::string_view kSinglePlaneProfile =
    "IESNA:LM-63-2002\n"
    "TEST=single-plane\n"
    "TESTLAB\n"
    "ISSUEDATE\n"
    "MANUFAC\n"
    "LUMINAIRE\n"
    "1000 1 2 1 1 2 0 0 0 1 100\n"
    "0 180\n"
    "0\n"
    "0 100\n";

constexpr std::string_view kZeroCandelaProfile =
    "IESNA:LM-63-2002\n"
    "TEST=zero-candela\n"
    "TESTLAB\n"
    "ISSUEDATE\n"
    "MANUFAC\n"
    "LUMINAIRE\n"
    "1000 1 2 1 1 2 0 0 0 1 100\n"
    "0 180\n"
    "0\n"
    "0 0\n";

} // namespace

TEST(IESProfileContractTest, ParsesCandelaGridAndBilinearlySamplesClampedAngles)
{
    IESParser parser;
    const IESProfile profile = parser.parse(kTwoPlaneProfile);

    ASSERT_TRUE(profile.isValid());
    ASSERT_EQ(profile.verticalAngles.size(), 3u);
    ASSERT_EQ(profile.horizontalAngles.size(), 2u);
    ASSERT_EQ(profile.candelaValues.size(), 2u);
    EXPECT_FLOAT_EQ(profile.maxCandela, 50.0f);
    EXPECT_FLOAT_EQ(profile.sample(45.0f, 45.0f), 25.0f);
    EXPECT_FLOAT_EQ(profile.sample(-10.0f, 0.0f), 10.0f);
    EXPECT_FLOAT_EQ(profile.sample(200.0f, 0.0f), 30.0f);
}

TEST(IESProfileContractTest, ParsesWindowsCrLfProfileWithoutChangingCandelaSamples)
{
    std::string crlfProfile;
    crlfProfile.reserve(kTwoPlaneProfile.size() + 16);
    for (const char character : kTwoPlaneProfile) {
        if (character == '\n') crlfProfile += "\r\n";
        else crlfProfile += character;
    }

    IESParser parser;
    const IESProfile profile = parser.parse(crlfProfile);

    ASSERT_TRUE(profile.isValid());
    ASSERT_EQ(profile.verticalAngles.size(), 3u);
    ASSERT_EQ(profile.horizontalAngles.size(), 2u);
    EXPECT_FLOAT_EQ(profile.maxCandela, 50.0f);
    EXPECT_FLOAT_EQ(profile.sample(45.0f, 45.0f), 25.0f);
}

TEST(IESProfileContractTest, BuildsNormalizedRgbaLutForSingleHorizontalPlane)
{
    IESParser parser;
    const IESProfile profile = parser.parse(kSinglePlaneProfile);
    ASSERT_TRUE(profile.isValid());

    IESLutTexture texture;
    texture.width = 2;
    texture.height = 2;
    texture.buildFromProfile(profile);

    ASSERT_EQ(texture.data.size(), 2u * 2u * 4u);
    EXPECT_FLOAT_EQ(texture.data[0], 0.25f);
    EXPECT_FLOAT_EQ(texture.data[1], 0.25f);
    EXPECT_FLOAT_EQ(texture.data[2], 0.25f);
    EXPECT_FLOAT_EQ(texture.data[3], 1.0f);
    const std::size_t upperRow = 2u * 4u;
    EXPECT_FLOAT_EQ(texture.data[upperRow], 0.75f);
    EXPECT_FLOAT_EQ(texture.data[upperRow + 1], 0.75f);
    EXPECT_FLOAT_EQ(texture.data[upperRow + 2], 0.75f);
    EXPECT_FLOAT_EQ(texture.data[upperRow + 3], 1.0f);
}

TEST(IESProfileContractTest, ZeroCandelaProfileBuildsFiniteBlackLut)
{
    IESParser parser;
    const IESProfile profile = parser.parse(kZeroCandelaProfile);
    ASSERT_TRUE(profile.isValid());
    EXPECT_FLOAT_EQ(profile.maxCandela, 0.0f);
    EXPECT_FLOAT_EQ(profile.sample(90.0f, 180.0f), 0.0f);

    IESLutTexture texture;
    texture.width = 2;
    texture.height = 1;
    texture.buildFromProfile(profile);

    ASSERT_EQ(texture.data.size(), 8u);
    for (std::size_t pixel = 0; pixel < 2; ++pixel) {
        EXPECT_FLOAT_EQ(texture.data[pixel * 4], 0.0f);
        EXPECT_FLOAT_EQ(texture.data[pixel * 4 + 1], 0.0f);
        EXPECT_FLOAT_EQ(texture.data[pixel * 4 + 2], 0.0f);
        EXPECT_FLOAT_EQ(texture.data[pixel * 4 + 3], 1.0f);
    }
}

TEST(IESProfileContractTest, InvalidProfileSamplesBlackAndDoesNotReplaceExistingLut)
{
    IESParser parser;
    const IESProfile invalid = parser.parse("not an IES profile\n");
    EXPECT_FALSE(invalid.isValid());
    EXPECT_FLOAT_EQ(invalid.sample(45.0f, 90.0f), 0.0f);

    IESLutTexture texture;
    texture.width = 1;
    texture.height = 1;
    texture.data = {0.25f, 0.5f, 0.75f, 1.0f};
    texture.buildFromProfile(invalid);
    ASSERT_EQ(texture.data.size(), 4u);
    EXPECT_FLOAT_EQ(texture.data[0], 0.25f);
    EXPECT_FLOAT_EQ(texture.data[1], 0.5f);
    EXPECT_FLOAT_EQ(texture.data[2], 0.75f);
    EXPECT_FLOAT_EQ(texture.data[3], 1.0f);
}
