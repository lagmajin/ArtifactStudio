#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QTemporaryDir>

import Asset.Sequence;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {

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

TEST(AssetSequenceDetectionContractTest, SeparatesGroupsByPrefixSuffixAndPadding)
{
    const auto result = detectSequences(names({
        "a_01.png", "a_02.png", "a_001.png", "a_002.png",
        "a_01.exr", "a_02.exr", "b_01.png", "b_02.png"}));

    ASSERT_EQ(result.sequences.size(), 4u);
    EXPECT_EQ(result.sequences[0].padding, 1);
    EXPECT_EQ(toStdString(result.sequences[0].suffix), ".exr");
    EXPECT_EQ(toStdString(result.sequences[1].prefix), "a_");
    EXPECT_EQ(result.sequences[1].padding, 1);
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
    EXPECT_TRUE(result.singles.empty());
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
