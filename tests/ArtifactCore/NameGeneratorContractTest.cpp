#include <gtest/gtest.h>

#include <string>
#include <type_traits>

import Core.ArtifactString;
import Utils.NameGenerator;

using namespace ArtifactCore;

static_assert(!std::is_copy_constructible_v<PatternNameGenerator>);
static_assert(!std::is_copy_assignable_v<PatternNameGenerator>);

TEST(NameGeneratorContractTest, ReplacesNameAndNumbersFirstSequence)
{
    PatternNameGenerator generator("(\\name)_***");
    EXPECT_EQ(generator.Generate("layer"), "layer_1");
    EXPECT_EQ(generator.Generate("layer"), "layer_2");
    EXPECT_EQ(generator.Generate("layer"), "layer_3");
}

TEST(NameGeneratorContractTest, ZeroPaddingUsesRequestedMinimumWidth)
{
    PatternNameGenerator generator("(\\name)_***", 3);
    EXPECT_EQ(generator.Generate("frame"), "frame_001");
    EXPECT_EQ(generator.Generate("frame"), "frame_002");
}

TEST(NameGeneratorContractTest, CountersAreIndependentForEachBaseName)
{
    PatternNameGenerator generator("(\\name)-***", 2);
    EXPECT_EQ(generator.Generate("left"), "left-01");
    EXPECT_EQ(generator.Generate("right"), "right-01");
    EXPECT_EQ(generator.Generate("left"), "left-02");
}

TEST(NameGeneratorContractTest, UsedNamesRemainUniqueAcrossDifferentBases)
{
    PatternNameGenerator generator("***", 3);
    EXPECT_EQ(generator.Generate("first"), "001");
    EXPECT_EQ(generator.Generate("second"), "002");
    EXPECT_EQ(generator.Generate("third"), "003");
}

TEST(NameGeneratorContractTest, ReleasingNameMakesItAvailableToAnotherBase)
{
    PatternNameGenerator generator("***", 2);
    const String first = generator.Generate("first");
    EXPECT_EQ(first, "01");
    generator.Release(first);
    EXPECT_EQ(generator.Generate("second"), "01");
}

TEST(NameGeneratorContractTest, ReclaimedNameIsReservedAgainstOtherBasesAgain)
{
    PatternNameGenerator generator("***", 2);
    const String first = generator.Generate("first");
    generator.Release(first);

    EXPECT_EQ(generator.Generate("second"), "01");
    EXPECT_EQ(generator.Generate("third"), "02");
}

TEST(NameGeneratorContractTest, ReleasingUnknownNameDoesNotDisturbCounters)
{
    PatternNameGenerator generator("(\\name)_***");
    generator.Release("missing_99");
    EXPECT_EQ(generator.Generate("item"), "item_1");
    EXPECT_EQ(generator.Generate("item"), "item_2");
}

TEST(NameGeneratorContractTest, ReleasingNameDoesNotRewindItsBaseCounter)
{
    PatternNameGenerator generator("(\\name)_***");
    const String first = generator.Generate("item");
    generator.Release(first);
    EXPECT_EQ(generator.Generate("item"), "item_2");
}

TEST(NameGeneratorContractTest, PaddingDoesNotTruncateNumbersBeyondWidth)
{
    PatternNameGenerator generator("***", 2);
    String last;
    for (int index = 0; index < 100; ++index) {
        last = generator.Generate("unused");
    }
    EXPECT_EQ(last, "100");
}

TEST(NameGeneratorContractTest, PaddingWidthBeyondInternalSmallBufferIsHonored)
{
    PatternNameGenerator generator("***", 40);
    const std::string generated = toStdString(generator.Generate("ignored"));

    ASSERT_EQ(generated.size(), 40u);
    EXPECT_EQ(generated.substr(0, 39), std::string(39, '0'));
    EXPECT_EQ(generated.back(), '1');
}

TEST(NameGeneratorContractTest, NonpositivePaddingWidthUsesUnpaddedNumbers)
{
    PatternNameGenerator generator("***", -4);
    EXPECT_EQ(generator.Generate("item"), "1");
    EXPECT_EQ(generator.Generate("item"), "2");
}

TEST(NameGeneratorContractTest, LiteralPatternWithoutNamePlaceholderKeepsOnlyNumberSlot)
{
    PatternNameGenerator generator("render-***.exr", 4);
    EXPECT_EQ(generator.Generate("ignored"), "render-0001.exr");
}

TEST(NameGeneratorContractTest, LiteralPatternKeepsGeneratedNamesUniqueAcrossBases)
{
    PatternNameGenerator generator("render-***.exr", 2);

    EXPECT_EQ(generator.Generate("first"), "render-01.exr");
    EXPECT_EQ(generator.Generate("second"), "render-02.exr");
}

TEST(NameGeneratorContractTest, EmptyBaseNameStillAdvancesNumberSlot)
{
    PatternNameGenerator generator("(\\name)_***", 2);
    EXPECT_EQ(generator.Generate(""), "_01");
    EXPECT_EQ(generator.Generate(""), "_02");
}

TEST(NameGeneratorContractTest, UnicodeBaseNameIsPreservedInGeneratedName)
{
    PatternNameGenerator generator("(\\name)-***", 2);
    EXPECT_EQ(generator.Generate("レイヤー"), "レイヤー-01");
    EXPECT_EQ(generator.Generate("レイヤー"), "レイヤー-02");
}

TEST(NameGeneratorContractTest, PlaceholderLikeTextInBaseNameIsPreserved)
{
    PatternNameGenerator generator("(\\name)_***", 2);

    EXPECT_EQ(generator.Generate("take***"), "take***_01");
}
