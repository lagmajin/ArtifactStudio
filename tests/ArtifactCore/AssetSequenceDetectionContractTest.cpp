#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QStringList>
#include <QTemporaryDir>

import Asset.Sequence;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {

bool writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
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

TEST(AssetSequenceDetectionContractTest, SplitsRunsAtMissingFrameNumbers)
{
    const auto result = detectSequences(names({
        "shot_0004.png", "shot_0001.png", "shot_0002.png", "shot_0006.png",
        "notes.txt"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(toStdString(sequence.prefix), "shot_");
    EXPECT_EQ(toStdString(sequence.suffix), ".png");
    EXPECT_EQ(sequence.padding, 4);
    EXPECT_EQ(sequence.firstFrame, 1);
    EXPECT_EQ(sequence.lastFrame, 2);
    ASSERT_EQ(sequence.filenames.size(), 2u);
    EXPECT_EQ(toStdString(sequence.filenames[0]), "shot_0001.png");
    EXPECT_EQ(toStdString(sequence.filenames[1]), "shot_0002.png");
    EXPECT_EQ(toStdString(sequence.representative()), "shot_0001.png");
    EXPECT_EQ(toStdString(sequence.pathPattern()), "shot_%04lld.png");
    EXPECT_EQ(toStdString(sequence.displayName()),
              "shot_[0001-0002].png  (2 frames)");
    EXPECT_EQ(result.singles.size(), 3u);
}

TEST(AssetSequenceDetectionContractTest, EmptyInputProducesNoSequencesOrSingles)
{
    const auto result = detectSequences({});

    EXPECT_TRUE(result.sequences.empty());
    EXPECT_TRUE(result.singles.empty());
}

TEST(AssetSequenceDetectionContractTest, DuplicateFrameNumbersDoNotFormAContiguousRun)
{
    const auto result = detectSequences(names({
        "shot_0001.png", "shot_0001.png", "shot_0002.png"}));

    EXPECT_TRUE(result.sequences.empty());
    ASSERT_EQ(result.singles.size(), 3u);
    EXPECT_EQ(toStdString(result.singles[0]), "shot_0001.png");
    EXPECT_EQ(toStdString(result.singles[1]), "shot_0001.png");
    EXPECT_EQ(toStdString(result.singles[2]), "shot_0002.png");
}

TEST(AssetSequenceDetectionContractTest, DuplicateFrameNumbersAtRunEndOrMiddleDemoteTheWholeGroup)
{
    const std::vector<std::vector<String>> duplicateGroups{
        names({"shot_0001.png", "shot_0002.png", "shot_0002.png"}),
        names({"shot_0001.png", "shot_0002.png", "shot_0002.png",
               "shot_0003.png"}),
    };

    for (const auto& filenames : duplicateGroups) {
        const auto result = detectSequences(filenames);

        EXPECT_TRUE(result.sequences.empty());
        ASSERT_EQ(result.singles.size(), filenames.size());
    }
}

TEST(AssetSequenceDetectionContractTest, PreservePolicyDoesNotGroupDuplicateFrameNumbers)
{
    const auto result = detectSequences(
        names({"shot_0001.png", "shot_0001.png", "shot_0002.png"}),
        2, MissingFramePolicy::Preserve);

    EXPECT_TRUE(result.sequences.empty());
    ASSERT_EQ(result.singles.size(), 3u);
    EXPECT_EQ(toStdString(result.singles[0]), "shot_0001.png");
    EXPECT_EQ(toStdString(result.singles[1]), "shot_0001.png");
    EXPECT_EQ(toStdString(result.singles[2]), "shot_0002.png");
}

TEST(AssetSequenceDetectionContractTest, NonpositiveMinimumRunLengthStillRequiresTwoFrames)
{
    for (const int minimum : {0, -1, std::numeric_limits<int>::min()}) {
        const auto result = detectSequences(names({"single_0001.png"}), minimum);
        SCOPED_TRACE(minimum);
        EXPECT_TRUE(result.sequences.empty());
        ASSERT_EQ(result.singles.size(), 1u);
        EXPECT_EQ(toStdString(result.singles.front()), "single_0001.png");
    }
}

TEST(AssetSequenceDetectionContractTest, ReportsMultipleRunsAndDemotesShortGapRun)
{
    const auto result = detectSequences(names({
        "shot_0007.png", "shot_0002.png", "shot_0001.png", "shot_0006.png",
        "shot_0004.png", "shot_0008.png"}));

    ASSERT_EQ(result.sequences.size(), 2u);
    EXPECT_EQ(result.sequences[0].firstFrame, 1);
    EXPECT_EQ(result.sequences[0].lastFrame, 2);
    EXPECT_EQ(result.sequences[1].firstFrame, 6);
    EXPECT_EQ(result.sequences[1].lastFrame, 8);
    ASSERT_EQ(result.singles.size(), 1u);
    EXPECT_EQ(toStdString(result.singles[0]), "shot_0004.png");
}

TEST(AssetSequenceDetectionContractTest, PreservePolicyReportsAbsentFrameNumbers)
{
    const auto result = detectSequences(
        names({"plate.0010.exr", "plate.0012.exr", "plate.0011.exr"}),
        2, MissingFramePolicy::Preserve);

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(sequence.firstFrame, 10);
    EXPECT_EQ(sequence.lastFrame, 12);
    EXPECT_TRUE(sequence.missingFrames.empty());
    EXPECT_EQ(sequence.filenames.size(), 3u);

    const auto withGap = detectSequences(
        names({"plate.0010.exr", "plate.0012.exr"}),
        2, MissingFramePolicy::Preserve);
    ASSERT_EQ(withGap.sequences.size(), 1u);
    EXPECT_EQ(withGap.sequences.front().missingFrames,
              (std::vector<int64_t>{11}));
}

TEST(AssetSequenceDetectionContractTest, GroupingIsInvariantAcrossEveryInputPermutation)
{
    constexpr std::array<const char*, 7> inputNames = {
        "shot_0005.exr", "other_0009.png", "shot_0001.exr", "shot_0002.exr",
        "notes.txt", "shot_0004.exr", "other_0010.png",
    };

    for (const auto policy : {MissingFramePolicy::Split, MissingFramePolicy::Preserve}) {
        std::vector<String> baselineInput;
        baselineInput.reserve(inputNames.size());
        for (const char* filename : inputNames) {
            baselineInput.emplace_back(filename);
        }
        const auto baseline = detectSequences(baselineInput, 2, policy);

        std::array<std::size_t, inputNames.size()> permutation{};
        for (std::size_t index = 0; index < permutation.size(); ++index) {
            permutation[index] = index;
        }
        std::size_t permutationCount = 0;
        do {
            std::vector<String> shuffledInput;
            shuffledInput.reserve(inputNames.size());
            for (const std::size_t index : permutation) {
                shuffledInput.emplace_back(inputNames[index]);
            }

            const auto actual = detectSequences(shuffledInput, 2, policy);
            ASSERT_EQ(actual.sequences.size(), baseline.sequences.size());
            ASSERT_EQ(actual.singles.size(), baseline.singles.size());
            for (std::size_t sequenceIndex = 0;
                 sequenceIndex < baseline.sequences.size(); ++sequenceIndex) {
                const auto& expected = baseline.sequences[sequenceIndex];
                const auto& observed = actual.sequences[sequenceIndex];
                EXPECT_EQ(observed.prefix, expected.prefix);
                EXPECT_EQ(observed.suffix, expected.suffix);
                EXPECT_EQ(observed.padding, expected.padding);
                EXPECT_EQ(observed.firstFrame, expected.firstFrame);
                EXPECT_EQ(observed.lastFrame, expected.lastFrame);
                EXPECT_EQ(observed.filenames, expected.filenames);
                EXPECT_EQ(observed.missingFrames, expected.missingFrames);
            }
            for (std::size_t singleIndex = 0; singleIndex < baseline.singles.size(); ++singleIndex) {
                EXPECT_EQ(actual.singles[singleIndex], baseline.singles[singleIndex]);
            }
            ++permutationCount;
        } while (std::next_permutation(permutation.begin(), permutation.end()));

        EXPECT_EQ(permutationCount, 5040u);
        ASSERT_EQ(baseline.singles.size(), 1u);
        EXPECT_EQ(toStdString(baseline.singles.front()), "notes.txt");
        if (policy == MissingFramePolicy::Split) {
            ASSERT_EQ(baseline.sequences.size(), 3u);
            EXPECT_EQ(toStdString(baseline.sequences[0].prefix), "other_");
            EXPECT_EQ(baseline.sequences[0].firstFrame, 9);
            EXPECT_EQ(baseline.sequences[0].lastFrame, 10);
            EXPECT_EQ(toStdString(baseline.sequences[1].prefix), "shot_");
            EXPECT_EQ(baseline.sequences[1].firstFrame, 1);
            EXPECT_EQ(baseline.sequences[1].lastFrame, 2);
            EXPECT_EQ(baseline.sequences[2].firstFrame, 4);
            EXPECT_EQ(baseline.sequences[2].lastFrame, 5);
            EXPECT_TRUE(baseline.sequences[0].missingFrames.empty());
            EXPECT_TRUE(baseline.sequences[1].missingFrames.empty());
        } else {
            ASSERT_EQ(baseline.sequences.size(), 2u);
            EXPECT_EQ(toStdString(baseline.sequences[0].prefix), "other_");
            EXPECT_EQ(baseline.sequences[0].firstFrame, 9);
            EXPECT_EQ(baseline.sequences[0].lastFrame, 10);
            EXPECT_EQ(toStdString(baseline.sequences[1].prefix), "shot_");
            EXPECT_EQ(baseline.sequences[1].firstFrame, 1);
            EXPECT_EQ(baseline.sequences[1].lastFrame, 5);
            EXPECT_EQ(baseline.sequences[1].missingFrames,
                      (std::vector<std::int64_t>{3}));
        }
    }
}

TEST(AssetSequenceDetectionContractTest,
     GroupAndSingleOrderingIsInvariantAcrossMixedInputPermutations)
{
    constexpr std::array<const char*, 5> inputNames = {
        "shot_0003.png", "notes-b.txt", "shot_0001.png",
        "shot_0004.png", "notes-a.txt",
    };
    std::vector<String> baselineInput;
    baselineInput.reserve(inputNames.size());
    for (const char* filename : inputNames) {
        baselineInput.emplace_back(filename);
    }
    const auto baseline = detectSequences(baselineInput);
    ASSERT_EQ(baseline.sequences.size(), 1u);
    ASSERT_EQ(baseline.singles.size(), 3u);

    std::array<std::size_t, inputNames.size()> permutation{};
    for (std::size_t index = 0; index < permutation.size(); ++index) {
        permutation[index] = index;
    }
    std::size_t permutationCount = 0;
    do {
        std::vector<String> shuffledInput;
        shuffledInput.reserve(inputNames.size());
        for (const std::size_t index : permutation) {
            shuffledInput.emplace_back(inputNames[index]);
        }

        const auto actual = detectSequences(shuffledInput);
        ASSERT_EQ(actual.sequences.size(), baseline.sequences.size());
        ASSERT_EQ(actual.singles.size(), baseline.singles.size());
        EXPECT_EQ(actual.sequences.front().filenames,
                  baseline.sequences.front().filenames);
        EXPECT_EQ(actual.singles, baseline.singles);
        ++permutationCount;
    } while (std::next_permutation(permutation.begin(), permutation.end()));

    EXPECT_EQ(permutationCount, 120u);
}

TEST(AssetSequenceDetectionContractTest,
     ExhaustiveSmallFrameSubsetsMatchIndependentGapPolicyModel)
{
    struct ExpectedGroup {
        std::vector<int> frames;
        std::vector<std::int64_t> missingFrames;
    };
    const auto filenameFor = [](int frame) {
        std::string digits = std::to_string(frame);
        digits.insert(digits.begin(), 4u - digits.size(), '0');
        return String("plate_" + digits + ".exr");
    };
    constexpr int firstFrame = 100;
    constexpr int frameDomainSize = 8;
    constexpr int subsetCount = 1 << frameDomainSize;
    constexpr std::array policies = {
        MissingFramePolicy::Split,
        MissingFramePolicy::Preserve,
    };

    for (const MissingFramePolicy policy : policies) {
        for (int minimumRunLength = 2; minimumRunLength <= 4; ++minimumRunLength) {
            for (int mask = 0; mask < subsetCount; ++mask) {
                SCOPED_TRACE(testing::Message()
                             << "policy=" << static_cast<int>(policy)
                             << " min=" << minimumRunLength
                             << " mask=" << mask);
                std::vector<int> selectedFrames;
                std::vector<String> input;
                for (int offset = 0; offset < frameDomainSize; ++offset) {
                    if ((mask & (1 << offset)) == 0) continue;
                    selectedFrames.push_back(firstFrame + offset);
                    input.emplace_back(filenameFor(firstFrame + offset));
                }
                std::reverse(input.begin(), input.end());

                std::vector<ExpectedGroup> expectedGroups;
                std::vector<int> expectedSingles;
                if (policy == MissingFramePolicy::Preserve &&
                    static_cast<int>(selectedFrames.size()) >= minimumRunLength) {
                    ExpectedGroup group;
                    group.frames = selectedFrames;
                    for (int frame = selectedFrames.front();
                         frame <= selectedFrames.back(); ++frame) {
                        if ((mask & (1 << (frame - firstFrame))) == 0) {
                            group.missingFrames.push_back(frame);
                        }
                    }
                    expectedGroups.push_back(std::move(group));
                } else if (policy == MissingFramePolicy::Preserve) {
                    expectedSingles = selectedFrames;
                } else {
                    for (std::size_t begin = 0; begin < selectedFrames.size();) {
                        std::size_t end = begin + 1;
                        while (end < selectedFrames.size() &&
                               selectedFrames[end] == selectedFrames[end - 1] + 1) {
                            ++end;
                        }
                        if (end - begin >= static_cast<std::size_t>(minimumRunLength)) {
                            ExpectedGroup group;
                            group.frames.assign(selectedFrames.begin() + begin,
                                                selectedFrames.begin() + end);
                            expectedGroups.push_back(std::move(group));
                        } else {
                            expectedSingles.insert(expectedSingles.end(),
                                selectedFrames.begin() + begin,
                                selectedFrames.begin() + end);
                        }
                        begin = end;
                    }
                }

                const auto actual = detectSequences(input, minimumRunLength, policy);
                ASSERT_EQ(actual.sequences.size(), expectedGroups.size());
                ASSERT_EQ(actual.singles.size(), expectedSingles.size());
                for (std::size_t groupIndex = 0;
                     groupIndex < expectedGroups.size(); ++groupIndex) {
                    const auto& expected = expectedGroups[groupIndex];
                    const auto& sequence = actual.sequences[groupIndex];
                    ASSERT_FALSE(expected.frames.empty());
                    EXPECT_EQ(sequence.firstFrame, expected.frames.front());
                    EXPECT_EQ(sequence.lastFrame, expected.frames.back());
                    EXPECT_EQ(sequence.padding, 4);
                    EXPECT_EQ(sequence.missingFrames, expected.missingFrames);
                    ASSERT_EQ(sequence.filenames.size(), expected.frames.size());
                    for (std::size_t frameIndex = 0;
                         frameIndex < expected.frames.size(); ++frameIndex) {
                        EXPECT_EQ(toStdString(sequence.filenames[frameIndex]),
                                  toStdString(filenameFor(expected.frames[frameIndex])));
                    }
                }
                for (std::size_t singleIndex = 0;
                     singleIndex < expectedSingles.size(); ++singleIndex) {
                    EXPECT_EQ(toStdString(actual.singles[singleIndex]),
                              toStdString(filenameFor(expectedSingles[singleIndex])));
                }
            }
        }
    }
}

TEST(AssetSequenceDetectionContractTest, PreservePolicyStillEnforcesMinimumRunLength)
{
    const auto result = detectSequences(
        names({"plate.0010.exr", "plate.0011.exr"}),
        3, MissingFramePolicy::Preserve);

    EXPECT_TRUE(result.sequences.empty());
    ASSERT_EQ(result.singles.size(), 2u);
    EXPECT_EQ(toStdString(result.singles[0]), "plate.0010.exr");
    EXPECT_EQ(toStdString(result.singles[1]), "plate.0011.exr");
}

TEST(AssetSequenceDetectionContractTest, SeparatesGroupsByPrefixSuffixAndPadding)
{
    const auto result = detectSequences(names({
        "a_01.png", "a_02.png", "a_001.png", "a_002.png",
        "a_01.exr", "a_02.exr", "b_01.png", "b_02.png"}));

    ASSERT_EQ(result.sequences.size(), 4u);
    EXPECT_EQ(result.sequences[0].padding, 2);
    EXPECT_EQ(toStdString(result.sequences[0].suffix), ".exr");
    EXPECT_EQ(toStdString(result.sequences[1].prefix), "a_");
    EXPECT_EQ(result.sequences[1].padding, 2);
    EXPECT_EQ(toStdString(result.sequences[1].suffix), ".png");
    EXPECT_EQ(result.sequences[2].padding, 3);
    EXPECT_EQ(result.sequences[3].prefix, String("b_"));
}

TEST(AssetSequenceDetectionContractTest, EnforcesMinimumRunLengthAndRejectsOversizedFrameToken)
{
    const auto result = detectSequences(
        names({"single_0001.png", "pair_0001.png", "pair_0002.png",
               "huge_9999999999999999999.png"}),
        1);

    ASSERT_EQ(result.sequences.size(), 1u);
    EXPECT_EQ(toStdString(result.sequences.front().prefix), "pair_");
    ASSERT_EQ(result.singles.size(), 2u);
    EXPECT_EQ(toStdString(result.singles[0]), "single_0001.png");
    EXPECT_EQ(toStdString(result.singles[1]), "huge_9999999999999999999.png");

    const auto threeFrameMinimum = detectSequences(
        names({"pair_0001.png", "pair_0002.png", "triple_0001.png",
               "triple_0002.png", "triple_0003.png"}),
        3);
    ASSERT_EQ(threeFrameMinimum.sequences.size(), 1u);
    EXPECT_EQ(toStdString(threeFrameMinimum.sequences.front().prefix), "triple_");
    ASSERT_EQ(threeFrameMinimum.singles.size(), 2u);
}

TEST(AssetSequenceDetectionContractTest, SplitPolicyAppliesMinimumLengthToEachContiguousRun)
{
    const auto result = detectSequences(
        names({"plate_0001.exr", "plate_0002.exr", "plate_0005.exr",
               "plate_0006.exr", "plate_0007.exr"}),
        3, MissingFramePolicy::Split);

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(sequence.firstFrame, 5);
    EXPECT_EQ(sequence.lastFrame, 7);
    ASSERT_EQ(sequence.filenames.size(), 3u);
    EXPECT_EQ(toStdString(sequence.filenames.front()), "plate_0005.exr");
    EXPECT_EQ(toStdString(sequence.filenames.back()), "plate_0007.exr");
    ASSERT_EQ(result.singles.size(), 2u);
    EXPECT_EQ(toStdString(result.singles[0]), "plate_0001.exr");
    EXPECT_EQ(toStdString(result.singles[1]), "plate_0002.exr");
}

TEST(AssetSequenceDetectionContractTest, AcceptsAdjacentEighteenDigitFrameNumbers)
{
    const auto result = detectSequences(names({
        "shot_999999999999999998.exr",
        "shot_999999999999999999.exr"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(sequence.padding, 18);
    EXPECT_EQ(sequence.firstFrame, 999999999999999998LL);
    EXPECT_EQ(sequence.lastFrame, 999999999999999999LL);
    ASSERT_EQ(sequence.filenames.size(), 2u);
    EXPECT_EQ(toStdString(sequence.filenames.front()),
              "shot_999999999999999998.exr");
    EXPECT_EQ(toStdString(sequence.pathPattern()), "shot_%018lld.exr");
    EXPECT_TRUE(result.singles.empty());
}

TEST(AssetSequenceDetectionContractTest, PreservePolicyHandlesRunEndingAtInt64Max)
{
    constexpr auto maximumFrame = std::numeric_limits<std::int64_t>::max();
    const auto result = detectSequences(
        names({"edge_9223372036854775806.exr",
               "edge_9223372036854775807.exr"}),
        2, MissingFramePolicy::Preserve);

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(sequence.firstFrame, maximumFrame - 1);
    EXPECT_EQ(sequence.lastFrame, maximumFrame);
    EXPECT_TRUE(sequence.missingFrames.empty());
    ASSERT_EQ(sequence.filenames.size(), 2u);
    EXPECT_EQ(toStdString(sequence.filenames.back()),
              "edge_9223372036854775807.exr");

    const auto withTerminalGap = detectSequences(
        names({"edge_9223372036854775805.exr",
               "edge_9223372036854775807.exr"}),
        2, MissingFramePolicy::Preserve);
    ASSERT_EQ(withTerminalGap.sequences.size(), 1u);
    EXPECT_EQ(withTerminalGap.sequences.front().firstFrame, maximumFrame - 2);
    EXPECT_EQ(withTerminalGap.sequences.front().lastFrame, maximumFrame);
    EXPECT_EQ(withTerminalGap.sequences.front().missingFrames,
              (std::vector<std::int64_t>{maximumFrame - 1}));
}

TEST(AssetSequenceDetectionContractTest, SplitPolicyHandlesRunEndingAtInt64Max)
{
    constexpr auto maximumFrame = std::numeric_limits<std::int64_t>::max();
    const auto result = detectSequences(names({
        "edge_9223372036854775806.exr",
        "edge_9223372036854775807.exr"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(sequence.firstFrame, maximumFrame - 1);
    EXPECT_EQ(sequence.lastFrame, maximumFrame);
    EXPECT_TRUE(sequence.missingFrames.empty());
    ASSERT_EQ(sequence.filenames.size(), 2u);
    EXPECT_EQ(toStdString(sequence.filenames.back()),
              "edge_9223372036854775807.exr");
    EXPECT_TRUE(result.singles.empty());
}

TEST(AssetSequenceDetectionContractTest, DetectsSequenceStartingAtFrameZero)
{
    const auto result = detectSequences(names({"preview_0000.png", "preview_0001.png"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(sequence.firstFrame, 0);
    EXPECT_EQ(sequence.lastFrame, 1);
    EXPECT_EQ(toStdString(sequence.representative()), "preview_0000.png");
    EXPECT_EQ(toStdString(sequence.pathPattern()), "preview_%04lld.png");
    EXPECT_TRUE(sequence.missingFrames.empty());
}

TEST(AssetSequenceDetectionContractTest, UsesLastDigitRunBeforeCasePreservedExtension)
{
    const auto result = detectSequences(names({
        "v003_render.0001.EXR", "v003_render.0002.EXR"}));

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(toStdString(sequence.prefix), "v003_render.");
    EXPECT_EQ(toStdString(sequence.suffix), ".EXR");
    EXPECT_EQ(sequence.padding, 4);
    EXPECT_EQ(sequence.firstFrame, 1);
    EXPECT_EQ(sequence.lastFrame, 2);
    EXPECT_EQ(toStdString(sequence.pathPattern()), "v003_render.%04lld.EXR");
}

TEST(AssetSequenceDetectionContractTest, ExtensionlessFrameLikeNamesRemainSingles)
{
    const auto result = detectSequences(names({"render_0001", "render_0002"}));

    EXPECT_TRUE(result.sequences.empty());
    ASSERT_EQ(result.singles.size(), 2u);
    EXPECT_EQ(toStdString(result.singles[0]), "render_0001");
    EXPECT_EQ(toStdString(result.singles[1]), "render_0002");
}

TEST(AssetSequenceDetectionContractTest, DirectoryScanIgnoresSubdirectories)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QByteArray rootUtf8 = directory.path().toUtf8();
    const std::filesystem::path root =
        std::filesystem::u8path(rootUtf8.constData());
    ASSERT_TRUE(QDir().mkdir(directory.filePath(QStringLiteral("nested"))));
    ASSERT_TRUE(writeFile(directory.filePath(QStringLiteral("render_0001.png")),
                          QByteArrayLiteral("frame")));
    ASSERT_TRUE(writeFile(directory.filePath(QStringLiteral("render_0002.png")),
                          QByteArrayLiteral("frame")));
    ASSERT_TRUE(writeFile(directory.filePath(QStringLiteral("nested/render_0003.png")),
                          QByteArrayLiteral("frame")));
    ASSERT_TRUE(writeFile(directory.filePath(QStringLiteral("notes.txt")),
                          QByteArrayLiteral("notes")));

    const auto result = detectSequencesInDirectory(root);
    ASSERT_EQ(result.sequences.size(), 1u);
    EXPECT_EQ(result.sequences.front().firstFrame, 1);
    EXPECT_EQ(result.sequences.front().lastFrame, 2);
    EXPECT_EQ(result.sequences.front().filenames.size(), 2u);
    ASSERT_EQ(result.singles.size(), 1u);
    EXPECT_EQ(toStdString(result.singles.front()), "notes.txt");
}

TEST(AssetSequenceDetectionContractTest, DirectoryScanReturnsDeterministicNameOrder)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QStringList createdNames{
        QStringLiteral("zeta_0002.png"), QStringLiteral("note-z.txt"),
        QStringLiteral("zeta_0001.png"), QStringLiteral("alpha_0002.png"),
        QStringLiteral("note-a.txt"), QStringLiteral("alpha_0001.png")};
    for (const QString& name : createdNames) {
        ASSERT_TRUE(writeFile(directory.filePath(name), QByteArrayLiteral("frame")))
            << name.toStdString();
    }
    const QByteArray rootUtf8 = directory.path().toUtf8();
    const std::filesystem::path root =
        std::filesystem::u8path(rootUtf8.constData());

    const auto result = detectSequencesInDirectory(root);

    ASSERT_EQ(result.sequences.size(), 2u);
    EXPECT_EQ(toStdString(result.sequences[0].prefix), "alpha_");
    EXPECT_EQ(toStdString(result.sequences[1].prefix), "zeta_");
    ASSERT_EQ(result.singles.size(), 2u);
    EXPECT_EQ(toStdString(result.singles[0]), "note-a.txt");
    EXPECT_EQ(toStdString(result.singles[1]), "note-z.txt");
}

TEST(AssetSequenceDetectionContractTest, DirectoryScanPreservesUtf8SequenceNames)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString firstName = QStringLiteral("日本_0001.png");
    const QString secondName = QStringLiteral("日本_0002.png");
    ASSERT_TRUE(writeFile(directory.filePath(firstName), QByteArrayLiteral("frame-1")));
    ASSERT_TRUE(writeFile(directory.filePath(secondName), QByteArrayLiteral("frame-2")));
    const QByteArray rootUtf8 = directory.path().toUtf8();
    const std::filesystem::path root =
        std::filesystem::u8path(rootUtf8.constData());

    const auto result = detectSequencesInDirectory(root);

    ASSERT_EQ(result.sequences.size(), 1u);
    const auto& sequence = result.sequences.front();
    EXPECT_EQ(toStdString(sequence.prefix),
              QStringLiteral("日本_").toUtf8().toStdString());
    EXPECT_EQ(toStdString(sequence.filenames.front()), firstName.toUtf8().toStdString());
    EXPECT_EQ(toStdString(sequence.filenames.back()), secondName.toUtf8().toStdString());
}

TEST(AssetSequenceDetectionContractTest, MissingDirectoryProducesEmptyResult)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QByteArray rootUtf8 = directory.path().toUtf8();
    const std::filesystem::path root =
        std::filesystem::u8path(rootUtf8.constData()) / "not-created";

    const auto result = detectSequencesInDirectory(root);

    EXPECT_TRUE(result.sequences.empty());
    EXPECT_TRUE(result.singles.empty());
}
