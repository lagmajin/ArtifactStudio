#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>

import Asset.Sequence;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto ticks = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
        const auto base = std::filesystem::temp_directory_path();
        for (int attempt = 0; attempt < 16; ++attempt) {
            path_ = base / ("ArtifactSequenceStandalone_" +
                            std::to_string(ticks) + "_" +
                            std::to_string(attempt));
            std::error_code error;
            if (std::filesystem::create_directory(path_, error)) {
                valid_ = true;
                return;
            }
        }
    }

    ~TemporaryDirectory()
    {
        if (valid_) {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }
    }

    bool isValid() const { return valid_; }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
    bool valid_ = false;
};

bool writeFile(const std::filesystem::path& path, const char* contents)
{
    std::ofstream file(path, std::ios::binary);
    return file && static_cast<bool>(file << contents);
}

String frameName(int frame)
{
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "shot_%04d.exr", frame);
    return String(buffer);
}

std::array<bool, 8> selectedFrames(unsigned int mask)
{
    std::array<bool, 8> selected{};
    for (unsigned int frame = 0; frame < selected.size(); ++frame) {
        selected[frame] = (mask & (1u << frame)) != 0;
    }
    return selected;
}

std::vector<String> reverseOrderedFrameNames(const std::array<bool, 8>& selected)
{
    std::vector<String> result;
    for (int frame = 7; frame >= 0; --frame) {
        if (selected[static_cast<std::size_t>(frame)]) {
            result.emplace_back(frameName(frame));
        }
    }
    return result;
}

std::vector<String> names(std::initializer_list<const char*> values)
{
    std::vector<String> result;
    result.reserve(values.size());
    for (const char* value : values) {
        result.emplace_back(value);
    }
    return result;
}

} // namespace

TEST(AssetSequenceStandaloneTest, EmptyInputProducesNoSequencesOrSingles)
{
    const auto result = detectSequences({});

    EXPECT_TRUE(result.sequences.empty());
    EXPECT_TRUE(result.singles.empty());
}

TEST(AssetSequenceStandaloneTest, GroupsHaveDeterministicPrefixOrder)
{
    const auto result = detectSequences(names({
        "z_take_0002.exr", "a_take_0002.exr",
        "z_take_0001.exr", "a_take_0001.exr"}));

    ASSERT_EQ(result.sequences.size(), 2u);
    EXPECT_EQ(toStdString(result.sequences[0].prefix), "a_take_");
    EXPECT_EQ(toStdString(result.sequences[1].prefix), "z_take_");
    EXPECT_TRUE(result.singles.empty());
}

TEST(AssetSequenceStandaloneTest, ScrambledInputKeepsGroupsAndFramesInStableOrder)
{
    const auto result = detectSequences(names({
        "z_take_0003.exr", "a_take_0002.exr", "z_take_0001.exr",
        "a_take_0001.exr", "z_take_0002.exr"}));

    ASSERT_EQ(result.sequences.size(), 2u);
    EXPECT_EQ(toStdString(result.sequences[0].prefix), "a_take_");
    EXPECT_EQ(result.sequences[0].firstFrame, 1);
    EXPECT_EQ(result.sequences[0].lastFrame, 2);
    ASSERT_EQ(result.sequences[0].filenames.size(), 2u);
    EXPECT_EQ(toStdString(result.sequences[0].filenames[0]), "a_take_0001.exr");
    EXPECT_EQ(toStdString(result.sequences[0].filenames[1]), "a_take_0002.exr");

    EXPECT_EQ(toStdString(result.sequences[1].prefix), "z_take_");
    ASSERT_EQ(result.sequences[1].filenames.size(), 3u);
    EXPECT_EQ(toStdString(result.sequences[1].filenames[0]), "z_take_0001.exr");
    EXPECT_EQ(toStdString(result.sequences[1].filenames[1]), "z_take_0002.exr");
    EXPECT_EQ(toStdString(result.sequences[1].filenames[2]), "z_take_0003.exr");
    EXPECT_TRUE(result.singles.empty());
}

