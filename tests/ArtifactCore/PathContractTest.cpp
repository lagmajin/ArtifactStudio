#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

import Utils.Text.Path;

using namespace ArtifactCore;

TEST(PathContractTest, NormalizesRepeatedSeparatorsAndPreservesUncPrefix)
{
    const auto windows = normalizePathSeparators("C:\\\\work\\\\asset.mov");
    const auto unc = normalizePathSeparators("\\\\server\\\\share\\\\asset.mov");

    ASSERT_TRUE(windows);
    EXPECT_EQ(windows.value(), "C:/work/asset.mov");
    ASSERT_TRUE(unc);
    EXPECT_EQ(unc.value(), "//server/share/asset.mov");
}

TEST(PathContractTest, SeparatorNormalizationIsIdempotent)
{
    constexpr std::string_view paths[] = {
        "C:\\work\\asset.mov",
        "//server/share/folder/asset.mov",
        "relative\\folder/asset.mov",
    };

    for (const std::string_view path : paths) {
        const auto first = normalizePathSeparators(path);
        ASSERT_TRUE(first);
        const auto second = normalizePathSeparators(first.value());
        ASSERT_TRUE(second);
        EXPECT_EQ(second.value(), first.value()) << path;
    }
}

TEST(PathContractTest, NormalizationIsIdempotentAcrossEveryShortSeparatorPattern)
{
    constexpr std::array<char, 4> alphabet{'a', '.', '/', '\\'};
    std::size_t combinations = 1;

    for (std::size_t length = 0; length <= 6; ++length) {
        for (std::size_t encoded = 0; encoded < combinations; ++encoded) {
            std::string path(length, '\0');
            std::size_t remaining = encoded;
            for (char& character : path) {
                character = alphabet[remaining % alphabet.size()];
                remaining /= alphabet.size();
            }

            const auto first = normalizePathSeparators(path);
            ASSERT_TRUE(first) << path;
            const auto second = normalizePathSeparators(first.value());
            ASSERT_TRUE(second) << path;
            EXPECT_EQ(second.value(), first.value()) << path;
        }
        combinations *= alphabet.size();
    }
}

TEST(PathContractTest, RejectsEmbeddedNulAndKeepsInputContext)
{
    constexpr std::string_view path("bad\0path", 8);
    const auto normalized = normalizePathSeparators(path, "asset.path");

    ASSERT_FALSE(normalized);
    EXPECT_EQ(normalized.errorContext().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(normalized.errorContext().operation, "path.normalizeSeparators");
    EXPECT_EQ(normalized.errorContext().objectId, "asset.path");
}

TEST(PathContractTest, SafeRelativePredicateRejectsEmbeddedNul)
{
    constexpr std::string_view path("safe\0name", 9);
    EXPECT_FALSE(isSafeRelativePath(path));
}

TEST(PathContractTest, RejectsEmbeddedNulAtEveryInputPosition)
{
    constexpr std::string_view invalidPaths[] = {
        std::string_view("\0asset", 6),
        std::string_view("asset\0", 6),
        std::string_view("folder/\0asset", 13),
    };

    for (const auto path : invalidPaths) {
        EXPECT_FALSE(normalizePathSeparators(path)) << path.size();
    }
}

TEST(PathContractTest, RecognizesDriveAndRootedPathsAsAbsolute)
{
    EXPECT_TRUE(isAbsolutePath("C:/work/asset.mov"));
    EXPECT_TRUE(isAbsolutePath("C:\\work\\asset.mov"));
    EXPECT_TRUE(isAbsolutePath("/work/asset.mov"));
    EXPECT_TRUE(isAbsolutePath("\\\\server\\share\\asset.mov"));
    EXPECT_FALSE(isAbsolutePath("work/asset.mov"));
    EXPECT_FALSE(isAbsolutePath("C:asset.mov"));
    EXPECT_FALSE(isAbsolutePath(""));
}

TEST(PathContractTest, RecognizesEveryAsciiDriveLetterAsAbsolute)
{
    char path[] = "A:/asset";
    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        path[0] = drive;
        EXPECT_TRUE(isAbsolutePath(path)) << drive;
    }
    for (char drive = 'a'; drive <= 'z'; ++drive) {
        path[0] = drive;
        EXPECT_TRUE(isAbsolutePath(path)) << drive;
    }
}

TEST(PathContractTest, RejectsNonAlphabeticDrivePrefixAsAbsolute)
{
    EXPECT_FALSE(isAbsolutePath("1:/asset"));
    EXPECT_FALSE(isAbsolutePath("_:/asset"));
    EXPECT_FALSE(isAbsolutePath("-:\\asset"));
}

