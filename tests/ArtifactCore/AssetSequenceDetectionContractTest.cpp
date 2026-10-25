#include <gtest/gtest.h>

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