TEST(AssetSequenceStandaloneTest, ParsesTheFinalDigitRunAndKeepsEarlierDigitsInPrefix)
{
    const auto result = detectSequences(names({
        "version012_take_0002.exr", "version012_take_0001.exr"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(toStdString(sequence.prefix), "version012_take_");
    EXPECT_EQ(sequence.firstFrame, 1);
    EXPECT_EQ(sequence.lastFrame, 2);
    EXPECT_EQ(sequence.padding, 4);
}

TEST(AssetSequenceStandaloneTest, GroupConvenienceFieldsMatchSortedSequenceMetadata)
{
    const auto result = detectSequences(names({
        "shot_0002.exr", "shot_0001.exr"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(toStdString(sequence.representative()), "shot_0001.exr");
    EXPECT_EQ(toStdString(sequence.pathPattern()), "shot_%04lld.exr");
    EXPECT_EQ(toStdString(sequence.displayName()),
              "shot_[0001-0002].exr  (2 frames)");
    EXPECT_TRUE(sequence.missingFrames.empty());
}

TEST(AssetSequenceStandaloneTest, DetectsSequenceStartingAtFrameZero)
{
    const auto result = detectSequences(names({"shot_0000.exr", "shot_0001.exr"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(sequence.firstFrame, 0);
    EXPECT_EQ(sequence.lastFrame, 1);
    EXPECT_EQ(toStdString(sequence.representative()), "shot_0000.exr");
    EXPECT_EQ(toStdString(sequence.pathPattern()), "shot_%04lld.exr");
    EXPECT_TRUE(sequence.missingFrames.empty());
}

TEST(AssetSequenceStandaloneTest, PreservesUppercaseAlphanumericExtension)
{
    const auto result = detectSequences(names({"take_001.EXR", "take_002.EXR"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(toStdString(sequence.suffix), ".EXR");
    EXPECT_EQ(toStdString(sequence.representative()), "take_001.EXR");
    EXPECT_EQ(toStdString(sequence.pathPattern()), "take_%03lld.EXR");
    EXPECT_TRUE(result.singles.empty());
}

TEST(AssetSequenceStandaloneTest, PreservesUnicodePrefixAndFilenames)
{
    const auto result = detectSequences(names({
        "素材_0002.exr", "素材_0001.exr"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(toStdString(sequence.prefix), "素材_");
    EXPECT_EQ(sequence.firstFrame, 1);
    EXPECT_EQ(sequence.lastFrame, 2);
    ASSERT_EQ(sequence.filenames.size(), 2u);
    EXPECT_EQ(toStdString(sequence.filenames.front()), "素材_0001.exr");
    EXPECT_EQ(toStdString(sequence.filenames.back()), "素材_0002.exr");
    EXPECT_TRUE(result.singles.empty());
}

TEST(AssetSequenceStandaloneTest, SplitsAcrossVeryLargeFrameNumberGaps)
{
    const auto result = detectSequences(names({
        "shot_000000000000000001.exr",
        "shot_999999999999999999.exr"}));

    EXPECT_TRUE(result.sequences.empty());
    ASSERT_EQ(result.singles.size(), 2u);
    EXPECT_EQ(toStdString(result.singles[0]), "shot_000000000000000001.exr");
    EXPECT_EQ(toStdString(result.singles[1]), "shot_999999999999999999.exr");
}

TEST(AssetSequenceStandaloneTest, PreserveGroupConvenienceFieldsIncludeOnlyPresentFiles)
{
    const auto result = detectSequences(
        names({"take_0004.png", "take_0001.png", "take_0002.png"}),
        2, MissingFramePolicy::Preserve);

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(toStdString(sequence.representative()), "take_0001.png");
    EXPECT_EQ(toStdString(sequence.pathPattern()), "take_%04lld.png");
    EXPECT_EQ(toStdString(sequence.displayName()),
              "take_[0001-0004].png  (3 frames)");
    EXPECT_EQ(sequence.missingFrames, (std::vector<int64_t>{3}));
}

TEST(AssetSequenceStandaloneTest, SeparatesGroupsBySuffixAndFramePadding)
{
    const auto result = detectSequences(names({
        "take_01.png", "take_02.png", "take_001.png", "take_002.png",
        "take_01.exr", "take_02.exr"}));

    ASSERT_EQ(result.sequences.size(), 3u);
    EXPECT_EQ(toStdString(result.sequences[0].suffix), ".exr");
    EXPECT_EQ(result.sequences[0].padding, 2);
    EXPECT_EQ(toStdString(result.sequences[1].suffix), ".png");
    EXPECT_EQ(result.sequences[1].padding, 2);
    EXPECT_EQ(toStdString(result.sequences[2].suffix), ".png");
    EXPECT_EQ(result.sequences[2].padding, 3);
    EXPECT_TRUE(result.singles.empty());
}

TEST(AssetSequenceStandaloneTest, SplitPolicyPartitionsEveryConsecutiveRun)
{
    const auto result = detectSequences(names({
        "take_0008.exr", "take_0001.exr", "take_0007.exr",
        "take_0004.exr", "take_0002.exr", "take_0010.exr"}));

    ASSERT_EQ(result.sequences.size(), 2u);
    EXPECT_EQ(result.sequences[0].firstFrame, 1);
    EXPECT_EQ(result.sequences[0].lastFrame, 2);
    ASSERT_EQ(result.sequences[0].filenames.size(), 2u);
    EXPECT_EQ(toStdString(result.sequences[0].filenames[0]), "take_0001.exr");
    EXPECT_EQ(toStdString(result.sequences[0].filenames[1]), "take_0002.exr");
    EXPECT_EQ(result.sequences[1].firstFrame, 7);
    EXPECT_EQ(result.sequences[1].lastFrame, 8);
    ASSERT_EQ(result.singles.size(), 2u);
    EXPECT_EQ(toStdString(result.singles[0]), "take_0004.exr");
    EXPECT_EQ(toStdString(result.singles[1]), "take_0010.exr");
}

TEST(AssetSequenceStandaloneTest, PreservePolicyListsEveryMissingFrameAcrossMultipleGaps)
{
    const auto result = detectSequences(
        names({"render_0001.png", "render_0004.png", "render_0007.png"}),
        2, MissingFramePolicy::Preserve);

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(sequence.firstFrame, 1);
    EXPECT_EQ(sequence.lastFrame, 7);
    EXPECT_EQ(sequence.missingFrames,
              (std::vector<int64_t>{2, 3, 5, 6}));
    ASSERT_EQ(sequence.filenames.size(), 3u);
}

TEST(AssetSequenceStandaloneTest, MinimumRunLengthDemotesShortRunsAndClampsToTwo)
{
    const auto result = detectSequences(
        names({"shot_0001.png", "shot_0002.png", "shot_0005.png"}),
        0);

    ASSERT_EQ(result.sequences.size(), 1u);
    EXPECT_EQ(result.sequences.front().firstFrame, 1);
    EXPECT_EQ(result.sequences.front().lastFrame, 2);
    ASSERT_EQ(result.singles.size(), 1u);
    EXPECT_EQ(toStdString(result.singles.front()), "shot_0005.png");
}

TEST(AssetSequenceStandaloneTest, MinimumRunLengthAppliesAfterGapSplitting)
{
    const auto result = detectSequences(
        names({"shot_0001.png", "shot_0002.png",
               "shot_0004.png", "shot_0005.png"}),
        3);

    EXPECT_TRUE(result.sequences.empty());
    ASSERT_EQ(result.singles.size(), 4u);
    EXPECT_EQ(toStdString(result.singles[0]), "shot_0001.png");
    EXPECT_EQ(toStdString(result.singles[1]), "shot_0002.png");
    EXPECT_EQ(toStdString(result.singles[2]), "shot_0004.png");
    EXPECT_EQ(toStdString(result.singles[3]), "shot_0005.png");
}

TEST(AssetSequenceStandaloneTest, PreservePolicyStillDemotesBucketsBelowMinimum)
{
    const auto result = detectSequences(
        names({"shot_0001.png", "shot_0005.png"}),
        3, MissingFramePolicy::Preserve);

    EXPECT_TRUE(result.sequences.empty());
    ASSERT_EQ(result.singles.size(), 2u);
    EXPECT_EQ(toStdString(result.singles[0]), "shot_0001.png");
    EXPECT_EQ(toStdString(result.singles[1]), "shot_0005.png");
}

TEST(AssetSequenceStandaloneTest, ParsesUppercaseAsciiExtensionsWithoutChangingSuffix)
{
    const auto result = detectSequences(names({"shot_01.PNG", "shot_02.PNG"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    EXPECT_EQ(toStdString(result.sequences.front().suffix), ".PNG");
    EXPECT_EQ(result.sequences.front().padding, 2);
    EXPECT_TRUE(result.singles.empty());
}

TEST(AssetSequenceStandaloneTest, AllowsEmptyPrefixButRequiresFinalExtension)
{
    const auto result = detectSequences(names({
        "0001.png", "0002.png", "shot_0001", "shot_0002"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    EXPECT_TRUE(result.sequences.front().prefix.isEmpty());
    EXPECT_EQ(result.sequences.front().firstFrame, 1);
    EXPECT_EQ(result.sequences.front().lastFrame, 2);
    ASSERT_EQ(result.singles.size(), 2u);
    EXPECT_EQ(toStdString(result.singles[0]), "shot_0001");
    EXPECT_EQ(toStdString(result.singles[1]), "shot_0002");
}

TEST(AssetSequenceStandaloneTest, AcceptsAdjacentMaximumLengthFrameTokens)
{
    const auto result = detectSequences(names({
        "shot_999999999999999999.exr",
        "shot_999999999999999998.exr"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(sequence.padding, 18);
    EXPECT_EQ(sequence.firstFrame, 999999999999999998LL);
    EXPECT_EQ(sequence.lastFrame, 999999999999999999LL);
    ASSERT_EQ(sequence.filenames.size(), 2u);
    EXPECT_EQ(toStdString(sequence.filenames[0]),
              "shot_999999999999999998.exr");
    EXPECT_EQ(toStdString(sequence.filenames[1]),
              "shot_999999999999999999.exr");
}

TEST(AssetSequenceStandaloneTest, PreservesEighteenDigitPaddingForZeroBasedFrames)
{
    const auto result = detectSequences(names({
        "shot_000000000000000001.exr",
        "shot_000000000000000000.exr"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(sequence.padding, 18);
    EXPECT_EQ(sequence.firstFrame, 0);
    EXPECT_EQ(sequence.lastFrame, 1);
    EXPECT_EQ(toStdString(sequence.filenames[0]),
              "shot_000000000000000000.exr");
    EXPECT_EQ(toStdString(sequence.filenames[1]),
              "shot_000000000000000001.exr");
}

TEST(AssetSequenceStandaloneTest, FormatsConvenienceFieldsAtMaximumFramePadding)
{
    const auto result = detectSequences(names({
        "shot_999999999999999999.exr",
        "shot_999999999999999998.exr"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(toStdString(sequence.pathPattern()), "shot_%018lld.exr");
    EXPECT_EQ(toStdString(sequence.displayName()),
              "shot_[999999999999999998-999999999999999999].exr  (2 frames)");
}

TEST(AssetSequenceStandaloneTest, RejectsNineteenDigitTokensEvenWhenValueFitsInt64)
{
    const auto result = detectSequences(names({
        "shot_1000000000000000000.exr",
        "shot_1000000000000000001.exr"}));

    EXPECT_TRUE(result.sequences.empty());
    ASSERT_EQ(result.singles.size(), 2u);
    EXPECT_EQ(toStdString(result.singles[0]), "shot_1000000000000000000.exr");
    EXPECT_EQ(toStdString(result.singles[1]), "shot_1000000000000000001.exr");
}

TEST(AssetSequenceStandaloneTest, KeepsDigitsInExtensionOutOfTheFrameToken)
{
    const auto result = detectSequences(names({"shot_02.exr2", "shot_01.exr2"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(toStdString(sequence.prefix), "shot_");
    EXPECT_EQ(toStdString(sequence.suffix), ".exr2");
    EXPECT_EQ(sequence.firstFrame, 1);
    EXPECT_EQ(sequence.lastFrame, 2);
}

TEST(AssetSequenceStandaloneTest, SplitPolicyClassifiesEveryEightFramePresencePattern)
{
    constexpr unsigned int patternCount = 1u << 8u;
    for (unsigned int mask = 0; mask < patternCount; ++mask) {
        const auto selected = selectedFrames(mask);
        const auto result = detectSequences(reverseOrderedFrameNames(selected));
        std::vector<std::vector<int>> expectedRuns;
        std::vector<int> expectedSingles;

        for (int frame = 0; frame < 8;) {
            if (!selected[static_cast<std::size_t>(frame)]) {
                ++frame;
                continue;
            }
            const int first = frame;
            while (frame < 8 && selected[static_cast<std::size_t>(frame)]) {
                ++frame;
            }
            if (frame - first >= 2) {
                expectedRuns.emplace_back();
                for (int member = first; member < frame; ++member) {
                    expectedRuns.back().push_back(member);
                }
            } else {
                expectedSingles.push_back(first);
            }
        }

        SCOPED_TRACE(::testing::Message() << "mask=" << mask);
        ASSERT_EQ(result.sequences.size(), expectedRuns.size());
        ASSERT_EQ(result.singles.size(), expectedSingles.size());
        for (std::size_t run = 0; run < expectedRuns.size(); ++run) {
            const auto& actual = result.sequences[run];
            const auto& expected = expectedRuns[run];
            ASSERT_EQ(actual.filenames.size(), expected.size());
            EXPECT_EQ(actual.firstFrame, expected.front());
            EXPECT_EQ(actual.lastFrame, expected.back());
            for (std::size_t member = 0; member < expected.size(); ++member) {
                EXPECT_EQ(actual.filenames[member], frameName(expected[member]));
            }
        }
        for (std::size_t index = 0; index < expectedSingles.size(); ++index) {
            EXPECT_EQ(result.singles[index], frameName(expectedSingles[index]));
        }
    }
}

TEST(AssetSequenceStandaloneTest, PreservePolicyClassifiesEveryEightFramePresencePattern)
{
    constexpr unsigned int patternCount = 1u << 8u;
    for (unsigned int mask = 0; mask < patternCount; ++mask) {
        const auto selected = selectedFrames(mask);
        const auto result = detectSequences(
            reverseOrderedFrameNames(selected), 2, MissingFramePolicy::Preserve);
        std::vector<int> expectedFrames;
        std::vector<int64_t> expectedMissing;
        for (int frame = 0; frame < 8; ++frame) {
            if (selected[static_cast<std::size_t>(frame)]) {
                expectedFrames.push_back(frame);
            }
        }

        SCOPED_TRACE(::testing::Message() << "mask=" << mask);
        if (expectedFrames.size() < 2) {
            EXPECT_TRUE(result.sequences.empty());
            ASSERT_EQ(result.singles.size(), expectedFrames.size());
            for (std::size_t index = 0; index < expectedFrames.size(); ++index) {
                EXPECT_EQ(result.singles[index], frameName(expectedFrames[index]));
            }
            continue;
        }

        ASSERT_EQ(result.sequences.size(), 1u);
        EXPECT_TRUE(result.singles.empty());
        const auto& actual = result.sequences.front();
        ASSERT_EQ(actual.filenames.size(), expectedFrames.size());
        EXPECT_EQ(actual.firstFrame, expectedFrames.front());
        EXPECT_EQ(actual.lastFrame, expectedFrames.back());
        for (std::size_t index = 0; index < expectedFrames.size(); ++index) {
            EXPECT_EQ(actual.filenames[index], frameName(expectedFrames[index]));
        }
        for (int frame = expectedFrames.front() + 1;
             frame < expectedFrames.back(); ++frame) {
            if (!selected[static_cast<std::size_t>(frame)]) {
                expectedMissing.push_back(frame);
            }
        }
        EXPECT_EQ(actual.missingFrames, expectedMissing);
    }
}

TEST(AssetSequenceStandaloneTest, EveryPresencePatternRespectsAllMinimumThresholds)
{
    constexpr unsigned int patternCount = 1u << 8u;
    for (int requestedMinimum = 1; requestedMinimum <= 9; ++requestedMinimum) {
        const int minimum = std::max(2, requestedMinimum);
        for (unsigned int mask = 0; mask < patternCount; ++mask) {
            const auto selected = selectedFrames(mask);
            std::vector<int> selectedIndices;
            for (int frame = 0; frame < 8; ++frame) {
                if (selected[static_cast<std::size_t>(frame)]) {
                    selectedIndices.push_back(frame);
                }
            }

            for (const auto policy : {MissingFramePolicy::Split,
                                      MissingFramePolicy::Preserve}) {
                const auto actual = detectSequences(
                    reverseOrderedFrameNames(selected), minimum, policy);
                std::vector<std::vector<int>> expectedGroups;
                std::vector<int> expectedSingles;

                if (policy == MissingFramePolicy::Preserve) {
                    if (static_cast<int>(selectedIndices.size()) >= minimum) {
                        expectedGroups.push_back(selectedIndices);
                    } else {
                        expectedSingles = selectedIndices;
                    }
                } else {
                    for (int frame = 0; frame < 8;) {
                        if (!selected[static_cast<std::size_t>(frame)]) {
                            ++frame;
                            continue;
                        }
                        const int first = frame;
                        while (frame < 8 && selected[static_cast<std::size_t>(frame)]) {
                            ++frame;
                        }
                        if (frame - first >= minimum) {
                            expectedGroups.emplace_back();
                            for (int member = first; member < frame; ++member) {
                                expectedGroups.back().push_back(member);
                            }
                        } else {
                            for (int member = first; member < frame; ++member) {
                                expectedSingles.push_back(member);
                            }
                        }
                    }
                }

                SCOPED_TRACE(::testing::Message()
                    << "minimum=" << requestedMinimum << " mask=" << mask
                    << " policy=" << static_cast<int>(policy));
                ASSERT_EQ(actual.sequences.size(), expectedGroups.size());
                ASSERT_EQ(actual.singles.size(), expectedSingles.size());
                for (std::size_t groupIndex = 0;
                     groupIndex < expectedGroups.size(); ++groupIndex) {
                    const auto& group = actual.sequences[groupIndex];
                    const auto& expected = expectedGroups[groupIndex];
                    ASSERT_EQ(group.filenames.size(), expected.size());
                    EXPECT_EQ(group.firstFrame, expected.front());
                    EXPECT_EQ(group.lastFrame, expected.back());
                    for (std::size_t frameIndex = 0;
                         frameIndex < expected.size(); ++frameIndex) {
                        EXPECT_EQ(group.filenames[frameIndex],
                                  frameName(expected[frameIndex]));
                    }
                    std::vector<int64_t> expectedMissing;
                    if (policy == MissingFramePolicy::Preserve) {
                        for (int frame = expected.front() + 1;
                             frame < expected.back(); ++frame) {
                            if (!selected[static_cast<std::size_t>(frame)]) {
                                expectedMissing.push_back(frame);
                            }
                        }
                    }
                    EXPECT_EQ(group.missingFrames, expectedMissing);
                }
                for (std::size_t singleIndex = 0;
                     singleIndex < expectedSingles.size(); ++singleIndex) {
                    EXPECT_EQ(actual.singles[singleIndex],
                              frameName(expectedSingles[singleIndex]));
                }
            }
        }
    }
}

TEST(AssetSequenceStandaloneTest, UnparsedNamesRemainSinglesAlongsideDetectedRuns)
{
    const auto result = detectSequences(names({
        "plate_0001.exr", "plate_0002.exr", "readme", "plate_0003.exr.bak",
        "plate_9999999999999999999.exr"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    ASSERT_EQ(result.singles.size(), 3u);
    EXPECT_EQ(toStdString(result.singles[0]), "readme");
    EXPECT_EQ(toStdString(result.singles[1]), "plate_0003.exr.bak");
    EXPECT_EQ(toStdString(result.singles[2]), "plate_9999999999999999999.exr");
}

TEST(AssetSequenceStandaloneTest, DirectoryScanIgnoresSubdirectories)
{
    TemporaryDirectory directory;
    ASSERT_TRUE(directory.isValid());
    std::filesystem::create_directory(directory.path() / "nested");
    ASSERT_TRUE(writeFile(directory.path() / "render_0001.png", "frame"));
    ASSERT_TRUE(writeFile(directory.path() / "render_0002.png", "frame"));
    ASSERT_TRUE(writeFile(directory.path() / "nested" / "render_0003.png", "frame"));
    ASSERT_TRUE(writeFile(directory.path() / "notes.txt", "notes"));

    const auto result = detectSequencesInDirectory(directory.path());

    ASSERT_EQ(result.sequences.size(), 1u);
    EXPECT_EQ(result.sequences.front().firstFrame, 1);
    EXPECT_EQ(result.sequences.front().lastFrame, 2);
    ASSERT_EQ(result.sequences.front().filenames.size(), 2u);
    ASSERT_EQ(result.singles.size(), 1u);
    EXPECT_EQ(toStdString(result.singles.front()), "notes.txt");
}

TEST(AssetSequenceStandaloneTest, DirectoryScanSortsGroupsAndSinglesDeterministically)
{
    TemporaryDirectory directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(writeFile(directory.path() / "z_take_0002.exr", "frame"));
    ASSERT_TRUE(writeFile(directory.path() / "notes-z.txt", "notes"));
    ASSERT_TRUE(writeFile(directory.path() / "a_take_0002.exr", "frame"));
    ASSERT_TRUE(writeFile(directory.path() / "z_take_0001.exr", "frame"));
    ASSERT_TRUE(writeFile(directory.path() / "notes-a.txt", "notes"));
    ASSERT_TRUE(writeFile(directory.path() / "a_take_0001.exr", "frame"));

    const auto result = detectSequencesInDirectory(directory.path());

    ASSERT_EQ(result.sequences.size(), 2u);
    EXPECT_EQ(toStdString(result.sequences[0].prefix), "a_take_");
    EXPECT_EQ(toStdString(result.sequences[1].prefix), "z_take_");
    ASSERT_EQ(result.singles.size(), 2u);
    EXPECT_EQ(toStdString(result.singles[0]), "notes-a.txt");
    EXPECT_EQ(toStdString(result.singles[1]), "notes-z.txt");
}

TEST(AssetSequenceStandaloneTest, DirectoryScanForwardsMinimumRunLength)
{
    TemporaryDirectory directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(writeFile(directory.path() / "render_0001.png", "frame"));
    ASSERT_TRUE(writeFile(directory.path() / "render_0002.png", "frame"));

    const auto result = detectSequencesInDirectory(directory.path(), 3);

    EXPECT_TRUE(result.sequences.empty());
    ASSERT_EQ(result.singles.size(), 2u);
    EXPECT_EQ(toStdString(result.singles[0]), "render_0001.png");
    EXPECT_EQ(toStdString(result.singles[1]), "render_0002.png");
}

TEST(AssetSequenceStandaloneTest, EmptyDirectoryProducesNoSequencesOrSingles)
{
    TemporaryDirectory directory;
    ASSERT_TRUE(directory.isValid());

    const auto result = detectSequencesInDirectory(directory.path());

    EXPECT_TRUE(result.sequences.empty());
    EXPECT_TRUE(result.singles.empty());
}

TEST(AssetSequenceStandaloneTest, MissingDirectoryProducesEmptyResult)
{
    TemporaryDirectory directory;
    ASSERT_TRUE(directory.isValid());
    const auto missingPath = directory.path() / "does-not-exist";
    ASSERT_FALSE(std::filesystem::exists(missingPath));

    const auto result = detectSequencesInDirectory(missingPath);

    EXPECT_TRUE(result.sequences.empty());
    EXPECT_TRUE(result.singles.empty());
}