TEST(PathContractTest, DetectsTraversalByWholePathSegment)
{
    EXPECT_TRUE(hasParentTraversal("cache/../asset.mov"));
    EXPECT_TRUE(hasParentTraversal("cache\\..\\asset.mov"));
    EXPECT_FALSE(hasParentTraversal("cache/..cache/asset.mov"));
    EXPECT_FALSE(hasParentTraversal("cache/asset.mov"));

    EXPECT_TRUE(isSafeRelativePath("cache/asset.mov"));
    EXPECT_FALSE(isSafeRelativePath("/cache/asset.mov"));
    EXPECT_FALSE(isSafeRelativePath("cache/../asset.mov"));
    EXPECT_FALSE(isSafeRelativePath("cache\\..\\asset.mov"));
}

TEST(PathContractTest, DetectsLeadingTrailingAndMixedSeparatorTraversal)
{
    EXPECT_TRUE(hasParentTraversal("../asset.mov"));
    EXPECT_TRUE(hasParentTraversal("cache/.."));
    EXPECT_FALSE(hasParentTraversal("cache\\..../asset.mov"));
    EXPECT_TRUE(hasParentTraversal("cache\\..\\asset.mov"));
    EXPECT_TRUE(hasParentTraversal("cache/..\\asset.mov"));
    EXPECT_FALSE(hasParentTraversal("cache/.../asset.mov"));

    EXPECT_FALSE(isSafeRelativePath("../asset.mov"));
    EXPECT_FALSE(isSafeRelativePath("cache/.."));
    EXPECT_FALSE(isSafeRelativePath("cache/..\\asset.mov"));
}

TEST(PathContractTest, EmptyPathNormalizesSuccessfullyButIsNotSafeRelative)
{
    const auto normalized = normalizePathSeparators("");
    ASSERT_TRUE(normalized);
    EXPECT_TRUE(normalized.value().empty());
    EXPECT_FALSE(isSafeRelativePath(""));
}

TEST(PathContractTest, NormalizationCollapsesMixedRunsAndKeepsOnlyUncDoublePrefix)
{
    const auto path = normalizePathSeparators("\\\\\\server////share\\\\file");
    ASSERT_TRUE(path);
    EXPECT_EQ(path.value(), "//server/share/file");
    const auto rooted = normalizePathSeparators("/folder\\\\item");
    ASSERT_TRUE(rooted);
    EXPECT_EQ(rooted.value(), "/folder/item");
}

TEST(PathContractTest, DotSegmentsAndDottedNamesAreNotParentTraversal)
{
    EXPECT_FALSE(hasParentTraversal("./cache/asset..mov"));
    EXPECT_FALSE(hasParentTraversal("cache/.../asset.mov"));
    EXPECT_TRUE(isSafeRelativePath("./cache/asset..mov"));
    EXPECT_TRUE(isSafeRelativePath("cache/.../asset.mov"));
}

TEST(PathContractTest, SafeRelativePredicateRejectsEveryRootedSeparatorForm)
{
    EXPECT_FALSE(isSafeRelativePath("/rooted/file"));
    EXPECT_FALSE(isSafeRelativePath("\\rooted\\file"));
    EXPECT_FALSE(isSafeRelativePath("//server/share/file"));
    EXPECT_FALSE(isSafeRelativePath("D:/rooted/file"));
}

TEST(PathContractTest, RecognizesSeparatorOnlyPathsAsRooted)
{
    EXPECT_TRUE(isAbsolutePath("/"));
    EXPECT_TRUE(isAbsolutePath("\\"));
    EXPECT_TRUE(isAbsolutePath("C:/"));
    EXPECT_TRUE(isAbsolutePath("C:\\"));
    EXPECT_FALSE(isSafeRelativePath("/"));
    EXPECT_FALSE(isSafeRelativePath("\\"));
}

TEST(PathContractTest, AllowsCurrentDirectoryAndEmptyRelativeSegments)
{
    EXPECT_FALSE(hasParentTraversal("."));
    EXPECT_FALSE(hasParentTraversal("./"));
    EXPECT_TRUE(isSafeRelativePath("."));
    EXPECT_TRUE(isSafeRelativePath("./"));
    EXPECT_TRUE(isSafeRelativePath("cache//asset.mov"));
}

TEST(PathContractTest, NormalizationPreservesNonSeparatorBytes)
{
    constexpr std::string_view path("asset-\xc3\xa9\\frame-01.png", 21);
    const auto normalized = normalizePathSeparators(path);
    ASSERT_TRUE(normalized);
    EXPECT_EQ(normalized.value(), "asset-\xc3\xa9/frame-01.png");
}
